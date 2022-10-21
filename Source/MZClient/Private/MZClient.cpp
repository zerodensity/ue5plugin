

#include "MZClient.h"

#include "ScreenRendering.h"
#include "HardwareInfo.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"

#include "EngineUtils.h"
#include "GenericPlatform/GenericPlatformMemory.h"

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
		if (PluginClient)
		{
			PluginClient->OnPinShowAsChanged(*(FGuid*)action.pin_id(), action.show_as());
		}
    }

    virtual void OnFunctionCall(mz::app::FunctionCall const& action) override
    {
		LOG("Function called from mediaz");
		if (PluginClient)
		{

			PluginClient->OnFunctionCall(*(FGuid*)action.function()->id(), ParsePins(*action.function()));
		}
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
		std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins;
		for (auto [_, pin] : Pins)
		{
			graphPins.push_back(pin->Serialize(mb));
		}
		auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::ANY, 0, &graphPins, 0, 0, 0, &graphNodes));
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
	if (treeNode->GetAsActorNode())
	{
		graphPins = treeNode->GetAsActorNode()->SerializePins(mb);
	}
	std::vector<flatbuffers::Offset<mz::fb::Node>> graphFunctions; 
	if (treeNode->GetAsActorNode())
	{
		for (auto mzfunc : treeNode->GetAsActorNode()->Functions)
		{
			graphFunctions.push_back(mzfunc->Serialize(mb));
		}
	}
	auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::ANY, 0, &graphPins, 0, &graphFunctions, 0, &graphNodes));
	Client->Write(msg);

	//Send list actors on the scene 
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
	auto msg = MakeAppEvent(mb, mz::CreateNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::CLEAR_PINS, 0, &graphPins, 0, 0, 0, 0));
	Client->Write(msg);
	
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
				mzprop->PinShowAs = newShowAs;
				Pins.Add(mzprop->id, mzprop);
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
				mzfunc->Parameters = nullptr;
			}
		});
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
			FName CategoryName = FObjectEditorUtils::GetCategoryFName(AProperty);

			UClass* Class = ActorClass;

			if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryName.ToString()) || !PropertyVisible(AProperty))
			{
				AProperty = AProperty->PropertyLinkNext;
				continue;
			}

			MZProperty* mzprop = new MZProperty(actorNode->actor, AProperty);
			RegisteredProperties.Add(mzprop->id, mzprop);
			actorNode->Properties.push_back(mzprop);
			AProperty = AProperty->PropertyLinkNext;
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
				
				if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryName.ToString()) || !PropertyVisible(Property))
				{
					continue;
				}
				
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

				auto OwnerClass = UEFunction->GetOwnerClass();
				if (!OwnerClass || !Cast<UBlueprint>(OwnerClass->ClassGeneratedBy))
				{
					//continue; // export only BP functions //? what we will show in mediaz
				}
				
				MZFunction* mzfunc = new MZFunction(actorNode->actor, UEFunction);
				
				// Parse all function parameters.
				
				//uint8* Parms = (uint8*)FMemory_Alloca_Aligned(UEFunction->ParmsSize, UEFunction->GetMinAlignment());
				//mzfunc->StructMemory = Parms;
				//FMemory::Memzero(Parms, UEFunction->ParmsSize);

				//for (TFieldIterator<FProperty> It(UEFunction); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
				//{
				//	FProperty* LocalProp = *It;
				//	checkSlow(LocalProp);
				//	if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
				//	{
				//		LocalProp->InitializeValue_InContainer(Parms);
				//	}
				//}

				for (TFieldIterator<FProperty> PropIt(UEFunction); PropIt && PropIt->HasAnyPropertyFlags(CPF_Parm); ++PropIt)
				{

					MZProperty* mzprop = new MZProperty(nullptr, *PropIt);
					mzfunc->Properties.push_back(mzprop);
					RegisteredProperties.Add(mzprop->id, mzprop);
					
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
		//ITERATE CHILD COMPONENTS TO SHOW END

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

