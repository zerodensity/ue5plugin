#include "MZClient.h"
#include "MZCustomTimeStep.h"
// std
#include <cstdio>
#include <string>
#include <filesystem>

// UE
#include "HardwareInfo.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "ToolMenus.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"
#include "Engine/EngineCustomTimeStep.h"
#include "Misc/MessageDialog.h"

#include "mzFlatBuffersCommon.h"
#include "EditorActorFolders.h"

#define LOCTEXT_NAMESPACE "FMZClient"
#pragma optimize("", off)

DEFINE_LOG_CATEGORY(LogMediaZ);
#define LOG(x) UE_LOG(LogMediaZ, Warning, TEXT(x))
#define LOGF(x, y) UE_LOG(LogMediaZ, Warning, TEXT(x), y)

template<typename T>
inline const T& FinishBuffer(flatbuffers::FlatBufferBuilder& builder, flatbuffers::Offset<T> const& offset)
{
	builder.Finish(offset);
	auto buf = builder.Release();
	return *flatbuffers::GetRoot<T>(buf.data());
}

FGuid FMZClient::NodeId = {};

void* FMediaZ::LibHandle = nullptr;
PFN_MakeAppServiceClient FMediaZ::MakeAppServiceClient = nullptr;
PFN_mzGetD3D12Resources FMediaZ::GetD3D12Resources = nullptr;

bool FMediaZ::Initialize()
{
	FString SdkPath = FPlatformMisc::GetEnvironmentVariable(TEXT("MZ_SDK_DIR"));
	FString SdkBinPath = FPaths::Combine(SdkPath, TEXT("bin"));
	FPlatformProcess::PushDllDirectory(*SdkBinPath);
	FString SdkDllPath = FPaths::Combine(SdkBinPath, "mzSDK.dll");

	if (!FPaths::FileExists(SdkDllPath))
	{
		UE_LOG(LogMediaZ, Error, TEXT("Failed to find the mzSDK.dll at %s. Plugin will not be functional."), *SdkPath);
		return false;
	}

	LibHandle = FPlatformProcess::GetDllHandle(*SdkDllPath);

	if (LibHandle == nullptr)
	{
		UE_LOG(LogMediaZ, Error, TEXT("Failed to load required library %s. Plugin will not be functional."), *SdkDllPath);
		return false;
	}

	MakeAppServiceClient = (PFN_MakeAppServiceClient)FPlatformProcess::GetDllExport(LibHandle, TEXT("MakeAppServiceClient"));
	GetD3D12Resources = (PFN_mzGetD3D12Resources)FPlatformProcess::GetDllExport(LibHandle, TEXT("mzGetD3D12Resources"));
	
	if (!MakeAppServiceClient || !GetD3D12Resources)
	{
		UE_LOG(LogMediaZ, Error, TEXT("Failed to load some of the functions in MediaZ SDK. The plugin uses a different version of the SDK (%s) that what is available in your system."), *SdkDllPath)
		return false;
	}

	return true;
}

void FMediaZ::Shutdown()
{
	if (LibHandle)
	{
		FPlatformProcess::FreeDllHandle(LibHandle);
		LibHandle = nullptr;
		MakeAppServiceClient = nullptr;
		GetD3D12Resources = nullptr;
	}
}

class FMediaZPluginEditorCommands : public TCommands<FMediaZPluginEditorCommands>
{
public:
	FMediaZPluginEditorCommands()
		: TCommands<FMediaZPluginEditorCommands>
		(
			TEXT("MediaZPluginEditor"),
			NSLOCTEXT("Contexts", "MediaZPluginEditor", "MediaZPluginEditor Plugin"),
			NAME_None,
			FAppStyle::GetAppStyleSetName()
			) {}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(TestCommand, "TestCommand", "This is test command", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(PopulateRootGraph, "PopulateRootGraph", "Call PopulateRootGraph", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(SendRootUpdate, "SendRootUpdate", "Call SendNodeUpdate with Root Graph Id", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(SendAssetList, "SendAssetList", "Call SendAssetList", EUserInterfaceActionType::Button, FInputGesture());
	}

public:
	TSharedPtr<FUICommandInfo> TestCommand;
	TSharedPtr<FUICommandInfo> PopulateRootGraph;
	TSharedPtr<FUICommandInfo> SendRootUpdate;
	TSharedPtr<FUICommandInfo> SendAssetList;
};

TMap<FGuid, std::vector<uint8>> ParsePins(mz::fb::Node const& archive)
{
	TMap<FGuid, std::vector<uint8>> re;
	for (auto pin : *archive.pins())
	{
		std::vector<uint8> data(pin->data()->size(), 0);
		memcpy(data.data(), pin->data()->data(), data.size());
		re.Add(*(FGuid*)pin->id()->bytes()->Data(), data);
	}
	return re;
}

TMap<FGuid, const mz::fb::Pin*> ParsePins(const mz::fb::Node* archive)
{
	TMap<FGuid, const mz::fb::Pin*> re;
	for (auto pin : *(archive->pins()))
	{
		re.Add(*(FGuid*)pin->id()->bytes()->Data(), pin);
	}
	return re;
}

void MZEventDelegates::OnAppConnected(mz::fb::Node const& appNode)
{
	FMZClient::NodeId = *(FGuid*)appNode.id();
	if (!PluginClient)
	{
		return;
	}

	UE_LOG(LogMediaZ, Warning, TEXT("Connected to mzEngine"));
	//PluginClient->SceneTree.Root->Id = *(FGuid*)appNode.id();
	PluginClient->Connected();

	mz::fb::TNode copy;
	appNode.UnPackTo(&copy);	
	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient, copy]()
		{
			flatbuffers::FlatBufferBuilder fbb;
			auto offset = mz::fb::CreateNode(fbb, &copy);
			fbb.Finish(offset);
			auto buf = fbb.Release();
			MZClient->OnMZConnected.Broadcast(*flatbuffers::GetRoot<mz::fb::Node>(buf.data()));
		});

    
}

void MZEventDelegates::OnNodeUpdated(mz::fb::Node const& appNode)
{
	LOG("Node update from mediaz");

	if (!PluginClient)
	{
		return;
	}

	if (!FMZClient::NodeId.IsValid())
	{
		FMZClient::NodeId = *(FGuid*)appNode.id();
		//PluginClient->SceneTree.Root->Id = *(FGuid*)appNode.id();
		PluginClient->Connected();
	}

	mz::fb::TNode copy;
	appNode.UnPackTo(&copy);
	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient, copy]()
		{
			flatbuffers::FlatBufferBuilder fbb;
			auto offset = mz::fb::CreateNode(fbb, &copy);
			fbb.Finish(offset);
			auto buf = fbb.Release();
			MZClient->OnMZNodeUpdated.Broadcast(*flatbuffers::GetRoot<mz::fb::Node>(buf.data()));
		});
}

void MZEventDelegates::OnConnectionClosed()
{
	LOG("Connection with mediaz is finished.");
	FMZClient::NodeId = {};
	if (!PluginClient)
	{
		return;
	}
	PluginClient->Disconnected();
	
	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient]()
		{
			MZClient->OnMZConnectionClosed.Broadcast();
		});
}

void MZEventDelegates::OnNodeRemoved()
{
	LOG("Plugin node removed from mediaz");
	FMZClient::NodeId = {};
	if (!PluginClient)
	{
		return;
	}

	if (PluginClient->MZTimeStep)
	{
		PluginClient->MZTimeStep->Step();
	}

	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient]()
		{
			MZClient->OnMZNodeRemoved.Broadcast();
		});
}

void MZEventDelegates::OnPinValueChanged(mz::fb::UUID const& pinId, uint8_t const* data, size_t size)
{
	LOG("Pin value changed from mediaz editor");
	if (!PluginClient)
	{
		return;
	}
	
	std::vector<uint8_t> copy(size, 0);
	memcpy(copy.data(), data, size);
	FGuid id = *(FGuid*)&pinId;

	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient, copy, size, id]()
		{
			MZClient->OnMZPinValueChanged.Broadcast(*(mz::fb::UUID*)&id, copy.data(), size);
		});
}

void MZEventDelegates::OnPinShowAsChanged(mz::fb::UUID const& pinId, mz::fb::ShowAs newShowAs)
{
	LOG("Pin show as changed from mediaz");
	if (!PluginClient)
	{
		return;
	}
	

	FGuid id = *(FGuid*)&pinId;
	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient, id, newShowAs]()
		{
			MZClient->OnMZPinShowAsChanged.Broadcast(*(mz::fb::UUID*)&id, newShowAs);
		});
}

void MZEventDelegates::OnFunctionCall(mz::fb::UUID const& nodeId, mz::fb::Node const& function)
{
	LOG("Function called from mediaz");
	if (!PluginClient)
	{
		return;
	}


	mz::fb::TNode copy;
	function.UnPackTo(&copy);
	FGuid id = *(FGuid*)&nodeId;

	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient, copy, id]()
		{
			flatbuffers::FlatBufferBuilder fbb;
			auto offset = mz::fb::CreateNode(fbb, &copy);
			fbb.Finish(offset);
			auto buf = fbb.Release();
			MZClient->OnMZFunctionCalled.Broadcast(*(mz::fb::UUID*)&id, *flatbuffers::GetRoot<mz::fb::Node>(buf.data()));
		});
}

void MZEventDelegates::OnExecuteApp(mz::fb::Node const& appNode)
{
	if (!PluginClient)
	{
		return;
	}

	PluginClient->OnUpdatedNodeExecuted(ParsePins(appNode));

	mz::fb::TNode copy;
	appNode.UnPackTo(&copy);
	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient, copy]()
		{
			flatbuffers::FlatBufferBuilder fbb;
			auto offset = mz::fb::CreateNode(fbb, &copy);
			fbb.Finish(offset);
			auto buf = fbb.Release();
			MZClient->OnMZExecutedApp.Broadcast(*flatbuffers::GetRoot<mz::fb::Node>(buf.data()));
		});
}

void MZEventDelegates::OnNodeSelected(mz::fb::UUID const& nodeId)
{
	LOG("Node selected from mediaz");
	if (!PluginClient)
	{
		return;
	}
	FGuid id = *(FGuid*)&nodeId;
	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient, id]()
		{
			MZClient->OnMZNodeSelected.Broadcast(*(mz::fb::UUID*)&id);
		});
}

void MZEventDelegates::OnContextMenuRequested(mz::ContextMenuRequest const& request)
{
	LOG("Context menu fired from MediaZ");
	if (!PluginClient)
	{
		return;
	}

	
	mz::TContextMenuRequest copy;
	request.UnPackTo(&copy);
	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient, copy]()
		{
			flatbuffers::FlatBufferBuilder fbb;
			auto offset = mz::CreateContextMenuRequest(fbb, &copy);
			fbb.Finish(offset);
			auto buf = fbb.Release();
			MZClient->OnMZContextMenuRequested.Broadcast(*flatbuffers::GetRoot<mz::ContextMenuRequest>(buf.data()));
		});
}

void MZEventDelegates::OnContextMenuCommandFired(mz::ContextMenuAction const& action)
{
	LOG("Context menu command fired from MediaZ");
	if (!PluginClient)
	{
		return;
	}

	mz::TContextMenuAction copy;
	action.UnPackTo(&copy);
	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient, copy]()
		{
			flatbuffers::FlatBufferBuilder fbb;
			auto offset = mz::CreateContextMenuAction(fbb, &copy);
			fbb.Finish(offset);
			auto buf = fbb.Release();
			MZClient->OnMZContextMenuCommandFired.Broadcast(*flatbuffers::GetRoot<mz::ContextMenuAction>(buf.data()));
		});
}

void MZEventDelegates::OnNodeImported(mz::fb::Node const& appNode)
{
	LOG("Node imported from MediaZ");
	if (!PluginClient)
	{
		return;
	}

	mz::fb::TNode copy;
	appNode.UnPackTo(&copy);
	PluginClient->TaskQueue.Enqueue([MZClient = PluginClient, copy]()
		{
			flatbuffers::FlatBufferBuilder fbb;
			auto offset = mz::fb::CreateNode(fbb, &copy);
			fbb.Finish(offset);
			auto buf = fbb.Release();
			MZClient->OnMZNodeImported.Broadcast(*flatbuffers::GetRoot<mz::fb::Node>(buf.data()));
		});
}

FMZClient::FMZClient() {}

bool FMZClient::IsConnected()
{
	return AppServiceClient && AppServiceClient->IsConnected();
}

void FMZClient::Connected()
{
	TaskQueue.Enqueue([&]()
		{
			auto WorldContext = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport);
			if (WorldContext->World())
			{
				mz::fb::TNodeStatusMessage MapNameStatus;
				MapNameStatus.text = TCHAR_TO_UTF8(*WorldContext->World()->GetMapName());
				MapNameStatus.type = mz::fb::NodeStatusMessageType::INFO;
				UENodeStatusHandler.Add("map_name", MapNameStatus);
			}
		});
}

void FMZClient::Disconnected()
{
	if (MZTimeStep)
	{
		MZTimeStep->Step();
	}
}

void FMZClient::TryConnect()
{
	if (!IsWorldInitialized)
	{
		return;
	}

	if (!AppServiceClient)
	{
		AppServiceClient = TSharedPtr<mz::app::IAppServiceClient>(FMediaZ::MakeAppServiceClient("localhost:50053", "UE5", "UE5"));
		EventDelegates = TSharedPtr<MZEventDelegates>(new MZEventDelegates());
		EventDelegates->PluginClient = this;
		UENodeStatusHandler.SetClient(this);
		AppServiceClient->RegisterEventDelegates(EventDelegates.Get());
		LOG("AppClient instance is created");
	}

	if (AppServiceClient)
	{
		AppServiceClient->TryConnect();
	}

	// (Samil) TODO: This connection logic should be provided by the SDK itself. 
	// App developers should not be required implement 'always connect' behaviour.
	//if (!Client->IsChannelReady)
	//{
	//	Client->IsChannelReady = (GRPC_CHANNEL_READY == Client->Connect());
	//}

	if (!CustomTimeStepBound && IsConnected())
	{
		MZTimeStep = NewObject<UMZCustomTimeStep>();
		MZTimeStep->PluginClient = this;
		if (GEngine->SetCustomTimeStep(MZTimeStep))
		{
			CustomTimeStepBound = true;
		}
	}
	return;
}

void FMZClient::OnPostWorldInit(UWorld* World, const UWorld::InitializationValues initValues)
{
	TaskQueue.Enqueue([World, this]()
		{
			auto WorldContext = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport);
			if (World != WorldContext->World())
			{
				return;
			}

			IsWorldInitialized = true;

			mz::fb::TNodeStatusMessage MapNameStatus;
			MapNameStatus.text = TCHAR_TO_UTF8(*World->GetMapName());
			MapNameStatus.type = mz::fb::NodeStatusMessageType::INFO;
			UENodeStatusHandler.Add("map_name", MapNameStatus);
		});
}

void FMZClient::OnPreWorldFinishDestroy(UWorld* World)
{
	LOG("World is destroyed");

	if (!GEngine)
	{
		return;
	}
	auto WorldContext = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport);
	if (WorldContext->World())
	{
		mz::fb::TNodeStatusMessage MapNameStatus;
		MapNameStatus.text = TCHAR_TO_UTF8(*WorldContext->World()->GetMapName());
		MapNameStatus.type = mz::fb::NodeStatusMessageType::INFO;
		UENodeStatusHandler.Add("map_name", MapNameStatus);
	}
	if (World != WorldContext->World())
	{
		return;
	}
	
}

void FMZClient::OnActorFolderChanged(const AActor* actor, FName oldPath)
{
	//FMZClient& MzModulee = FModuleManager::LoadModuleChecked<FMZClient>("MZClient");
	//MzModulee.OnActorOuterChanged(nullptr, nullptr);

	//auto ActorID = actor->GetActorGuid();
	//if (oldPath == NAME_Reality_FolderName && ActorsSpawnedByMediaZ.Contains(ActorID))
	//{
	//	//actor->SetFolderPath(NAME_Reality_FolderName);
	//	PathUpdates.Add(ActorID, NAME_Reality_FolderName);
	//}
	//else if (actor->GetFolder().GetPath() == NAME_Reality_FolderName && !ActorsSpawnedByMediaZ.Contains(ActorID))
	//{
	//	PathUpdates.Add(ActorID, oldPath);
	//}
}

void FMZClient::OnActorOuterChanged(AActor * actor, UObject * OldOuter)
{
	UE_LOG(LogTemp, Warning, TEXT("new out actor is %s "), actor->GetOuter() ? *actor->GetOuter()->GetFName().ToString() : TEXT("null"));
}

void FMZClient::StartupModule() {


	if (!FApp::HasProjectName())
	{
		return;
	}

	auto hwinfo = FHardwareInfo::GetHardwareInfo(NAME_RHI);
	if ("D3D12" != hwinfo)
	{
		FMessageDialog::Debugf(FText::FromString("MediaZ plugin supports DirectX12 only!"), 0);
		return;
	}

	if (!FMediaZ::Initialize())
	{
		return;
	}

	//Add Delegates
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMZClient::Tick));
	FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FMZClient::OnPostWorldInit);
	FWorldDelegates::OnPreWorldFinishDestroy.AddRaw(this, &FMZClient::OnPreWorldFinishDestroy);

	//FLevelActorFolderChangedEvent
	GEngine->OnLevelActorFolderChanged().AddRaw(this, &FMZClient::OnActorFolderChanged);
	GEngine->OnLevelActorOuterChanged().AddRaw(this, &FMZClient::OnActorOuterChanged);

	//TODO remove debugactions from releaase
	if (GEditor)
	{
		FMediaZPluginEditorCommands::Register();

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedRef<FUICommandList> CommandList = LevelEditorModule.GetGlobalLevelEditorActions();

		CommandList->MapAction(
			FMediaZPluginEditorCommands::Get().TestCommand,
			FExecuteAction::CreateRaw(this, &FMZClient::TestAction));
		CommandList->MapAction(
			FMediaZPluginEditorCommands::Get().PopulateRootGraph,
			FExecuteAction::CreateLambda([=]() {
				}));
		CommandList->MapAction(
			FMediaZPluginEditorCommands::Get().SendRootUpdate,
			FExecuteAction::CreateLambda([=]() {
				}));
		CommandList->MapAction(
			FMediaZPluginEditorCommands::Get().SendAssetList,
			FExecuteAction::CreateLambda([=]() {
				}));

		UToolMenus* ToolMenus = UToolMenus::Get();
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");
		UToolMenu* MediaZMenu = Menu->AddSubMenu(
			ToolMenus->CurrentOwner(),
			NAME_None,
			TEXT("MediaZ Debug Actions"),
			LOCTEXT("DragDropMenu_MediaZ", "MediaZ"),
			LOCTEXT("DragDropMenu_MediaZ_ToolTip", "Debug actions for the MediaZ plugin")
		);

		FToolMenuSection& Section = MediaZMenu->AddSection("DebugActions", NSLOCTEXT("LevelViewportContextMenu", "DebugActions", "DebugActions Collection"));
		{
			Section.AddMenuEntry(FMediaZPluginEditorCommands::Get().TestCommand);
			Section.AddMenuEntry(FMediaZPluginEditorCommands::Get().PopulateRootGraph);
			Section.AddMenuEntry(FMediaZPluginEditorCommands::Get().SendRootUpdate);
			Section.AddMenuEntry(FMediaZPluginEditorCommands::Get().SendAssetList);
		}
	}
}

void FMZClient::ShutdownModule()
{
	FMediaZ::Shutdown();
	FMediaZPluginEditorCommands::Unregister();
}

void FMZClient::TestAction()
{
	UE_LOG(LogTemp, Warning, TEXT("It Works!!!"));
}

bool FMZClient::Tick(float dt)
{
	// Uncomment when partial node updates are broadcasted by the mediaz engine.
	//if (FPSCounter.Update(dt))
	//{
	//	UENodeStatusHandler.Add("fps", FPSCounter.GetNodeStatusMessage());
	//}

	//for (auto& [id, newPath] : PathUpdates)
	//{
	//	if(SceneTree.NodeMap.Contains(id))
	//	{
	//		auto actorNode = SceneTree.NodeMap.FindRef(id);
	//		if (actorNode->GetAsActorNode() && actorNode->GetAsActorNode()->actor)
	//		{
	//			auto actor = actorNode->GetAsActorNode()->actor;
	//			actor->SetFolderPath(newPath);

	//			//check if parents path is changed
	//			auto parent = actor->GetSceneOutlinerParent();
	//			if (!parent)
	//			{
	//				continue;
	//			}
	//			bool actorSpawnedByMediaZ = ActorsSpawnedByMediaZ.Contains(actor->GetActorGuid());
	//			bool parentSpawnedByMediaZ = ActorsSpawnedByMediaZ.Contains(parent->GetActorGuid());
	//			if (actorSpawnedByMediaZ != parentSpawnedByMediaZ)
	//			{
	//				actor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
	//			}
	//		}
	//	}
	//}
	//PathUpdates.Empty();

    TryConnect();

	while (!TaskQueue.IsEmpty()) {
		Task task;
		TaskQueue.Dequeue(task);
		task();
	}

	UENodeStatusHandler.Update();

	return true;
}

void FMZClient::OnUpdatedNodeExecuted(TMap<FGuid, std::vector<uint8>> updates)
{
	if (MZTimeStep)
	{
		MZTimeStep->Step();
	}
}

void UENodeStatusHandler::SetClient(FMZClient* _PluginClient)
{
	this->PluginClient = _PluginClient;
}

void UENodeStatusHandler::Add(std::string const& Id, mz::fb::TNodeStatusMessage const& Status)
{
	StatusMessages[Id] = Status;
	Dirty = true;
}

void UENodeStatusHandler::Remove(std::string const& Id)
{
	auto it = StatusMessages.find(Id);
	if (it != StatusMessages.end())
	{
		StatusMessages.erase(it);
		Dirty = true;
	}
}

void UENodeStatusHandler::Update()
{
	if (Dirty)
	{
		SendStatus();
	}
}

void UENodeStatusHandler::SendStatus()
{
	if (!PluginClient || !PluginClient->IsConnected())
		return;
	flatbuffers::FlatBufferBuilder Builder;
	mz::TPartialNodeUpdate UpdateRequest;
	UpdateRequest.node_id = std::make_unique<mz::fb::UUID>(*((mz::fb::UUID*)&FMZClient::NodeId));
	for (auto& [_, StatusMsg] : StatusMessages)
	{
		UpdateRequest.status_messages.push_back(std::make_unique<mz::fb::TNodeStatusMessage>(StatusMsg));
	}
	PluginClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(Builder, mz::CreatePartialNodeUpdate(Builder, &UpdateRequest)));
	Dirty = false;
}

bool FPSCounter::Update(float dt)
{
	++FrameCount;
	DeltaTimeAccum += dt;
	if (DeltaTimeAccum >= 1.f)
	{
		auto Prev = FramesPerSecond;
		FramesPerSecond = 1.f / (DeltaTimeAccum / float(FrameCount));
		FramesPerSecond = std::roundf(FramesPerSecond  * 100.f) / 100.f;
		DeltaTimeAccum = 0;
		FrameCount = 0;
		return Prev != FramesPerSecond;
	}
	return false;
}

mz::fb::TNodeStatusMessage FPSCounter::GetNodeStatusMessage() const
{
	mz::fb::TNodeStatusMessage FpsStatusMessage;

	FpsStatusMessage.text.resize(32);
	::snprintf(FpsStatusMessage.text.data(), 32, "%.2f FPS", FramesPerSecond);
	FpsStatusMessage.text.shrink_to_fit();

	FpsStatusMessage.type = mz::fb::NodeStatusMessageType::INFO;
	return FpsStatusMessage;
}

#pragma optimize("", on)
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZClient, ClientImpl)

