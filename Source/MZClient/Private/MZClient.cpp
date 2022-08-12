

#include "MZClient.h"
#include "MZCamParams.h"

#include "../../MZRemoteControl/Public/IMZRemoteControl.h"
#include "ScreenRendering.h"

#define LOCTEXT_NAMESPACE "FMZClient"

#pragma optimize("", off)


class MZCLIENT_API ClientImpl : public mz::app::AppClient
{
public:
    using mz::app::AppClient::AppClient;
    
    virtual void OnAppConnected(mz::app::AppConnectedEvent const& event) override
    {
        FMessageDialog::Debugf(FText::FromString("Connected to mzEngine"), 0);

        if (flatbuffers::IsFieldPresent(&event, mz::app::AppConnectedEvent::VT_NODE))
        {
            nodeId = *(FGuid*)event.node()->id();
            IMZClient::Get()->SendNodeUpdate(IMZRemoteControl::Get()->GetExposedEntities());
        }
    }

    virtual void OnNodeUpdate(mz::NodeUpdated const& archive) override
    {
        IMZClient::Get()->OnNodeUpdateReceived(*archive.node());
    }

    virtual void OnMenuFired(mz::ContextMenuRequest const& request) override
    {
    }

    void OnTextureCreated(mz::fb::Texture const& texture)
    {
        //IMZClient::Get()->OnTextureReceived(texture);
    }

    virtual void Done(grpc::Status const& Status) override
    {
        IMZClient::Get()->Disconnect();
        nodeId = {};
        shutdown = true;
    }

    virtual void OnNodeRemoved(mz::app::NodeRemovedEvent const& action) override
    {
        IMZClient::Get()->NodeRemoved();
    }

    virtual void OnPinShowAsChanged(mz::PinShowAsChanged const& action) override
    {
        IMZClient::Get()->OnPinShowAsChanged(*(FGuid*)action.pin_id(), action.show_as());
    }

    virtual void OnPinValueChanged(mz::PinValueChanged const& action) override
    {
        auto ptr = action.value()->Data();
        auto sz = action.value()->size();

        IMZClient::Get()->OnPinValueChanged(*(FGuid*)action.pin_id(), ptr, sz);
    }

    FGuid nodeId;

    std::atomic_bool shutdown = true;
};

FMZClient::FMZClient() {}

void FMZClient::ClearResources()
{
    std::unique_lock lock(CopyOnTickMutex);
    for (auto& [_, pin] : CopyOnTick)
    {
        pin.Release();
    }
    PendingCopyQueue.Empty();
    CopyOnTick.Empty();
    ResourceChanged.Empty();
}

void FMZClient::NodeRemoved() 
{
    Client->nodeId = {};
    ClearResources();
}

void FMZClient::Disconnect() {
    Client->nodeId = {};

    std::unique_lock lock(CopyOnTickMutex);

    PendingCopyQueue.Empty();
    CopyOnTick.Empty();
    ResourceChanged.Empty();
}

void FMZClient::InitConnection()
{
    if (Client)
    {
        if (Client->shutdown)
        {
            Client->shutdown = (GRPC_CHANNEL_READY != Client->Connect());
        }
        return;
    }

    std::string protoPath = (std::filesystem::path(std::getenv("PROGRAMDATA")) / "mediaz" / "core" / "Applications" / "Unreal Engine 5").string();
    Client = new ClientImpl("UE5", "UE5", protoPath.c_str());
}

void FMZClient::InitRHI()
{
    Dev = (ID3D12Device*)GDynamicRHI->RHIGetNativeDevice();
    HRESULT re = Dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CmdAlloc));
    FD3D12DynamicRHI* D3D12RHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);
    CmdQueue = D3D12RHI->RHIGetD3DCommandQueue();
    CmdQueue->AddRef();
    Dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CmdAlloc, 0, IID_PPV_ARGS(&CmdList));
    Dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&CmdFence));
    CmdEvent = CreateEventA(0, 0, 0, 0);
    CmdList->Close();
}


TMap<FGuid, const mz::fb::Pin*> ParsePins(mz::fb::Node const& archive)
{
    TMap<FGuid, const mz::fb::Pin*> re;
    for (auto pin : *archive.pins())
    {
        re.Add(*(FGuid*)pin->id()->bytes()->Data(), pin);
    }
    return re;
}

void FMZClient::OnNodeUpdateReceived(mz::fb::Node const& archive)
{
    if (!Client->nodeId.IsValid())
    {
        Client->nodeId = *(FGuid*)archive.id()->bytes()->Data();
        SendNodeUpdate(IMZRemoteControl::Get()->GetExposedEntities());
        return;
    }

    if (Client->nodeId != *(FGuid*)archive.id()->bytes()->Data())
    {   
        // SHOULDNT BE HERE
        abort();
    }

    std::unique_lock lock1(PendingCopyQueueMutex);
    std::unique_lock lock2(CopyOnTickMutex);
    for (auto& [id, pin] : ParsePins(archive))
    {
        if (auto entity = PendingCopyQueue.Find(id))
        {
            mz::fb::Texture* tex = (mz::fb::Texture*)pin->data()->Data();
            MzTextureShareInfo info = {
                .type = tex->type(),
                .handle = tex->handle(),
                .pid = tex->pid(),
                .memory = tex->memory(),
                .offset = tex->offset(),
                .textureInfo = {
                    .width = tex->width(),
                    .height = tex->height(),
                    .format = (MzFormat)tex->format(),
                    .usage = (MzImageUsage)tex->usage(),
                },
            };

            ResourceInfo copyInfo = {
                .SrcEntity = *entity,
                .ReadOnly = pin->show_as() == mz::fb::ShowAs::INPUT_PIN,
                .Info = info,
            };

            mzGetD3D12Resources(&info, Dev, &copyInfo.DstResource);
            PendingCopyQueue.Remove(id);
            CopyOnTick.Add(id, copyInfo);
        }
    }
}

void FMZClient::OnPinShowAsChanged(FGuid id, mz::fb::ShowAs showAs)
{
    std::unique_lock lock(CopyOnTickMutex);
    if (auto res = CopyOnTick.Find(id))
    {
        res->ReadOnly = (showAs == mz::fb::ShowAs::INPUT_PIN);
    }
    MZEntity entity;
    if (IMZRemoteControl::Get()->GetExposedEntity(id, entity))
    {
        entity.Entity->SetMetadataValue("MZ_PIN_SHOW_AS_VALUE", FString::FromInt((u32)showAs));
    }
}

void FMZClient::OnPinValueChanged(FGuid id, const void* val, size_t sz)
{
    std::lock_guard lock(ValueUpdatesMutex);
    ValueUpdates.Add(id, std::vector<uint8>((uint8*)val, (uint8*)val + sz));
}

void FMZClient::OnTextureReceived(FGuid id, mz::fb::Texture const& texture)
{

}

void FMZClient::QueueTextureCopy(FGuid id, const MZEntity* entity, mz::fb::Texture* tex)
{
    MzTextureInfo info = entity->GetResourceInfo();
    {
        std::unique_lock lock(PendingCopyQueueMutex);
        PendingCopyQueue.Add(id, *entity);
    }

    tex->mutate_width(info.width);
    tex->mutate_height(info.height);
    tex->mutate_format(mz::fb::Format(info.format));
    tex->mutate_usage(mz::fb::ImageUsage(info.usage) | mz::fb::ImageUsage::SAMPLED);
    tex->mutate_type(0x00000040);
}

void FMZClient::StartupModule() {
    using namespace grpc;
    using namespace grpc::internal;
    if (grpc::g_glip == nullptr) {
        static auto* const g_gli = new GrpcLibrary();
        grpc::g_glip = g_gli;
    }
    if (grpc::g_core_codegen_interface == nullptr) {
        static auto* const g_core_codegen = new CoreCodegen();
        grpc::g_core_codegen_interface = g_core_codegen;
    }

    CustomTimeStepImpl = NewObject<UMZCustomTimeStep>();
    //GEngine->SetCustomTimeStep(CustomTimeStepImpl);
    // FMessageDialog::Debugf(FText::FromString("Loaded MZClient module"), 0);
    InitConnection();
    InitRHI();
    FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMZClient::Tick));
}

void FMZClient::ShutdownModule() 
{
}

void FMZClient::SendPinValueChanged(MZEntity entity)
{
    // SendNodeUpdate(entity);
}


void FMZClient::SendNodeUpdate(TMap<FGuid, MZEntity> const& entities)
{
    if (!Client || !Client->nodeId.IsValid())
    {
        return;
    }

    MessageBuilder mbb;
    std::vector<flatbuffers::Offset<mz::fb::Pin>> pins;

    for (auto& [id, entity] : entities)
    {
        pins.push_back(entity.SerializeToProto(mbb));
    }
    
    auto msg = MakeAppEvent(mbb, mz::CreateNodeUpdateDirect(mbb, (mz::fb::UUID*)&Client->nodeId, 0, 0, &pins));
    
    Client->Write(msg);
}

void FMZClient::SendPinAdded(MZEntity entity)
{
    if (!Client || !Client->nodeId.IsValid())
    {
        return;
    }

    MessageBuilder mbb;
    std::vector<flatbuffers::Offset<mz::fb::Pin>> pins = { entity.SerializeToProto(mbb) };
    Client->Write(MakeAppEvent(mbb, mz::CreateNodeUpdateDirect(mbb, (mz::fb::UUID*)&Client->nodeId, 0, 0, &pins)));
}

void FMZClient::SendPinRemoved(FGuid guid)
{
    FString id = guid.ToString();

    {
        std::unique_lock lock(CopyOnTickMutex);
        bool removed = false;
        ResourceInfo res = {};
        if (CopyOnTick.RemoveAndCopyValue(guid, res))
        {
            res.Release();
            removed = true;
        }
        else
        {
            removed = PendingCopyQueue.Remove(guid);
        }

        if (removed)
        {
            res.Release();
            MessageBuilder mbb;

            mz::fb::Texture tex;
            mz::app::TDestroyResource destroy;
            mz::app::TAPICalls call;

            tex.mutate_format((mz::fb::Format)res.Info.textureInfo.format);
            tex.mutate_usage((mz::fb::ImageUsage)res.Info.textureInfo.usage);
            tex.mutate_width(res.Info.textureInfo.width);
            tex.mutate_height(res.Info.textureInfo.height);
            tex.mutate_pid(res.Info.pid);
            tex.mutate_handle(res.Info.handle);
            tex.mutate_memory(res.Info.memory);
            tex.mutate_offset(res.Info.offset);
            tex.mutate_type(res.Info.type);

            destroy.res.Set(tex);
            call.call.Set(destroy);

            // Client->Write(MakeAppEvent(mbb, mz::app::CreateRemoveTexture(mbb, (mz::fb::UUID*)&guid)));
            Client->Write(MakeAppEvent(mbb, mz::app::CreateAPICalls(mbb, &call)));
        }
    }

    if (!Client || !Client->nodeId.IsValid())
    {
        return;
    }

    MessageBuilder mbb;
    std::vector<mz::fb::UUID> pins_to_delete = { *(mz::fb::UUID*)&guid };
    Client->Write(MakeAppEvent(mbb, mz::CreateNodeUpdateDirect(mbb, (mz::fb::UUID*)&Client->nodeId, 0, &pins_to_delete)));
}

void FMZClient::WaitCommands()
{
    if (CmdFence->GetCompletedValue() < CmdFenceValue)
    {
        CmdFence->SetEventOnCompletion(CmdFenceValue, CmdEvent);
        WaitForSingleObject(CmdEvent, INFINITE);
    }

    CmdAlloc->Reset();
    CmdList->Reset(CmdAlloc, 0);
}

void FMZClient::ExecCommands()
{
    CmdList->Close();
    CmdQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&CmdList);
    CmdQueue->Signal(CmdFence, ++CmdFenceValue);
}

void FMZClient::FreezeTextures(TArray<FGuid> textures)
{
    std::unique_lock lock(CopyOnTickMutex);
    ENQUEUE_RENDER_COMMAND(FMZClient_FreezeTextures)([this,textures=std::move(textures)](FRHICommandListImmediate& RHICmdList) {
        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        for (auto id : textures)
        {
            if (auto res = CopyOnTick.Find(id))
            {
                barriers.push_back(D3D12_RESOURCE_BARRIER{
                    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                    .Transition = {
                        .pResource = res->SrcEntity.GetResource(),
                        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        .StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
                    }
                    });
            }
        }

        WaitCommands();
        CmdList->ResourceBarrier(barriers.size(), barriers.data());
        ExecCommands();
        }
    );
    FlushRenderingCommands();
}


bool FMZClient::Tick(float dt)
{
    InitConnection();

    if (!ValueUpdates.IsEmpty())
    {
        TMap<FGuid, std::vector<uint8>> updates;
        {
            std::lock_guard lock(ValueUpdatesMutex);
            updates = std::move(ValueUpdates);
        }

        for (auto& [id, val] : updates)
        {
            MZEntity entity;
            if (IMZRemoteControl::Get()->GetExposedEntity(id, entity))
            {
                if (entity.IsTRT2D())
                {
                    mz::fb::Texture* tex = (mz::fb::Texture*)val.data();
                    MzTextureShareInfo info = {
                          .type = tex->type(),
                          .pid = tex->pid(),
                          .memory = tex->memory(),
                          .offset = tex->offset(),
                          .textureInfo = {
                              .width = tex->width(),
                              .height = tex->height(),
                              .format = (MzFormat)tex->format(),
                              .usage = (MzImageUsage)tex->usage(),
                          },
                    };
                    std::unique_lock xlock(CopyOnTickMutex);
                    if (auto res = CopyOnTick.Find(id))
                    {
                        if (res->Info.memory != info.memory || res->Info.offset != info.offset ||
                            res->Info.textureInfo.format != info.textureInfo.format ||
                            res->Info.textureInfo.usage != info.textureInfo.usage ||
                            res->Info.textureInfo.width != info.textureInfo.width ||
                            res->Info.textureInfo.height != info.textureInfo.height
                            )
                        {
                            WaitCommands();
                            res->DstResource->Release();
                            ExecCommands();
                            mzGetD3D12Resources(&info, Dev, &res->DstResource);
                            res->Info = info;
                        }
                    }
                }
                else
                {
                    entity.SetPropertyValue(val.data());
                }
            }
        }
    
    }

    if (!ResourceChanged.IsEmpty())
    {
        std::lock_guard lock(ResourceChangedMutex);
        MessageBuilder mbb;
        for (auto& [id, entity] : ResourceChanged)
        {
            // Client->Write(MakeAppEvent(mbb, mz::app::CreateRemoveTexture(mbb, (mz::fb::UUID*)&id)));
        }
        SendNodeUpdate(ResourceChanged);
        ResourceChanged.Empty();
    }

    if (CopyOnTick.IsEmpty())
    {
        return true;
    }

    {
        std::unique_lock lock(CopyOnTickMutex);
        MessageBuilder mbb;
        
        std::vector<flatbuffers::Offset<mz::app::AppEvent>> batched;

        for (auto& [id, pin] : CopyOnTick)
        {
            if(pin.ReadOnly)
            {
                // batched.push_back(mz::app::CreateAppEvent(mbb, mz::app::AppEventUnion::PinScheduleRequest, mz::app::CreatePinScheduleRequest(mbb, (mz::fb::UUID*)&id).Union()));
            }
        }
        if (!batched.empty())
        {
            Client->Write(MakeAppEvent(mbb, mz::app::CreateBatchAppEventDirect(mbb, &batched)));
        }
    }

    ENQUEUE_RENDER_COMMAND(FMZClient_CopyOnTick)(
        [this](FRHICommandListImmediate& RHICmdList)
        {
            std::unique_lock lock(CopyOnTickMutex);
            WaitCommands();
            TArray<D3D12_RESOURCE_BARRIER> barriers;
            for (auto& [id, pin] : CopyOnTick)
            {
                UTextureRenderTarget2D*  URT = pin.SrcEntity.GetURT();
                FRHITexture2D*  RHIResource = pin.SrcEntity.GetRHIResource();
                
                if (!RHIResource)
                {
                    continue;
                }
                
                FD3D12TextureBase* Base = GetD3D12TextureFromRHITexture(RHIResource);
                ID3D12Resource* SrcResource = Base->GetResource()->GetResource();
                ID3D12Resource* DstResource = pin.DstResource;
                D3D12_RESOURCE_DESC SrcDesc = SrcResource->GetDesc();
                D3D12_RESOURCE_DESC DstDesc = DstResource->GetDesc();

                if (pin.ReadOnly && SrcDesc != DstDesc)
                {
                    EPixelFormat format = PF_Unknown;
                    ETextureSourceFormat sourceFormat = TSF_Invalid;
                    ETextureRenderTargetFormat rtFormat = RTF_RGBA16f;
                    for (auto& fmt : GPixelFormats)
                    {
                        if (fmt.PlatformFormat == DstDesc.Format)
                        {
                            format = fmt.UnrealFormat;
                            sourceFormat = (fmt.BlockBytes == 8) ? TSF_RGBA16F : TSF_BGRA8;
                            rtFormat     = (fmt.BlockBytes == 8) ? RTF_RGBA16f : RTF_RGBA8;
                            break;
                        }
                    }
                    
                    ETextureCreateFlags Flags = ETextureCreateFlags::ShaderResource;

                    if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) Flags |= ETextureCreateFlags::Shared;
                    if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER)       Flags |= ETextureCreateFlags::Shared;
                    if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)       Flags |= ETextureCreateFlags::RenderTargetable;
                    if (DstDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)    Flags |= ETextureCreateFlags::UAV;
                    if (DstDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)      Flags ^= ETextureCreateFlags::ShaderResource;

                    FD3D12DynamicRHI* DynamicRHI = static_cast<FD3D12DynamicRHI*>(GDynamicRHI);
                    FTexture2DRHIRef Texture2DRHI = DynamicRHI->RHICreateTexture2DFromResource(format, Flags, FClearValueBinding::Black, DstResource);
                    URT->RenderTargetFormat = rtFormat;
                    URT->SizeX = DstDesc.Width;
                    URT->SizeY = DstDesc.Height;
                    URT->ClearColor = FLinearColor::Black;
                    URT->bGPUSharedFlag = 1;
                    RHIUpdateTextureReference(URT->TextureReference.TextureReferenceRHI, Texture2DRHI);
                    URT->Resource->TextureRHI = Texture2DRHI;
                    URT->Resource->SetTextureReference(URT->TextureReference.TextureReferenceRHI);
                    continue;
                }

                if (SrcResource == DstResource)
                {
                    continue;
                }

                D3D12_RESOURCE_BARRIER barrier = {
                    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                    .Transition = {
                        .pResource   = SrcResource,
                        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                        .StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE,
                    }
                };
                
                if (pin.ReadOnly)
                {
                    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
                    Swap(SrcResource, DstResource);
                }

                CmdList->ResourceBarrier(1, &barrier);
                CmdList->CopyResource(DstResource, SrcResource);
                Swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
                barriers.Add(barrier);
            }
            CmdList->ResourceBarrier(barriers.Num(), barriers.GetData());
            ExecCommands();
        });

    return true;
}


bool FMZClient::Connect() {
    return true;
}

uint32 FMZClient::Run() {
  return 0;
}
#pragma optimize("", on)
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZClient, MZClient)

//
//#include "DispelUnrealMadnessPostlude.h"

