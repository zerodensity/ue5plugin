

#include "MZClient.h"
#include "MZCamParams.h"

#include "../../MZRemoteControl/Public/IMZRemoteControl.h"
#include "ScreenRendering.h"
#include "HardwareInfo.h"

#define LOCTEXT_NAMESPACE "FMZClient"

#pragma optimize("", off)

template <class T> requires((u32)mz::app::AppEventUnionTraits<T>::enum_value != 0)
static flatbuffers::Offset<mz::app::AppEvent> CreateAppEventOffset(flatbuffers::FlatBufferBuilder& b, flatbuffers::Offset<T> event)
{
	return mz::app::CreateAppEvent(b, mz::app::AppEventUnionTraits<T>::enum_value, event.Union());
}

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
            IMZClient::Get()->SendNodeUpdate(IMZRemoteControl::Get()->GetExposedEntities(), IMZRemoteControl::Get()->GetExposedFunctions());
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
		IMZClient::Get()->OnExecute();
    }

    virtual void OnNodeRemoved(mz::app::NodeRemovedEvent const& action) override
    {
        IMZClient::Get()->NodeRemoved();
		IMZClient::Get()->OnExecute();
    }

    virtual void OnPinShowAsChanged(mz::PinShowAsChanged const& action) override
    {
        IMZClient::Get()->OnPinShowAsChanged(*(FGuid*)action.pin_id(), action.show_as());
    }

    virtual void OnFunctionCall(mz::app::FunctionCall const& action) override
    {
        IMZClient::Get()->OnFunctionCall(*(FGuid*)action.node_id(), *(FGuid*)action.function_id());
    }

	virtual void OnExecute(mz::app::AppExecute const& aE) override
    {
		//TODO: Dogukan, crash when engine is closed

        IMZClient::Get()->OnUpdateAndExecute(*aE.node());
    }

    FGuid nodeId;

    std::atomic_bool shutdown = true;
};

TMap<FGuid, const mz::fb::Pin*> ParsePins(mz::fb::Node const& archive)
{
	TMap<FGuid, const mz::fb::Pin*> re;
	for (auto pin : *archive.pins())
	{
		re.Add(*(FGuid*)pin->id()->bytes()->Data(), pin);
	}
	return re;
}

FMZClient::FMZClient() {}

bool FMZClient::IsConnected()
{
	return Client && Client->nodeId.IsValid() && !Client->shutdown;
}

void FMZClient::OnExecute()
{
	if (CustomTimeStepImpl)
	{
		CustomTimeStepImpl->CV.notify_one();
	}
}

void FMZClient::OnUpdateAndExecute(mz::fb::Node const& node)
{
	std::lock_guard lock(ValueUpdatesMutex);
	for (auto& [id, pin] : ParsePins(node))
	{
		auto val = pin->data()->Data();
		std::vector<uint8> value((uint8*)val, (uint8*)val + pin->data()->size());
		ValueUpdates.Add(id, value);
	}

	OnExecute();
}

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
    //TODO reset the custom time step
    std::unique_lock lock(CopyOnTickMutex);

    PendingCopyQueue.Empty();
    CopyOnTick.Empty();
    ResourceChanged.Empty();
}

void FMZClient::InitConnection()
{
    if (Client)
    {
        if (!ctsBound && (Client->nodeId.IsValid()))
        {
            CustomTimeStepImpl = NewObject<UMZCustomTimeStep>();
            auto tis = GEngine->SetCustomTimeStep(CustomTimeStepImpl);
            if (tis)
            {
                ctsBound = true;
            }
        }
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
	auto hwinfo = FHardwareInfo::GetHardwareInfo(NAME_RHI);
	if ("D3D12" != hwinfo)
	{
		return;
	}

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

void FMZClient::OnNodeUpdateReceived(mz::fb::Node const& archive)
{
    if (!Client->nodeId.IsValid())
    {
        Client->nodeId = *(FGuid*)archive.id()->bytes()->Data();
        SendNodeUpdate(IMZRemoteControl::Get()->GetExposedEntities(), IMZRemoteControl::Get()->GetExposedFunctions());
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
                .SrcMzrc = *entity,
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
    MZRemoteValue* mzrv = IMZRemoteControl::Get()->GetExposedEntity(id);
    if (mzrv)
    {
		mzrv->showAs = showAs;
        mzrv->Entity->SetMetadataValue("MZ_PIN_SHOW_AS_VALUE", FString::FromInt((u32)showAs));
    }
}

void FMZClient::OnFunctionCall(FGuid nodeId, FGuid funcId)
{    
    auto mzfunc = IMZRemoteControl::Get()->GetExposedFunction(funcId);

    if (mzfunc->rFunction.FunctionArguments && mzfunc->rFunction.FunctionArguments->IsValid())
    {
        std::lock_guard lock(FunctionsMutex);
        Functions.push(mzfunc->rFunction);
    }
}

void FMZClient::OnTextureReceived(FGuid id, mz::fb::Texture const& texture)
{

}

void FMZClient::QueueTextureCopy(FGuid id, MZRemoteValue* mzrv, mz::fb::Texture* tex)
{
    MzTextureInfo info = MZValueUtils::GetResourceInfo(mzrv);
    {
        std::unique_lock lock(PendingCopyQueueMutex);
        PendingCopyQueue.Add(id, mzrv);
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
    
    //GEngine->SetCustomTimeStep(CustomTimeStepImpl);
    // FMessageDialog::Debugf(FText::FromString("Loaded MZClient module"), 0);
    InitConnection();
    InitRHI();
    FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMZClient::Tick));
}

void FMZClient::ShutdownModule() 
{

}

void FMZClient::SendPinValueChanged(MZRemoteValue* mzrv)
{
    // SendNodeUpdate(entity);
}

void FMZClient::SendCategoryUpdate(TMap<FGuid, MZRemoteValue*> const& entities, TMap<FGuid, MZFunction*> const& functions)
{
	if (!Client || !Client->nodeId.IsValid())
	{
		return;
	}

	std::vector< flatbuffers::Offset<mz::app::PinCategory>> pinCategories;
	MessageBuilder mb;
	for (auto& [id, mzrv] : entities)
	{
		if (mzrv->GetAsProp())
		{
			pinCategories.push_back(mz::app::CreatePinCategoryDirect(mb, (mz::fb::UUID*)&mzrv->id, TCHAR_TO_ANSI(*mzrv->category.ToString())));
		}
	}
	std::vector< flatbuffers::Offset<mz::app::FunctionCategory>> funcCategories;
	for (auto& [id, mzf] : functions)
	{
		funcCategories.push_back(mz::app::CreateFunctionCategoryDirect(mb, (mz::fb::UUID*)&mzf->id, TCHAR_TO_ANSI(*mzf->category.ToString())));
	}
	auto msg = MakeAppEvent(mb, mz::app::CreateNodeCategoriesUpdateDirect(mb, (mz::fb::UUID*)&Client->nodeId, &pinCategories, &funcCategories));
	Client->Write(msg);
}

void FMZClient::SendNameUpdate(TMap<FGuid, MZRemoteValue*> const& entities, TMap<FGuid, MZFunction*> const& functions)
{
	if (!Client || !Client->nodeId.IsValid())
	{
		return;
	}

	std::vector< flatbuffers::Offset<mz::app::PinName>> pinNames;
	MessageBuilder mb;
	for (auto& [id, mzrv] : entities)
	{
		if (mzrv->GetAsProp())
		{
			pinNames.push_back(mz::app::CreatePinNameDirect(mb, (mz::fb::UUID*)&mzrv->id, TCHAR_TO_ANSI(*mzrv->GetAsProp()->name.ToString())));
		}
	}
	std::vector< flatbuffers::Offset<mz::app::FunctionName>> funcNames;
	for (auto& [id, mzf] : functions)
	{

		funcNames.push_back(mz::app::CreateFunctionNameDirect(mb, (mz::fb::UUID*)&mzf->id, TCHAR_TO_ANSI(*mzf->name.ToString())));

	}
	auto msg = MakeAppEvent(mb, mz::app::CreateNodeNamesUpdateDirect(mb, (mz::fb::UUID*)&Client->nodeId, &pinNames, &funcNames));
	Client->Write(msg);
}


void FMZClient::SendNodeUpdate(TMap<FGuid, MZRemoteValue*> const& entities, TMap<FGuid, MZFunction*> const& functions)
{
    if (!Client || !Client->nodeId.IsValid())
    {
        return;
    }

    MessageBuilder mbb;
	TimecodeID = FGuid::NewGuid();

	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins = {
		mz::fb::CreatePinDirect(mbb, (mz::fb::UUID*)&TimecodeID, TCHAR_TO_ANSI(TEXT("Timecode")), TCHAR_TO_ANSI(TEXT("mz.fb.Timecode")), mz::fb::ShowAs::INPUT_PIN, mz::fb::CanShowAs::INPUT_PIN_ONLY, "UE PROPERTY", mz::fb::Visualizer::NONE),
	};

    std::vector<flatbuffers::Offset<mz::fb::Node>> nodeFunctions;
	
    for (auto& [id, mzrv] : entities)
    {
        if (mzrv->GetAsProp())
        {
            pins.push_back(mzrv->SerializeToFlatBuffer(mbb));
        }
    }
    for (auto& [id, mzf] : functions)
    {
        auto rfunc = mzf->rFunction;
        auto func = rfunc.GetFunction();
        std::vector<flatbuffers::Offset<mz::fb::Pin>> fpins;
        for (auto param : mzf->params)
        {
            fpins.push_back(param->SerializeToFlatBuffer(mbb));
        }
        flatbuffers::Offset<mz::fb::Node> node = mz::fb::CreateNodeDirect(mbb, (mz::fb::UUID*)&(mzf->id), TCHAR_TO_ANSI(*mzf->name.ToString()), "UE5.UE5", false, true, &fpins, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(mbb, mz::fb::JobType::CPU).Union(), "UE5", 0, TCHAR_TO_ANSI(*mzf->category.ToString()));
        nodeFunctions.push_back(node);
    }
    
    auto msg = MakeAppEvent(mbb, mz::CreateNodeUpdateDirect(mbb, (mz::fb::UUID*)&Client->nodeId, true, 0, &pins, 0, &nodeFunctions));
    
    Client->Write(msg);
}

void FMZClient::SendPinAdded(MZRemoteValue* mzrv)
{
    if (!Client || Client->shutdown)
    {
        return;
    }

    MessageBuilder mbb;
    std::vector<flatbuffers::Offset<mz::fb::Pin>> pins = { mzrv->SerializeToFlatBuffer(mbb) };
    Client->Write(MakeAppEvent(mbb, mz::CreateNodeUpdateDirect(mbb, (mz::fb::UUID*)&Client->nodeId, 0, 0, &pins)));
}


void FMZClient::SendFunctionAdded(MZFunction* mzFunc)
{
    if (!Client || !Client->nodeId.IsValid())
    {
        return;
    }

    MessageBuilder mbb;
    std::vector<flatbuffers::Offset<mz::fb::Node>> funcList;
    auto rfunc = mzFunc->rFunction;
    auto func = rfunc.GetFunction();
    std::vector<flatbuffers::Offset<mz::fb::Pin>> pins;
    for (auto param : mzFunc->params)
    {
        pins.push_back(param->SerializeToFlatBuffer(mbb));
    }
    //TODO send pins 
    
    flatbuffers::Offset<mz::fb::Node> node = mz::fb::CreateNodeDirect(mbb, (mz::fb::UUID*)&(mzFunc->id), TCHAR_TO_ANSI(*mzFunc->name.ToString()), "UE5.UE5", false, true, &pins, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(mbb, mz::fb::JobType::CPU).Union(), "UE5", 0, TCHAR_TO_ANSI(*mzFunc->category.ToString()));

    funcList.push_back(node);
    Client->Write(MakeAppEvent(mbb, mz::CreateNodeUpdateDirect(mbb, (mz::fb::UUID*)&Client->nodeId, 0, 0, 0, 0, &funcList)));
}

void FMZClient::SendFunctionRemoved(FGuid guid) {
    
    
    if (!Client || !Client->nodeId.IsValid())
    {
        return;
    }
    MessageBuilder mbb;
    std::vector<mz::fb::UUID> functions_to_delete = { *(mz::fb::UUID*)&guid };
    Client->Write(MakeAppEvent(mbb, mz::CreateNodeUpdateDirect(mbb, (mz::fb::UUID*)&Client->nodeId, 0, 0, 0, &functions_to_delete)));
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
            //call.call.Set(destroy);

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
                        .pResource = MZValueUtils::GetResource(res->SrcMzrc),
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

    {
        FEditorScriptExecutionGuard ScriptGuard;
        std::lock_guard lock(FunctionsMutex);
        while (!Functions.empty())
        {
            auto rfunc = Functions.front();
            Functions.pop();
            for (UObject* Object : rfunc.GetBoundObjects())
            {
                if (rfunc.FunctionArguments && rfunc.FunctionArguments->IsValid())
                {
                    Object->Modify();
                    Object->ProcessEvent(rfunc.GetFunction(), rfunc.FunctionArguments->GetStructMemory());
                }
                else
                {
                    ensureAlwaysMsgf(false, TEXT("Function arguments could not be resolved."));
                }
            }

        }
    }

    if (!ValueUpdates.IsEmpty())
    {
        TMap<FGuid, std::vector<uint8>> updates;
        {
            std::lock_guard lock(ValueUpdatesMutex);
            updates = std::move(ValueUpdates);
        }

        for (auto& [id, val] : updates)
        {
            MZRemoteValue* mzrv = IMZRemoteControl::Get()->GetExposedEntity(id);
            if (mzrv)
            {
                auto obj = (UObject*)(mzrv->GetValue(EName::ObjectProperty));
                UTextureRenderTarget2D* mzrvtrt2d = nullptr;
                if (obj)
                {
                    if (((FObjectProperty*)mzrv->GetProperty())->PropertyClass->IsChildOf<UTextureRenderTarget2D>())
                    {
                        mzrvtrt2d = (UTextureRenderTarget2D*)obj;
                    }
                }
                        
                if (mzrvtrt2d)
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
                    mzrv->SetValue(val.data());
                }
            }
        }
    
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
			MessageBuilder fbb;
			std::vector<flatbuffers::Offset<mz::app::AppEvent>> events;
            for (auto& [id, pin] : CopyOnTick)
            {
                UObject* obj = pin.SrcMzrc->GetObject();
                if (!obj) continue;
                auto prop = CastField<FObjectProperty>(pin.SrcMzrc->GetProperty());
                if (!prop) continue;
                auto URT = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));
                if (!URT) continue;
                auto rt = URT->GetRenderTargetResource();
                if (!rt) continue;
                auto RHIResource = rt->GetTexture2DRHI();

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

				tbl<mz::app::AppEvent> msg;
				if(!pin.ReadOnly)
				{
					events.push_back(CreateAppEventOffset(fbb, mz::app::CreatePinDirtied(fbb, (mz::fb::UUID*)&id)));
				}
            }
            CmdList->ResourceBarrier(barriers.Num(), barriers.GetData());
            ExecCommands();

			if (!events.empty() && IsConnected())
			{
				Client->Write(MakeAppEvent(fbb, mz::app::CreateBatchAppEventDirect(fbb, &events)));
			}
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

