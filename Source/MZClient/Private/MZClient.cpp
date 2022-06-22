
#include "MZClient.h"
//
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
        if (event.has_node())
        {
            nodeId = (event.node().id().c_str());
            IMZClient::Get()->SendNodeUpdate(IMZRemoteControl::Get()->GetExposedEntities());
        }
    }

    virtual void OnNodeUpdate(mz::proto::Node const& archive) override
    {
        IMZClient::Get()->OnNodeUpdateReceived(archive);
    }

    virtual void OnMenuFired(mz::app::ContextMenuRequest const& request) override
    {
    }

    void OnTextureCreated(mz::proto::Texture const& texture)
    {
        //IMZClient::Get()->OnTextureReceived(texture);
    }

    virtual void Done(grpc::Status const& Status) override
    {
        IMZClient::Get()->Disconnect();
        nodeId.clear();
        shutdown = true;
    }

    virtual void OnNodeRemoved(mz::app::NodeRemovedEvent const& action) override
    {
        IMZClient::Get()->NodeRemoved();
    }

    virtual void OnPinShowAsChanged(mz::PinShowAsChanged const& action) override
    {
        FGuid out;
        if (FGuid::Parse(action.pin_id().c_str(), out))
        {
            IMZClient::Get()->OnPinShowAsChanged(out, action.show_as());
        }
    }

    std::string nodeId;

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
    Client->nodeId.clear();
    ClearResources();
}

void FMZClient::Disconnect() {
    Client->nodeId.clear();

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


TMap<FGuid, const mz::proto::Pin*> ParsePins(mz::proto::Node const& archive)
{
    TMap<FGuid, const mz::proto::Pin*> re;
    for (auto& pin : archive.pins())
    {
        FGuid out;
        if (FGuid::Parse(pin.id().c_str(), out))
        {
            re.Add(out, &pin);
        }
    }
    return re;
}

void FMZClient::OnNodeUpdateReceived(mz::proto::Node const& archive)
{
    if (Client->nodeId.empty())
    {
        Client->nodeId = archive.id().c_str();
        SendNodeUpdate(IMZRemoteControl::Get()->GetExposedEntities());
        return;
    }

    if (Client->nodeId != archive.id().c_str())
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
            mz::proto::msg<mz::proto::Texture> tex;
            if (mz::app::ParseFromString(tex.m_Ptr, pin->data().c_str()))
            {
                MzTextureShareInfo info = {
                    .textureInfo = {
                        .width = tex->width(),
                        .height = tex->height(),
                        .format = (MzFormat)tex->format(),
                        .usage = (MzImageUsage)tex->usage(),
                    },
                    .pid = tex->pid(),
                    .memory = tex->memory(),
                    .sync = tex->sync(),
                    .offset = tex->offset(),
                };

                ResourceInfo copyInfo = {
                    .SrcEntity = *entity,
                    .ReadOnly  = pin->pin_show_as() == mz::proto::ShowAs::OUTPUT_PIN,
                };

                ID3D12Fence* fence = 0;
                mzGetD3D12Resources(&info, Dev, &copyInfo.DstResource, &fence);
                fence->Release();
                PendingCopyQueue.Remove(id);
                CopyOnTick.Add(id, copyInfo);
            }
        }
    }
}

void FMZClient::OnPinShowAsChanged(FGuid id, mz::proto::ShowAs showAs)
{
    std::unique_lock lock(CopyOnTickMutex);
    if (auto res = CopyOnTick.Find(id))
    {
        res->ReadOnly = (showAs == mz::proto::ShowAs::OUTPUT_PIN);
    }
}

void FMZClient::OnTextureReceived(FGuid id, mz::proto::Texture const& texture)
{

}

void FMZClient::QueueTextureCopy(FGuid id, const MZEntity* entity, mz::proto::Pin* pin)
{
    MzTextureInfo info = entity->GetResourceInfo();
    {
        std::unique_lock lock(PendingCopyQueueMutex);
        PendingCopyQueue.Add(id, *entity);
    }

    mz::proto::msg<mz::proto::Texture> tex;
    tex->set_width(info.width);
    tex->set_height(info.height);
    tex->set_format(info.format);
    tex->set_usage(info.usage | MZ_IMAGE_USAGE_SAMPLED);
    mz::app::SetPin(pin, tex.m_Ptr);
}


void FMZClient::StartupModule() {

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
    if (!Client || Client->nodeId.empty())
    {
        return;
    }

    mz::proto::msg<mz::app::AppEvent> event;
    mz::app::NodeUpdate* req = event->mutable_node_update();
    mz::app::SetField(req, mz::app::NodeUpdate::kNodeIdFieldNumber, Client->nodeId.c_str());
    req->set_clear(false);
    for (auto& [id, entity] : entities)
    {
        mz::proto::Pin* pin = req->add_pins_to_add();
        FString label = entity.Entity->GetLabel().ToString();
        pin->set_pin_show_as(mz::proto::ShowAs::OUTPUT_PIN);
        pin->set_pin_can_show_as(mz::proto::CanShowAs::INPUT_OUTPUT_PROPERTY);
        entity.SerializeToProto(pin);
    }

    Client->Write(event);
}

void FMZClient::SendPinAdded(MZEntity entity)
{
    mz::proto::msg<mz::app::AppEvent> event;
    mz::app::NodeUpdate* req = event->mutable_node_update();
    mz::proto::Pin* pin = req->add_pins_to_add();
    req->set_clear(false);

    FString label = entity.Entity->GetLabel().ToString();
    pin->set_pin_show_as(mz::proto::ShowAs::OUTPUT_PIN);
    pin->set_pin_can_show_as(mz::proto::CanShowAs::INPUT_OUTPUT_PROPERTY);

    mz::app::SetField(req, mz::app::NodeUpdate::kNodeIdFieldNumber, Client->nodeId.c_str());
    entity.SerializeToProto(pin);

    Client->Write(event);
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
            mz::proto::msg<mz::app::AppEvent> event;
            mz::app::SetField(event.m_Ptr, mz::app::AppEvent::kRemoveTexture, TCHAR_TO_UTF8(*guid.ToString()));
            Client->Write(event);
        }
    }

    if (!Client || Client->nodeId.empty())
    {
        return;
    }

    mz::proto::msg<mz::app::AppEvent> event;
    mz::app::NodeUpdate* req = event->mutable_node_update();

    mz::app::SetField(req, mz::app::NodeUpdate::kNodeIdFieldNumber, Client->nodeId.c_str());
    mz::app::AddRepeatedField(req, mz::app::NodeUpdate::kPinsToDeleteFieldNumber, TCHAR_TO_UTF8(*id));
    
    Client->Write(event);
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

void FMZClient::ThawTextures(TArray<FGuid> textures)
{
    std::unique_lock lock(CopyOnTickMutex);
    for (auto id : textures)
    {
        ResourceInfo res;
        if (Frozen.RemoveAndCopyValue(id, res))
        {
            CopyOnTick.Add(id, res);
        }
    }
}


bool FMZClient::Tick(float dt)
{
    InitConnection();

    if (!ResourceChanged.IsEmpty())
    {
        std::lock_guard lock(ResourceChangedMutex);
        for (auto& [id, entity] : ResourceChanged)
        {
            mz::proto::msg<mz::app::AppEvent> event;
            mz::app::SetField(event.m_Ptr, mz::app::AppEvent::kRemoveTexture, TCHAR_TO_UTF8(*id.ToString()));
            Client->Write(event);
        }
        SendNodeUpdate(ResourceChanged);
        ResourceChanged.Empty();
    }

    if (CopyOnTick.IsEmpty())
    {
        return true;
    }

    ENQUEUE_RENDER_COMMAND(FMZClient_CopyOnTick)(
        [this](FRHICommandListImmediate& RHICmdList)
        {
            std::unique_lock lock(CopyOnTickMutex);
            WaitCommands();
            TArray<D3D12_RESOURCE_BARRIER> barriers;
            for (auto& [id, pin] : CopyOnTick)
            {
                FRHITexture2D*  RHIResource = pin.SrcEntity.GetRHIResource();
                if (!RHIResource)
                {
                    continue;
                }
                FD3D12TextureBase* Base = GetD3D12TextureFromRHITexture(RHIResource);
                ID3D12Resource* SrcResource = Base->GetResource()->GetResource();
                ID3D12Resource* DstResource = pin.DstResource;

                D3D12_RESOURCE_BARRIER barrier = {
                    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                    .Transition = {
                        .pResource   = SrcResource,
                        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                        .StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE,
                    }
                };
                
                if (!pin.ReadOnly)
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

