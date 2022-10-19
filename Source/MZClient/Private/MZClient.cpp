

#include "MZClient.h"

#include "ScreenRendering.h"
#include "HardwareInfo.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"

#include "EngineUtils.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "ToolMenus.h"
#include "EditorActorFolders.h"
#include "SlateBasics.h"
#include "EditorStyleSet.h"
#endif //WITH_EDITOR

#define LOCTEXT_NAMESPACE "FMZClient"

#pragma optimize("", off)

#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"


#include "SceneTree.h"
#include "MZActorProperties.h"
#include "MZActorFunctions.h"

DEFINE_LOG_CATEGORY(LogMediaZ);
#define LOG(x) UE_LOG(LogMediaZ, Warning, TEXT(x))

#if WITH_EDITOR
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

	virtual void RegisterCommands() override
	{
		UI_COMMAND(TestCommand, "TestCommand", "This is test command", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(PopulateRootGraph, "PopulateRootGraph", "Call PopulateRootGraph", EUserInterfaceActionType::Button, FInputGesture());
		UI_COMMAND(SendRootUpdate, "SendRootUpdate", "Call SendNodeUpdate with Root Graph Id", EUserInterfaceActionType::Button, FInputGesture());
	}

public:
	TSharedPtr<FUICommandInfo> TestCommand;
	TSharedPtr<FUICommandInfo> PopulateRootGraph;
	TSharedPtr<FUICommandInfo> SendRootUpdate;
};
#endif // WITH_EDITOR

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
		
		UE_LOG(LogMediaZ, Warning, TEXT("Connected to mzEngine"));
        if (flatbuffers::IsFieldPresent(&event, mz::app::AppConnectedEvent::VT_NODE))
        {
            nodeId = *(FGuid*)event.node()->id();
			PluginClient->sceneTree.Root->id = *(FGuid*)event.node()->id();
			PluginClient->Connected();
        }
    }

    virtual void OnNodeUpdate(mz::NodeUpdated const& archive) override
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
    }

    virtual void OnMenuFired(mz::ContextMenuRequest const& request) override
    {
		LOG("Menu fired from mediaz");

    }

    void OnTextureCreated(mz::fb::Texture const& texture)
    {
		LOG("Texture created from mediaz");

    }

    virtual void Done(grpc::Status const& Status) override
    {
		LOG("Connection with mediaz is finished.");

		PluginClient->Disconnected();
		shutdown = true;
		nodeId = {};
    }

    virtual void OnNodeRemoved(mz::app::NodeRemovedEvent const& action) override
    {
		LOG("Plugin node removed from mediaz");
		nodeId = {};
    }

	virtual void OnPinValueChanged(mz::PinValueChanged const& action) override
	{
		LOG("Pin value changed from mediaz editor");
		if (PluginClient)
		{
			PluginClient->SetPropertyValue(*(FGuid*)action.pin_id(), (void*)action.value()->data(), action.value()->size());
		}
	}

    virtual void OnPinShowAsChanged(mz::PinShowAsChanged const& action) override
    {
		LOG("Pin show as changed from mediaz");
    }

    virtual void OnFunctionCall(mz::app::FunctionCall const& action) override
    {
		LOG("Function called from mediaz");
    }

	virtual void OnExecute(mz::app::AppExecute const& aE) override
    {
    }

	virtual void OnNodeSelected(mz::NodeSelected const& action) override
	{
		LOG("Node selected from mediaz");
		if (PluginClient)
		{
			PluginClient->OnNodeSelected(*(FGuid*)action.node_id());
		}
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

void FMZClient::SetPropertyValue(FGuid pinId, void* newval, size_t size)
{
	if (!RegisteredProperties.Contains(pinId))
	{
		LOG("The property with given id is not found.");
		return;
	}

	MZProperty* mzprop = RegisteredProperties.FindRef(pinId);
	
	TaskQueue.Enqueue([mzprop, newval, size]()
		{
			mzprop->SetValue(newval, size);
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
		});
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
	LOG("AppClient instance is created");
}

void FMZClient::OnPostWorldInit(UWorld* world, const UWorld::InitializationValues initValues)
{
	PopulateSceneTree();
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
	//actor spawn
	//actor kill
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
			FExecuteAction::CreateRaw(this, &FMZClient::PopulateSceneTree));
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

	while (!TaskQueue.IsEmpty()) {
		Task task;
		TaskQueue.Dequeue(task);
		task();
	}

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
#if WITH_EDITOR
	sceneTree.Clear();

	UWorld* World = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();

	TArray<FString> names_experiment;

	// This code is commented out beacuse looks like folders does not exist in standalone mode || needs further investigation
	// 
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
			names_experiment.Add(ActorItr->GetFolder().GetPath().ToString() + TEXT("/") + ActorItr->GetActorLabel());
			ActorNode* newNode = sceneTree.AddActor(ActorItr->GetFolder().GetPath().ToString(), *ActorItr);
			if (newNode)
			{
				newNode->actor = *ActorItr;
			}
		}
	}
#endif //WITH_EDITOR
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

		auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, true, 0, 0, 0, 0, 0, &graphNodes));
		Client->Write(msg);
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
	if (treeNode->GetAsActorNode())
	{
		graphPins = treeNode->GetAsActorNode()->SerializePins(mb);
	}

	auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, true, 0, &graphPins, 0, 0, 0, &graphNodes));
	Client->Write(msg);

	
	//Send list actors on the scene 
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

		//experiment

		auto ActorClass = actorNode->actor->GetClass();
		class FProperty* Sroperty = ActorClass->PropertyLink;
		TArray<FName> PropertyNames;
		while (Sroperty != nullptr)
		{
			FName CategoryName = FObjectEditorUtils::GetCategoryFName(Sroperty);

			UClass* Class = ActorClass;

			if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryName.ToString()))
			{
				Sroperty = Sroperty->PropertyLinkNext;
				continue;
			}

			PropertyNames.Add(Sroperty->GetFName());
			MZProperty* mzprop = new MZProperty(actorNode->actor, Sroperty);
			RegisteredProperties.Add(mzprop->id, mzprop);
			actorNode->Properties.push_back(mzprop);
			Sroperty = Sroperty->PropertyLinkNext;
		}

		auto Components = actorNode->actor->GetComponents();
		for (auto Component : Components)
		{
			auto ComponentClass = Component->GetClass();

			if (Component->IsEditorOnly())
			{
				continue;
			}

			for (FProperty* Property = ComponentClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
			{
				FName CategoryName = FObjectEditorUtils::GetCategoryFName(Property);

				UClass* Class = ActorClass;
				
				if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryName.ToString()))
				{
					continue;
				}
				
				MZProperty* mzprop = new MZProperty(Component, Property);
				RegisteredProperties.Add(mzprop->id, mzprop);
				actorNode->Properties.push_back(mzprop);
				PropertyNames.Add(Property->GetFName());
			}
		}
		LOG("Getting property experiment is over.");
		//end experiment

		actorNode->Children.clear();
		USceneComponent* rootComponent = actorNode->actor->GetRootComponent();

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
				&& (ActorComp->CreationMethod != EComponentCreationMethod::Native); //|| FComponentEditorUtils::GetPropertyForEditableNativeComponent(ActorComp));
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

		treeNode->needsReload = false;
		return true;
	}
	else if (treeNode->GetAsSceneComponentNode())
	{

		treeNode->needsReload = false;
		return true;
	}

	
	return false;
}


#pragma optimize("", on)
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZClient, MZClient)

//
//#include "DispelUnrealMadnessPostlude.h"

