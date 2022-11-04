#if WITH_EDITOR

#include "MZClient.h"

//#include "ScreenRendering.h"
//#include "HardwareInfo.h"//

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "EngineUtils.h"
#include "GenericPlatform/GenericPlatformMemory.h"

#include "MZTextureShareManager.h"

//exp
#include "ActorFactories/ActorFactory.h"
#include "ActorFactories/ActorFactoryBasicShape.h"
//end exp

#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "ToolMenus.h"
#include "EditorStyleSet.h"
#include "EditorCategoryUtils.h"


#define LOCTEXT_NAMESPACE "FMZClient"

#pragma optimize("", off)

#include "ObjectEditorUtils.h"

#include "SceneTree.h"
#include "MZActorProperties.h"
#include "MZActorFunctions.h"
#include "AppTemplates.h"

#include "Kismet2/ComponentEditorUtils.h"

enum FunctionContextMenuActions
{
	BOOKMARK
};

enum FunctionContextMenuActionsOnRoot
{
	DELETE_BOOKMARK
};


DEFINE_LOG_CATEGORY(LogMediaZ);
#define LOG(x) UE_LOG(LogMediaZ, Warning, TEXT(x))
#define LOGF(x, y) UE_LOG(LogMediaZ, Warning, TEXT(x), y)


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

 
void ClientImpl::OnAppConnected(mz::app::AppConnectedEvent const& event) 
{
    FMessageDialog::Debugf(FText::FromString("Connected to mzEngine"), 0);
		
	UE_LOG(LogMediaZ, Warning, TEXT("Connected to mzEngine"));
    if (flatbuffers::IsFieldPresent(&event, mz::app::AppConnectedEvent::VT_NODE))
    {
        nodeId = *(FGuid*)event.node()->id();
		PluginClient->sceneTree.Root->id = *(FGuid*)event.node()->id();
		PluginClient->Connected();
    }
}

void ClientImpl::OnNodeUpdate(mz::NodeUpdated const& archive) 
{
	LOG("Node update from mediaz");
	if (!nodeId.IsValid())
	{
		if (flatbuffers::IsFieldPresent(&archive, mz::NodeUpdated::VT_NODE))
		{
			nodeId = *(FGuid*)archive.node()->id();
			PluginClient->sceneTree.Root->id = *(FGuid*)archive.node()->id();
			PluginClient->Connected();
		}
	}
	auto texman = MZTextureShareManager::GetInstance();
	std::unique_lock lock1(texman->PendingCopyQueueMutex);
	for (auto& [id, pin] : ParsePins(archive.node()))
	{
		if (texman->PendingCopyQueue.Contains(id))
		{
			auto mzprop = texman->PendingCopyQueue.FindRef(id);
			texman->UpdateTexturePin(mzprop, (mz::fb::Texture*)pin->data()->Data());
		}
	}
}

void ClientImpl::OnTextureCreated(mz::app::TextureCreated const& texture) 
{
	LOG("Texture created from mediaz");

}

void ClientImpl::Done(grpc::Status const& Status) 
{
	LOG("Connection with mediaz is finished.");

	PluginClient->Disconnected();
	shutdown = true;
	nodeId = {};
}

void ClientImpl::OnNodeRemoved(mz::app::NodeRemovedEvent const& action) 
{
	LOG("Plugin node removed from mediaz");
	nodeId = {};
}

void ClientImpl::OnPinValueChanged(mz::PinValueChanged const& action) 
{
	LOG("Pin value changed from mediaz editor");
	if (PluginClient)
	{
		PluginClient->SetPropertyValue(*(FGuid*)action.pin_id(), (void*)action.value()->data(), action.value()->size());
	}
}

void ClientImpl::OnPinShowAsChanged(mz::PinShowAsChanged const& action) 
{
	LOG("Pin show as changed from mediaz");
	if (PluginClient)
	{
		PluginClient->OnPinShowAsChanged(*(FGuid*)action.pin_id(), action.show_as());
	}
}

void ClientImpl::OnFunctionCall(mz::app::FunctionCall const& action) 
{
	LOG("Function called from mediaz");
	if (PluginClient)
	{

		PluginClient->OnFunctionCall(*(FGuid*)action.function()->id(), ParsePins(*action.function()));
	}
}

void ClientImpl::OnExecute(mz::app::AppExecute const& aE) 
{
}

void ClientImpl::OnNodeSelected(mz::NodeSelected const& action) 
{
	LOG("Node selected from mediaz");
	if (PluginClient)
	{
		PluginClient->OnNodeSelected(*(FGuid*)action.node_id());
	}
}

void ClientImpl::OnMenuFired(mz::ContextMenuRequest const& request) 
{
	LOG("Context menu fired from MediaZ");
}

void ClientImpl::OnCommandFired(mz::ContextMenuAction const& action)
{
	LOG("Context menu command fired from MediaZ");
}


FMZClient::FMZClient() {}

void FMZClient::SetPropertyValue(FGuid pinId, void* newval, size_t size)
{
	if (!RegisteredProperties.Contains(pinId))
	{
		LOG("The property with given id is not found.");
		return;
	}

	MZProperty* mzprop = RegisteredProperties.FindRef(pinId);
	char* copy = new char[size];
	memcpy(copy, newval, size);

	TaskQueue.Enqueue([mzprop, copy, size]()
		{
			mzprop->SetValue(copy, size);
			delete[] copy;
		});
}


bool FMZClient::IsConnected()
{
	return Client && Client->nodeId.IsValid() && !Client->shutdown;
}

void FMZClient::Connected()
{
	TaskQueue.Enqueue([&]()
		{
			PopulateSceneTree();
			SendNodeUpdate(Client->nodeId);
			SendAssetList();
		});
}

void FMZClient::Disconnected() 
{
    Client->nodeId = {};

}

void FMZClient::InitConnection()
{
    if (Client)
    {
        //if (/*!ctsBound && */ (Client->nodeId.IsValid()))
        //{
        //    //CustomTimeStepImpl = NewObject<UMZCustomTimeStep>();
        //    //auto tis = GEngine->SetCustomTimeStep(CustomTimeStepImpl);
        //    if (tis)
        //    {
        //        ctsBound = true;
        //    }
        //}
        if (Client->shutdown)
        {
            Client->shutdown = (GRPC_CHANNEL_READY != Client->Connect());
        }
        return;
    }
	
    std::string protoPath = (std::filesystem::path(std::getenv("PROGRAMDATA")) / "mediaz" / "core" / "Applications" / "Unreal Engine 5").string();
    Client = new ClientImpl("UE5", "UE5", protoPath.c_str());
	Client->PluginClient = this;
	LOG("AppClient instance is created");
}

void FMZClient::OnPostWorldInit(UWorld* world, const UWorld::InitializationValues initValues)
{


	FOnActorSpawned::FDelegate ActorSpawnedDelegate = FOnActorSpawned::FDelegate::CreateRaw(this, &FMZClient::OnActorSpawned);
	FOnActorDestroyed::FDelegate ActorDestroyedDelegate = FOnActorDestroyed::FDelegate::CreateRaw(this, &FMZClient::OnActorDestroyed);
	world->AddOnActorSpawnedHandler(ActorSpawnedDelegate);
	world->AddOnActorDestroyedHandler(ActorDestroyedDelegate);

	PopulateSceneTree();
	if (Client)
	{
		//SendAssetList();
		SendNodeUpdate(Client->nodeId);
	}
}
bool IsActorDisplayable(const AActor* Actor);
void FMZClient::OnActorSpawned(AActor* InActor)
{
	if (IsActorDisplayable(InActor))
	{
		LOG("Actor spawned");
		LOGF("%s", *(InActor->GetFName().ToString()));
		TaskQueue.Enqueue([InActor, this]()
			{
				SendActorAdded(InActor);
			});
	}
}

void FMZClient::OnActorDestroyed(AActor* InActor)
{
	LOG("Actor destroyed");
	//LOG(*(InActor->GetFName().ToString()));
	LOGF("%s", *(InActor->GetFName().ToString()) );
	auto id = InActor->GetActorGuid();
	TaskQueue.Enqueue([id, this]()
		{
			SendActorDeleted(id);
		});
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

	InitConnection();

	//Add Delegates
	FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMZClient::Tick));
	FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FMZClient::OnPostWorldInit);
	FEditorDelegates::PostPIEStarted.AddRaw(this, &FMZClient::HandleBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FMZClient::HandleEndPIE);
	//EndPlayMapDelegate
	//actor spawn
	//actor kill
	//PopulateRootGraph();

	//ADD CUSTOM FUNCTIONS
	{
		MZCustomFunction* mzcf = new MZCustomFunction;
		mzcf->id = FGuid::NewGuid();
		FGuid actorPinId = FGuid::NewGuid();
		mzcf->params.Add(actorPinId, "Spawn Actor");
		mzcf->serialize = [funcid = mzcf->id, actorPinId](flatbuffers::FlatBufferBuilder& fbb)->flatbuffers::Offset<mz::fb::Node>
		{
			std::vector<flatbuffers::Offset<mz::fb::Pin>> spawnPins = {
				mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&actorPinId, TCHAR_TO_ANSI(TEXT("Actor List")), TCHAR_TO_ANSI(TEXT("string")), mz::fb::ShowAs::PROPERTY, mz::fb::CanShowAs::PROPERTY_ONLY, "UE PROPERTY", mz::fb::CreateVisualizerDirect(fbb, mz::fb::VisualizerType::COMBO_BOX, "UE5_ACTOR_LIST")),
			};
			return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&funcid, "Spawn Actor", "UE5.UE5", false, true, &spawnPins, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(fbb, mz::fb::JobType::CPU).Union(), "UE5", 0, "ENGINE FUNCTIONS");
		};
		mzcf->function = [mzclient = this, actorPinId](TMap<FGuid, std::vector<uint8>> properties)
		{
			FString actorName((char*)properties.FindRef(actorPinId).data());

			if (mzclient->ActorPlacementParamMap.Contains(actorName))
			{
				auto placementInfo = mzclient->ActorPlacementParamMap.FindRef(actorName);
				UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
				if (PlacementSubsystem)
				{
					PlacementSubsystem->PlaceAsset(placementInfo, FPlacementOptions());
				}
			}
			else if (mzclient->SpawnableClasses.Contains(actorName))
			{
				if (GEngine)
				{
					if (UObject* ClassToSpawn = mzclient->SpawnableClasses[actorName])
					{
						UBlueprint* GeneratedBP = Cast<UBlueprint>(ClassToSpawn);
						UClass* NativeClass = Cast<UClass>(ClassToSpawn);
						UClass* Class = GeneratedBP ? (UClass*)(GeneratedBP->GeneratedClass) : (NativeClass);
						GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(Class);
					}
				}
			}
			else
			{
				LOG("Cannot spawn actor");
			}
			//mzclient->PopulateSceneTree();
			//mzclient->SendNodeUpdate(mzclient->Client->nodeId);
		};
		CustomFunctions.Add(mzcf->id, mzcf);
	}
	//Add Camera function
	{
		MZCustomFunction* mzcf = new MZCustomFunction;
		mzcf->id = FGuid::NewGuid();
		mzcf->serialize = [funcid = mzcf->id](flatbuffers::FlatBufferBuilder& fbb)->flatbuffers::Offset<mz::fb::Node>
		{
			return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&funcid, "Spawn Reality Camera", "UE5.UE5", false, true, 0, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(fbb, mz::fb::JobType::CPU).Union(), "UE5", 0, "ENGINE FUNCTIONS");
		};
		mzcf->function = [mzclient = this](TMap<FGuid, std::vector<uint8>> properties)
		{
			FString actorName("RealityCamera");

			if (mzclient->SpawnableClasses.Contains(actorName))
			{
				if (GEngine)
				{
					if (UObject* ClassToSpawn = mzclient->SpawnableClasses[actorName])
					{
						UBlueprint* GeneratedBP = Cast<UBlueprint>(ClassToSpawn);
						AActor* realityCamera = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(GeneratedBP->GeneratedClass);
						auto videoCamera = realityCamera->GetRootComponent();
						//videoCamera->GetProperty
						//realityCamera->GetComponentsByClass();
					}
				}
			}
			else
			{
				LOG("Cannot spawn actor");
			}
		};
		CustomFunctions.Add(mzcf->id, mzcf);
	}

#if WITH_EDITOR
	
	if(GEditor)
	{
		FMediaZPluginEditorCommands::Register();

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedRef<FUICommandList> CommandList = LevelEditorModule.GetGlobalLevelEditorActions();

		CommandList->MapAction(
			FMediaZPluginEditorCommands::Get().TestCommand,
			FExecuteAction::CreateRaw(this, &FMZClient::TestAction));
		CommandList->MapAction(
			FMediaZPluginEditorCommands::Get().PopulateRootGraph,
			FExecuteAction::CreateRaw(this, &FMZClient::PopulateSceneTree));
		CommandList->MapAction(
			FMediaZPluginEditorCommands::Get().SendRootUpdate,
			FExecuteAction::CreateLambda([=](){
					SendNodeUpdate(Client->nodeId);
				}));
		CommandList->MapAction(
			FMediaZPluginEditorCommands::Get().SendAssetList,
			FExecuteAction::CreateLambda([=]() {
					SendAssetList();
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

#endif //WITH_EDITOR
}

void FMZClient::ShutdownModule()  
{

#if WITH_EDITOR
	FMediaZPluginEditorCommands::Unregister();
#endif //WITH_EDITOR

}

void FMZClient::TestAction()
{
	UE_LOG(LogTemp, Warning, TEXT("It Works!!!"));
}

bool FMZClient::Tick(float dt)
{
    InitConnection();

	while (!TaskQueue.IsEmpty()) {
		Task task;
		TaskQueue.Dequeue(task);
		task();
	}

	MZTextureShareManager::GetInstance()->EnqueueCommands(Client);

	return true;
}

bool IsActorDisplayable(const AActor* Actor)
{
	static const FName SequencerActorTag(TEXT("SequencerActor"));

	return Actor &&
#if WITH_EDITOR
		Actor->IsEditable() &&																	// Only show actors that are allowed to be selected and drawn in editor
		Actor->IsListedInSceneOutliner() &&
#endif
		(((Actor->GetWorld() && Actor->GetWorld()->IsPlayInEditor()) || !Actor->HasAnyFlags(RF_Transient)) ||
			(Actor->ActorHasTag(SequencerActorTag))) &&
		!Actor->IsTemplate() &&																	// Should never happen, but we never want CDOs displayed
		!Actor->IsA(AWorldSettings::StaticClass()) &&											// Don't show the WorldSettings actor, even though it is technically editable
		IsValidChecked(Actor);// &&																// We don't want to show actors that are about to go away
		//!Actor->IsHidden();
}

void FMZClient::PopulateSceneTree() //Runs in game thread
{
	MZTextureShareManager::GetInstance()->Reset();
	sceneTree.Clear();
	Pins.Empty();

	UWorld* World = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();

	flatbuffers::FlatBufferBuilder fbb;
	std::vector<flatbuffers::Offset<mz::fb::Node>> actorNodes;

	TArray<AActor*> ActorsInScene;
	if (World)
	{
		for (TActorIterator< AActor > ActorItr(World); ActorItr; ++ActorItr)
		{
			if (!IsActorDisplayable(*ActorItr) || ActorItr->GetParentActor())
			{
				continue;
			}
			AActor* parent = ActorItr->GetSceneOutlinerParent();

			if (parent)
			{
				if (sceneTree.childMap.Contains(parent->GetActorGuid()))
				{
					sceneTree.childMap.Find(parent->GetActorGuid())->Add(*ActorItr);
				}
				else
				{
					sceneTree.childMap.FindOrAdd(parent->GetActorGuid()).Add(*ActorItr);
				}
				continue;
			}

			ActorsInScene.Add(*ActorItr);
			ActorNode* newNode = sceneTree.AddActor(ActorItr->GetFolder().GetPath().ToString(), *ActorItr);
			if (newNode)
			{
				newNode->actor = *ActorItr;
			}
		}
	}
}

void FMZClient::SendNodeUpdate(FGuid nodeId)
{
	if (!Client || !Client->nodeId.IsValid())
	{
		return;
	}
	
	if (nodeId == sceneTree.Root->id)
	{
		MessageBuilder mb;
		std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = sceneTree.Root->SerializeChildren(mb);
		std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins;
		for (auto [_, pin] : Pins)
		{
			graphPins.push_back(pin->Serialize(mb));
		}
		std::vector<flatbuffers::Offset<mz::fb::Node>> graphFunctions;
		for (auto [_, cfunc] : CustomFunctions)
		{
			graphFunctions.push_back(cfunc->serialize(mb));
		}


		auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateRequestDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::ANY, 0, &graphPins, 0, &graphFunctions, 0, &graphNodes));
		Client->Write(msg);
		return;
	}

	auto val = sceneTree.nodeMap.Find(nodeId);
	TreeNode* treeNode = val ? *val : nullptr;
	if (!(treeNode))
	{
		return;
	}

	MessageBuilder mb;
	std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = treeNode->SerializeChildren(mb);
	std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins;
	if (treeNode->GetAsActorNode() )
	{
		graphPins = treeNode->GetAsActorNode()->SerializePins(mb);
	}
	else if (treeNode->GetAsSceneComponentNode())
	{
		graphPins = treeNode->GetAsSceneComponentNode()->SerializePins(mb);
	}
	std::vector<flatbuffers::Offset<mz::fb::Node>> graphFunctions; 
	if (treeNode->GetAsActorNode())
	{
		for (auto mzfunc : treeNode->GetAsActorNode()->Functions)
		{
			graphFunctions.push_back(mzfunc->Serialize(mb));
		}
	}
	auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateRequestDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::ANY, 0, &graphPins, 0, &graphFunctions, 0, &graphNodes));
	Client->Write(msg);

	//Send list actors on the scene 
}

void FMZClient::SendPinValueChanged(FGuid propertyId, std::vector<uint8> data)
{
	if (!Client || !Client->nodeId.IsValid())
	{
		return;
	}

	MessageBuilder mb;
	auto msg = MakeAppEvent(mb, mz::CreatePinValueChangedDirect(mb, (mz::fb::UUID*)&propertyId, &data));
	Client->Write(msg);
}

void FMZClient::SendPinUpdate() //runs in game thread
{
	if (!Client || !Client->nodeId.IsValid())
	{
		return;
	}

	auto nodeId = Client->nodeId;

	MessageBuilder mb;
	std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins;
	for (auto [_, pin] : Pins)
	{
		graphPins.push_back(pin->Serialize(mb));
	}
	auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateRequestDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::CLEAR_PINS, 0, &graphPins, 0, 0, 0, 0));
	Client->Write(msg);
	
}

void FMZClient::SendActorAdded(AActor* actor) //runs in game thread
{
	if (auto sceneParent = actor->GetSceneOutlinerParent())
	{
		if (sceneTree.nodeMap.Contains(sceneParent->GetActorGuid()))
		{
			auto parentNode = sceneTree.nodeMap.FindRef(sceneParent->GetActorGuid());
			auto newNode = sceneTree.AddActor(parentNode, actor);
			if (!Client || !Client->nodeId.IsValid())
			{
				return;
			}
			MessageBuilder mb;
			std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = { newNode->Serialize(mb) };
			auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateRequestDirect(mb, (mz::fb::UUID*)&parentNode->id, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, &graphNodes));
			Client->Write(msg);
		}
	}
	else
	{
		ActorNode* newNode = sceneTree.AddActor(actor->GetFolder().GetPath().ToString(), actor);
		if (!Client || !Client->nodeId.IsValid())
		{
			return;
		}
		MessageBuilder mb;
		std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = { newNode->Serialize(mb) };
		auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateRequestDirect(mb, (mz::fb::UUID*)&Client->nodeId, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, &graphNodes));
		Client->Write(msg);
	}
}

void FMZClient::SendActorDeleted(FGuid id) //runs in game thread
{
	if (sceneTree.nodeMap.Contains(id))
	{
		auto node = sceneTree.nodeMap.FindRef(id);
		//delete from parent
		FGuid parentId = Client->nodeId;
		if (auto parent = node->Parent)
		{
			parentId = parent->id;
			auto v = parent->Children;
			auto it = std::find(v.begin(), v.end(), node);
			if (it != v.end())
				v.erase(it);
		}
		//delete from map
		sceneTree.nodeMap.Remove(node->id);

		if (!Client || !Client->nodeId.IsValid())
		{
			return;
		}

		MessageBuilder mb;
		std::vector<mz::fb::UUID> graphNodes = { *(mz::fb::UUID*)&node->id };
		auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateRequestDirect(mb, (mz::fb::UUID*)&parentId, mz::ClearFlags::NONE, 0, 0, 0, 0, &graphNodes, 0));
		Client->Write(msg);
	}
}

void FMZClient::HandleBeginPIE(bool bIsSimulating)
{
	LOG("PLAY SESSION IS STARTED");
	TaskQueue.Enqueue([this]()
		{
			PopulateSceneTree();
			if (Client)
			{
				SendNodeUpdate(Client->nodeId);
			}
		});
}

void FMZClient::HandleEndPIE(bool bIsSimulating)
{
	LOG("PLAY SESSION IS ENDING");
	TaskQueue.Enqueue([this]()
		{
			PopulateSceneTree();
			if (Client)
			{
				SendNodeUpdate(Client->nodeId);
			}
		});
}

void FMZClient::OnNodeSelected(FGuid nodeId)
{
	//Enqueue a task that will update the selected node
	TaskQueue.Enqueue([id = nodeId, client = this]()
		{
			if (client->PopulateNode(id))
			{
				client->SendNodeUpdate(id);
			}
		});
}

void FMZClient::OnPinShowAsChanged(FGuid nodeId, mz::fb::ShowAs newShowAs)
{
	TaskQueue.Enqueue([this, nodeId, newShowAs]()
		{
			if (Pins.Contains(nodeId))
			{
				auto mzprop = Pins.FindRef(nodeId);
				mzprop->PinShowAs = newShowAs;
				SendPinUpdate();
			}
			else if(RegisteredProperties.Contains(nodeId))
			{
				
				auto mzprop = RegisteredProperties.FindRef(nodeId);
				MZProperty* newmzprop = new MZProperty(mzprop->Container, mzprop->Property);
				memcpy(newmzprop, mzprop, sizeof(MZProperty));
				newmzprop->PinShowAs = newShowAs;
				newmzprop->id = FGuid::NewGuid();
				Pins.Add(newmzprop->id, newmzprop);
				RegisteredProperties.Add(newmzprop->id, newmzprop);
				SendPinUpdate();
			}
			else
			{
				LOG("Property with given id is not found.");
			}
		});
}

void FMZClient::OnFunctionCall(FGuid funcId, TMap<FGuid, std::vector<uint8>> properties)
{
	TaskQueue.Enqueue([this, funcId, properties]()
		{
			if (CustomFunctions.Contains(funcId))
			{
				auto mzcf = CustomFunctions.FindRef(funcId);
				mzcf->function(properties);
			}
			if (RegisteredFunctions.Contains(funcId))
			{
				auto mzfunc = RegisteredFunctions.FindRef(funcId);
				uint8* Parms = (uint8*)FMemory_Alloca_Aligned(mzfunc->Function->ParmsSize, mzfunc->Function->GetMinAlignment());
				mzfunc->Parameters = Parms;
				FMemory::Memzero(Parms, mzfunc->Function->ParmsSize);

				for (TFieldIterator<FProperty> It(mzfunc->Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
				{
					FProperty* LocalProp = *It;
					checkSlow(LocalProp);
					if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
					{
						LocalProp->InitializeValue_InContainer(Parms);
					}
				}

				for (auto [id, val] : properties)
				{
					if (RegisteredProperties.Contains(id))
					{
						auto mzprop = RegisteredProperties.FindRef(id);
						mzprop->SetValue((void*)val.data(), val.size(),Parms);
					}
				}

				mzfunc->Invoke();

				for (auto mzprop : mzfunc->OutProperties)
				{
					SendPinValueChanged(mzprop->id, mzprop->GetValue(Parms));
				}
				//for (TFieldIterator<FProperty> It(mzfunc->Function); It && It->HasAnyPropertyFlags(CPF_OutParm); ++It)
				//{
				//	SendPinValueChanged(It->)
				//}

				mzfunc->Parameters = nullptr;


			}
		});
}

void FMZClient::OnContexMenuFired(FGuid itemId)
{
}

void FMZClient::OnContexMenuActionFired(FGuid itemId, uint32 actionId)
{
}

bool PropertyVisible(FProperty* ueproperty)
{
	return !ueproperty->HasAllPropertyFlags(CPF_DisableEditOnInstance) &&
		!ueproperty->HasAllPropertyFlags(CPF_Deprecated) &&
		//!ueproperty->HasAllPropertyFlags(CPF_EditorOnly) && //? dont know what this flag does but it hides more than necessary
		ueproperty->HasAllPropertyFlags(CPF_Edit) &&
		//ueproperty->HasAllPropertyFlags(CPF_BlueprintVisible) && //? dont know what this flag does but it hides more than necessary
		ueproperty->HasAllFlags(RF_Public);
}

bool FMZClient::PopulateNode(FGuid nodeId)
{
	auto val = sceneTree.nodeMap.Find(nodeId);
	TreeNode* treeNode = val ? *val : nullptr;

	if (!treeNode || !treeNode->needsReload)
	{
		return false;
	}
	if (treeNode->GetAsActorNode())
	{
		ActorNode* actorNode = (ActorNode*)treeNode;
		auto ActorClass = actorNode->actor->GetClass();

		//ITERATE PROPERTIES BEGIN
		class FProperty* AProperty = ActorClass->PropertyLink;

		while (AProperty != nullptr)
		{
#if WITH_EDITOR
			FName CategoryName = FObjectEditorUtils::GetCategoryFName(AProperty);
#else
			FName CategoryName = "Default";
#endif

			UClass* Class = ActorClass;

#if WITH_EDITOR
			if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryName.ToString()) || !PropertyVisible(AProperty))
			{
				AProperty = AProperty->PropertyLinkNext;
				continue;
			}
#endif
			MZProperty* mzprop = new MZProperty(actorNode->actor, AProperty);
			RegisteredProperties.Add(mzprop->id, mzprop);
			actorNode->Properties.push_back(mzprop);
			AProperty = AProperty->PropertyLinkNext;
		}

		auto Components = actorNode->actor->GetComponents();
		for (auto Component : Components)
		{
			continue;

			auto ComponentClass = Component->GetClass();

			//if (Component->IsEditorOnly())
			//{
			//	continue;
			//}

			for (FProperty* Property = ComponentClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
			{
#if WITH_EDITOR
				FName CategoryName = FObjectEditorUtils::GetCategoryFName(Property);
#else
				FName CategoryName = "Default";
#endif

				UClass* Class = ActorClass;
#if WITH_EDITOR				
				if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryName.ToString()) || !PropertyVisible(Property))
				{
					continue;
				}
#endif				
				MZProperty* mzprop = new MZProperty(Component, Property);
				RegisteredProperties.Add(mzprop->id, mzprop);
				actorNode->Properties.push_back(mzprop);
			}
		}
		//ITERATE PROPERTIES END

		//ITERATE FUNCTIONS BEGIN
		auto ActorComponent = actorNode->actor->GetRootComponent();
		for (TFieldIterator<UFunction> FuncIt(ActorClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* UEFunction = *FuncIt;
			if (UEFunction->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_Public) &&
				!UEFunction->HasAllFunctionFlags(FUNC_Event))
			{
				auto UEFunctionName = UEFunction->GetFName().ToString();

				if (UEFunctionName.StartsWith("OnChanged_") || UEFunctionName.StartsWith("OnLengthChanged_"))
				{
					continue; // do not export user's changed handler functions
				}

				//auto OwnerClass = UEFunction->GetOwnerClass();
				//if (!OwnerClass || !Cast<UBlueprint>(OwnerClass->ClassGeneratedBy))
				//{
				//	//continue; // export only BP functions //? what we will show in mediaz
				//}
				
				MZFunction* mzfunc = new MZFunction(actorNode->actor, UEFunction);
				
				// Parse all function parameters.

				for (TFieldIterator<FProperty> PropIt(UEFunction); PropIt && PropIt->HasAnyPropertyFlags(CPF_Parm); ++PropIt)
				{
					MZProperty* mzprop = new MZProperty(nullptr, *PropIt);
					mzfunc->Properties.push_back(mzprop);
					RegisteredProperties.Add(mzprop->id, mzprop);			
					if (PropIt->HasAnyPropertyFlags(CPF_OutParm))
					{
						mzfunc->OutProperties.push_back(mzprop);
					}
				}
				
				actorNode->Functions.push_back(mzfunc);
				RegisteredFunctions.Add(mzfunc->id, mzfunc);
			}
		}

		 
		//ITERATE FUNCTIONS END
		

		//ITERATE CHILD COMPONENTS TO SHOW BEGIN
		actorNode->Children.clear();

		auto unattachedChildsPtr = sceneTree.childMap.Find(actorNode->id);
		TSet<AActor*> unattachedChilds = unattachedChildsPtr ? *unattachedChildsPtr : TSet<AActor*>();
		for (auto child : unattachedChilds)
		{
			sceneTree.AddActor(actorNode, child);
		}

		AActor* ActorContext = actorNode->actor;
		TSet<UActorComponent*> ComponentsToAdd(ActorContext->GetComponents());

		const bool bHideConstructionScriptComponentsInDetailsView = false; //GetDefault<UBlueprintEditorSettings>()->bHideConstructionScriptComponentsInDetailsView;
		auto ShouldAddInstancedActorComponent = [bHideConstructionScriptComponentsInDetailsView](UActorComponent* ActorComp, USceneComponent* ParentSceneComp)
		{
			// Exclude nested DSOs attached to BP-constructed instances, which are not mutable.
			return (ActorComp != nullptr
#if WITH_EDITOR
				&& (!ActorComp->IsVisualizationComponent())
#endif
				&& (ActorComp->CreationMethod != EComponentCreationMethod::UserConstructionScript || !bHideConstructionScriptComponentsInDetailsView)
				&& (ParentSceneComp == nullptr || !ParentSceneComp->IsCreatedByConstructionScript() || !ActorComp->HasAnyFlags(RF_DefaultSubObject)))
				&& (ActorComp->CreationMethod != EComponentCreationMethod::Native || FComponentEditorUtils::GetPropertyForEditableNativeComponent(ActorComp));
		};

		// Filter the components by their visibility
		for (TSet<UActorComponent*>::TIterator It(ComponentsToAdd.CreateIterator()); It; ++It)
		{
			UActorComponent* ActorComp = *It;
			USceneComponent* SceneComp = Cast<USceneComponent>(ActorComp);
			USceneComponent* ParentSceneComp = SceneComp != nullptr ? SceneComp->GetAttachParent() : nullptr;
			if (!ShouldAddInstancedActorComponent(ActorComp, ParentSceneComp))
			{
				It.RemoveCurrent();
			}
		}

		TArray<SceneComponentNode*> OutArray;


		TFunction<void(USceneComponent*, TreeNode*)> AddInstancedComponentsRecursive = [&, this](USceneComponent* Component, TreeNode* ParentHandle)
		{
			if (Component != nullptr)
			{
				for (USceneComponent* ChildComponent : Component->GetAttachChildren())
				{
					if (ComponentsToAdd.Contains(ChildComponent) && ChildComponent->GetOwner() == Component->GetOwner())
					{
						ComponentsToAdd.Remove(ChildComponent);
						SceneComponentNode* NewParentHandle = nullptr;
						if (ParentHandle->GetAsActorNode())
						{
							NewParentHandle = this->sceneTree.AddSceneComponent(ParentHandle->GetAsActorNode(), ChildComponent);
						}
						else if (ParentHandle->GetAsSceneComponentNode())
						{
							NewParentHandle = this->sceneTree.AddSceneComponent(ParentHandle->GetAsSceneComponentNode(), ChildComponent);
						}


						if (!NewParentHandle)
						{
							UE_LOG(LogMediaZ, Error, TEXT("A Child node other than actor or component is present!"));
							continue;
						}
						NewParentHandle->Children.clear();
						OutArray.Add(NewParentHandle);

						AddInstancedComponentsRecursive(ChildComponent, NewParentHandle);
					}
				}
			}
		};

		USceneComponent* RootComponent = ActorContext->GetRootComponent();

		// Add the root component first
		if (RootComponent != nullptr)
		{
			// We want this to be first every time, so remove it from the set of components that will be added later
			ComponentsToAdd.Remove(RootComponent);

			// Add the root component first
			SceneComponentNode* RootHandle = sceneTree.AddSceneComponent(actorNode, RootComponent);
			// Clear the loading child
			RootHandle->Children.clear();
			
			OutArray.Add(RootHandle);

			// Recursively add
			AddInstancedComponentsRecursive(RootComponent, RootHandle);
		}

		// Sort components by type (always put scene components first in the tree)
		ComponentsToAdd.Sort([](const UActorComponent& A, const UActorComponent& /* B */)
			{
				return A.IsA<USceneComponent>();
			});

		// Now add any remaining instanced owned components not already added above. This will first add any
		// unattached scene components followed by any instanced non-scene components owned by the Actor instance.
		for (UActorComponent* ActorComp : ComponentsToAdd)
		{
			// Create new subobject data with the original data as their parent.
			//OutArray.Add(sceneTree.AddSceneComponent(componentNode, ActorComp)); //TODO scene tree add actor components
		}
		//ITERATE CHILD COMPONENTS TO SHOW END

		treeNode->needsReload = false;
		return true;
	}
	else if (treeNode->GetAsSceneComponentNode())
	{
		auto Component = treeNode->GetAsSceneComponentNode()->sceneComponent;
		auto Actor = Component->GetOwner();

		auto ComponentClass = Component->GetClass();

		//if (Component->IsEditorOnly())
		//{
		//	continue;
		//}

		for (FProperty* Property = ComponentClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{

			FName CategoryName = FObjectEditorUtils::GetCategoryFName(Property);
			UClass* Class = Actor->StaticClass();
			
			if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryName.ToString()) || !PropertyVisible(Property))
			{
				continue;
			}
				
			MZProperty* mzprop = new MZProperty(Component, Property);
			RegisteredProperties.Add(mzprop->id, mzprop);
			treeNode->GetAsSceneComponentNode()->Properties.push_back(mzprop);
		}
		
		treeNode->needsReload = false;
		return true;
	}

	
	return false;
}

void FMZClient::SendAssetList()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	//AssetRegistryModule.Get().WaitForCompletion(); // wait in startup to completion of the scan

	FName BaseClassName = AActor::StaticClass()->GetFName();
	TSet< FName > DerivedNames;
	{
		TArray< FName > BaseNames;
		BaseNames.Add(BaseClassName);

		TSet< FName > Excluded;
		AssetRegistryModule.Get().GetDerivedClassNames(BaseNames, Excluded, DerivedNames);
	}

	for (auto& className : DerivedNames)
	{
		FString nameString(className.ToString());
		if (nameString.StartsWith(TEXT("SKEL_")))
		{
			continue;
		}
		nameString.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);
		SpawnableClasses.Add(nameString);
	}

	UWorld* currentWorld = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();
	ULevel* currentLevel = currentWorld->GetCurrentLevel();
	std::vector<std::pair<FString, FString>> BasicShapes =
	{
		{"Cube",UActorFactoryBasicShape::BasicCube.ToString()},
		{"Sphere",UActorFactoryBasicShape::BasicSphere.ToString() },
		{"Cylinder",UActorFactoryBasicShape::BasicCylinder.ToString()},
		{"Cone",UActorFactoryBasicShape::BasicCone.ToString()},
		{"Plane",UActorFactoryBasicShape::BasicPlane.ToString()}
	};

	for (auto [name, assetPath] : BasicShapes)
	{
		SpawnableClasses.Add(name);
		FAssetPlacementInfo PlacementInfo;
		PlacementInfo.AssetToPlace = FAssetData(LoadObject<UStaticMesh>(nullptr, *assetPath));
		PlacementInfo.FactoryOverride = UActorFactoryBasicShape::StaticClass();
		PlacementInfo.PreferredLevel = currentLevel;
		ActorPlacementParamMap.Add(name, PlacementInfo);
	}

	TArray<FAssetData> AllAssets;
	AssetRegistryModule.Get().GetAllAssets(AllAssets);
	TArray<FName> ShownNames;

	for (auto asset : AllAssets)
	{
		FString assetNameString = asset.AssetName.ToString();
		if (SpawnableClasses.Contains(assetNameString))
		{
			SpawnableClasses[assetNameString] = asset.GetAsset();
		}
	}

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* const CurrentClass = *ClassIt;
		if (CurrentClass->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		FString classNameString = CurrentClass->GetFName().ToString();
		if (SpawnableClasses.Contains(classNameString))
		{
			SpawnableClasses[classNameString] = CurrentClass;
		}
	}

	SpawnableClasses = SpawnableClasses.FilterByPredicate([](const TPair<FString, UObject*> pair)
		{
			if (pair.Value) return true;
			return false;
		});

	SpawnableClasses.KeySort([](FString a, FString b)
		{
			if (a.Compare(b) > 0) return false;
			else return true;
		});

	MessageBuilder mb;
	std::vector<mz::fb::String256> NameList;
	for (auto [name, ptr] : SpawnableClasses)
	{
		mz::fb::String256 str256;
		auto val = str256.mutable_val();
		auto size = name.Len() < 256 ? name.Len() : 256;
		memcpy(val->data(), TCHAR_TO_UTF8(*name), size);
		NameList.push_back(str256);
	}
	mz::fb::String256 listName;
	strcat((char*)listName.mutable_val()->data(), "UE5_ACTOR_LIST");
	Client->Write(MakeAppEvent(mb, mz::app::CreateUpdateStringList(mb, mz::fb::CreateString256ListDirect(mb, &listName, &NameList))));

	return;
}


#pragma optimize("", on)
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZClient, MZClient)

//
//#include "DispelUnrealMadnessPostlude.h"


#else
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MZClient);
#endif
