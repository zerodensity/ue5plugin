

#include "MZClient.h"

#include "ScreenRendering.h"
#include "HardwareInfo.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"

#include "EngineUtils.h"

#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "ToolMenus.h"
#include "EditorActorFolders.h"

#define LOCTEXT_NAMESPACE "FMZClient"

#pragma optimize("", off)

#include "SlateBasics.h"
#include "EditorStyleSet.h"

#include "SceneTree.h"

class FMediaZPluginEditorCommands : public TCommands<FMediaZPluginEditorCommands>
{
public:
	FMediaZPluginEditorCommands()
		: TCommands<FMediaZPluginEditorCommands>
		(
			TEXT("MediaZPluginEditor"),
			NSLOCTEXT("Contexts", "MediaZPluginEditor", "MediaZPluginEditor Plugin"),
			NAME_None,
			FEditorStyle::GetStyleSetName()
			) {}

	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> TestCommand;
	TSharedPtr<FUICommandInfo> PopulateRootGraph;
	TSharedPtr<FUICommandInfo> SendRootUpdate;
};

void FMediaZPluginEditorCommands::RegisterCommands()
{
	UI_COMMAND(TestCommand, "TestCommand", "This is test command", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PopulateRootGraph, "PopulateRootGraph", "Call PopulateRootGraph", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(SendRootUpdate, "SendRootUpdate", "Call SendNodeUpdate with Root Graph Id", EUserInterfaceActionType::Button, FInputGesture());
}




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
		
		UE_LOG(LogTemp, Warning, TEXT("Connected to mzEngine"));

        if (flatbuffers::IsFieldPresent(&event, mz::app::AppConnectedEvent::VT_NODE))
        {
            nodeId = *(FGuid*)event.node()->id();
        }
		memcpy(PluginClient->RootGraph->mutable_id()->mutable_bytes()->Data(), event.node()->id()->bytes()->Data(), event.node()->id()->bytes()->size());
		PluginClient->Connected();
    }

    virtual void OnNodeUpdate(mz::NodeUpdated const& archive) override
    {
    }

    virtual void OnMenuFired(mz::ContextMenuRequest const& request) override
    {
    }

    void OnTextureCreated(mz::fb::Texture const& texture)
    {
    }

    virtual void Done(grpc::Status const& Status) override
    {
		PluginClient->Disconnected();
		shutdown = true;
		nodeId = {};
    }

    virtual void OnNodeRemoved(mz::app::NodeRemovedEvent const& action) override
    {
    }

    virtual void OnPinShowAsChanged(mz::PinShowAsChanged const& action) override
    {
    }

    virtual void OnFunctionCall(mz::app::FunctionCall const& action) override
    {
    }

	virtual void OnExecute(mz::app::AppExecute const& aE) override
    {
    }

	FMZClient* PluginClient;
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

void FMZClient::Connected()
{
	//PopulateRootGraph();
	SendNodeUpdate(Client->nodeId);
}

void FMZClient::NodeRemoved() 
{
    Client->nodeId = {};
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
	//SendNodeUpdate(Client->nodeId);
}

void FMZClient::OnPostWorldInit(UWorld* world, const UWorld::InitializationValues initValues)
{
	PopulateRootGraph();
	if (Client)
	{
		SendNodeUpdate(Client->nodeId);
	}
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

	//PopulateRootGraph();


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
			FExecuteAction::CreateRaw(this, &FMZClient::PopulateRootGraph));
		CommandList->MapAction(
			FMediaZPluginEditorCommands::Get().SendRootUpdate,
			FExecuteAction::CreateLambda([=](){
					SendNodeUpdate(Client->nodeId);
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
		}
	}

#endif //WITH_EDITOR
}

void FMZClient::ShutdownModule() 
{
	FMediaZPluginEditorCommands::Unregister();
}

void FMZClient::TestAction()
{
	UE_LOG(LogTemp, Warning, TEXT("It Works!!!"));
}

bool FMZClient::Tick(float dt)
{
    InitConnection();
	return true;
}

bool IsActorDisplayable(const AActor* Actor)
{
	static const FName SequencerActorTag(TEXT("SequencerActor"));

	return Actor &&
		Actor->IsEditable() &&																	// Only show actors that are allowed to be selected and drawn in editor
		Actor->IsListedInSceneOutliner() &&
		(((Actor->GetWorld() && Actor->GetWorld()->IsPlayInEditor()) || !Actor->HasAnyFlags(RF_Transient)) ||
			(Actor->ActorHasTag(SequencerActorTag))) &&
		!Actor->IsTemplate() &&																	// Should never happen, but we never want CDOs displayed
		!Actor->IsA(AWorldSettings::StaticClass()) &&											// Don't show the WorldSettings actor, even though it is technically editable
		IsValidChecked(Actor);																// We don't want to show actors that are about to go away
}

flatbuffers::Offset<mz::fb::Node> CreateFBGraphRecursive(flatbuffers::FlatBufferBuilder& fbb, TreeNode* treeNode)
{
	if (treeNode->Children.empty())
	{
		FGuid actorId = FGuid::NewGuid();
		return 	mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&actorId, std::string(TCHAR_TO_UTF8(*treeNode->Name)).c_str(), "UE5.Actor", false, true, 0, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb).Union(), "UE5", 0, "ENGINE NODES");
	}

	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes;

	for (auto child : treeNode->Children)
	{
		childNodes.push_back(CreateFBGraphRecursive(fbb, child));
	}

	FGuid actorId = FGuid::NewGuid();
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&actorId, std::string(TCHAR_TO_UTF8(*treeNode->Name)).c_str(), "UE5.Actor", false, true, 0, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), "UE5", 0, "ENGINE NODES");
}

void FMZClient::PopulateRootGraphWithSceneTree(SceneTree sceneTree)
{
	flatbuffers::FlatBufferBuilder fbb;
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes;
	childNodes.push_back(CreateFBGraphRecursive(fbb, sceneTree.Root));

	fbb.Finish(mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&Client->nodeId, "UE5", "UE5.UE5", false, true, 0, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), "UE5", 0, 0));
	RootGraph = fbb.Release();

	TRootGraph = RootGraph->UnPack();
}

void FMZClient::PopulateRootGraph() //Runs in game thread
{
	UWorld* World = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();
	
	SceneTree sceneTree;

	TArray<FString> names_experiment;

	//FActorFolders::Get().ForEachFolder(*World, [this, &World, &names_experiment, &sceneTree](const FFolder& Folder)
	//{
	//		names_experiment.Add(Folder.GetPath().ToString());
	//		sceneTree.AddItem(Folder.GetPath().ToString());
	//		//if (FSceneOutlinerTreeItemPtr FolderItem = Mode->CreateItemFor<FActorFolderTreeItem>(FActorFolderTreeItem(Folder, World)))
	//		//{
	//		//	OutItems.Add(FolderItem);
	//		//}
	//		return true;
	//});

	flatbuffers::FlatBufferBuilder fbb;
	std::vector<flatbuffers::Offset<mz::fb::Node>> actorNodes;

	TArray<AActor*> ActorsInScene;
	if (World)
	{
		TActorIterator< AActor > ActorItr = TActorIterator< AActor >(World);
		while (ActorItr)
		{
			if (!IsActorDisplayable(*ActorItr))
			{
				++ActorItr;
				continue;
			}
			ActorsInScene.Add(*ActorItr);
			names_experiment.Add(ActorItr->GetFolder().GetPath().ToString() + TEXT("/") + ActorItr->GetActorLabel());
			sceneTree.AddItem(ActorItr->GetFolder().GetPath().ToString() + TEXT("/") + ActorItr->GetActorLabel());
			++ActorItr;
		}
	}

 	PopulateRootGraphWithSceneTree(sceneTree);
}

void FMZClient::SendNodeUpdate(FGuid nodeId)
{
	if (!Client || !Client->nodeId.IsValid())
	{
		return;
	}
	
	if (nodeId == *(FGuid*)RootGraph->id())
	{
		MessageBuilder mb;
		std::vector<flatbuffers::Offset<mz::fb::Node>> nodeFunctions;
		std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes;
		std::vector<flatbuffers::Offset<mz::fb::Pin>> pins;

		for(auto child : *(RootGraph->contents_as_Graph()->nodes()))
		{
			graphNodes.push_back(mz::fb::CreateNode(mb, child->UnPack()));
		}

		auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, true, 0, 0, 0, 0, 0, &graphNodes));
		Client->Write(msg);
	}
	

	//Send list actors on the scene 
}


#pragma optimize("", on)
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZClient, MZClient)

//
//#include "DispelUnrealMadnessPostlude.h"

