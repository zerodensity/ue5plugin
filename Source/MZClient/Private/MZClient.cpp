#if WITH_EDITOR

#include "MZClient.h"

// std
#include <cstdio>

// UE
#include "TimerManager.h"

//#include "ScreenRendering.h"
//#include "HardwareInfo.h"//

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "EngineUtils.h"
#include "GenericPlatform/GenericPlatformMemory.h"

#include "MZTextureShareManager.h"

#include "HardwareInfo.h"
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

#include "MZSceneTree.h"
#include "MZActorProperties.h"
#include "MZActorFunctions.h"
#include "AppTemplates.h"

#include "Kismet2/ComponentEditorUtils.h"

#include "Elements/Interfaces/TypedElementObjectInterface.h" //experiment
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Actor/ActorElementData.h"

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
    //FMessageDialog::Debugf(FText::FromString("Connected to mzEngine"), 0);
		
	UE_LOG(LogMediaZ, Warning, TEXT("Connected to mzEngine"));
    if (flatbuffers::IsFieldPresent(&event, mz::app::AppConnectedEvent::VT_NODE))
    {
        NodeId = *(FGuid*)event.node()->id();
		PluginClient->SceneTree.Root->Id = *(FGuid*)event.node()->id();
		PluginClient->Connected();
    }
}

void ClientImpl::OnNodeUpdate(mz::FullNodeUpdate const& archive) 
{
	LOG("Node update from mediaz");
	if (!NodeId.IsValid())
	{
		if (flatbuffers::IsFieldPresent(&archive, mz::FullNodeUpdate::VT_NODE))
		{
			NodeId = *(FGuid*)archive.node()->id();
			PluginClient->SceneTree.Root->Id = *(FGuid*)archive.node()->id();
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
	IsChannelReady = false;
	NodeId = {};
	PluginClient->Disconnected();
}

void ClientImpl::OnNodeRemoved(mz::app::NodeRemovedEvent const& action) 
{
	LOG("Plugin node removed from mediaz");
	NodeId = {};
	if (PluginClient && PluginClient->MZTimeStep)
	{
		PluginClient->MZTimeStep->Step();
	}
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
	
	if (PluginClient)
	{
		PluginClient->OnUpdatedNodeExecuted(ParsePins(*aE.node()));
	}
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
	if (PluginClient)
	{
		FVector2D pos(request.pos()->x(), request.pos()->y());
		PluginClient->OnContexMenuFired(*(FGuid*)request.item_id(), pos, request.instigator());
	}
}

void ClientImpl::OnCommandFired(mz::ContextMenuAction const& action)
{
	LOG("Context menu command fired from MediaZ");
	if (PluginClient)
	{
		PluginClient->OnContexMenuActionFired(*(FGuid*)action.item_id(), action.command());
	}
}

void ClientImpl::OnNodeImported(mz::app::NodeImported const& action)
{
	LOG("Node imported from MediaZ");
	if (PluginClient)
	{
		PluginClient->OnNodeImported(action.node());
	}
}

FMZClient::FMZClient() {}

void GetNodesSpawnedByMediaz(const mz::fb::Node* node, TMap<FGuid, FString>& spawnedByMediaz)
{
	if (flatbuffers::IsFieldPresent(node, mz::fb::Node::VT_META_DATA_MAP))
	{
		if (auto entry = node->meta_data_map()->LookupByKey("spawnTag"))
		{
			spawnedByMediaz.Add(*(FGuid*)node->id(), FString(entry->value()->c_str()));
		}
	}
	for (auto child : *node->contents_as_Graph()->nodes())
	{
		GetNodesSpawnedByMediaz(child, spawnedByMediaz);
	}
}

void GetNodesWithProperty(const mz::fb::Node* node, std::vector<const mz::fb::Node*>& out)
{
	if (node->pins()->size() > 0)
	{
		out.push_back(node);
	}
#if 0 //load only the root node
	for (auto child : *node->contents_as_Graph()->nodes())
	{
		GetNodesWithProperty(child, out);
	}
#endif
}

struct PropUpdate
{
	FGuid actorId;
	FString displayName;
	FString componentName;
	FString propName;
	void* newVal;
	size_t newValSize;
	void* defVal;
	size_t defValSize;
	mz::fb::ShowAs pinShowAs;
};

void FMZClient::OnNodeImported(const mz::fb::Node* node)
{
	std::vector<const mz::fb::Node*> nodesWithProperty;
	GetNodesWithProperty(node, nodesWithProperty);
	std::vector<PropUpdate> updates;
	for (auto nodeW : nodesWithProperty)
	{
		FGuid id = *(FGuid*)(nodeW->id());
		for (auto prop : *nodeW->pins())
		{	
			if (flatbuffers::IsFieldPresent(prop, mz::fb::Pin::VT_META_DATA_MAP))
			{
				FString componentName;
				FString displayName;
				FString propName;
				char* valcopy = new char[prop->data()->size()];
				char* defcopy = new char[prop->def()->size()];
				memcpy(valcopy, prop->data()->data(), prop->data()->size());
				memcpy(defcopy, prop->def()->data(), prop->def()->size());

				if (auto entry = prop->meta_data_map()->LookupByKey("property"))
				{
					propName = FString(entry->value()->c_str());
				}
				if (auto entry = prop->meta_data_map()->LookupByKey("component"))
				{
					componentName = FString(entry->value()->c_str());
				}
				if (auto entry = prop->meta_data_map()->LookupByKey("actorId"))
				{
					FString actorIdString = FString(entry->value()->c_str());
					FGuid actorId;
					if (FGuid::Parse(actorIdString, actorId))
					{
						id = actorId;
					}
					//componentName = FString(entry->value()->c_str());
				}

				if (flatbuffers::IsFieldPresent(prop, mz::fb::Pin::VT_NAME))
				{
					displayName = FString(prop->name()->c_str());
				}

				updates.push_back({ id, displayName, componentName, propName, valcopy, prop->data()->size(), defcopy, prop->def()->size(), prop->show_as()});
			}
			
		}
	}

	TMap<FGuid, FString> spawnedByMediaz; //old guid (imported) x spawn tag
	GetNodesSpawnedByMediaz(node, spawnedByMediaz);
	
	TaskQueue.Enqueue([this, updates, spawnedByMediaz]()
		{

			UWorld* World = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();

			TMap<FGuid, AActor*> sceneActorMap;
			if (World)
			{
				for (TActorIterator< AActor > ActorItr(World); ActorItr; ++ActorItr)
				{
					sceneActorMap.Add(ActorItr->GetActorGuid(), *ActorItr);
				}
			}

			for (auto [oldGuid, spawnTag] : spawnedByMediaz)
			{
				if (!sceneActorMap.Contains(oldGuid))
				{
					///spawn
					FString actorName(spawnTag);
					AActor* spawnedActor = nullptr;
					if (ActorPlacementParamMap.Contains(actorName))
					{
						auto placementInfo = ActorPlacementParamMap.FindRef(actorName);
						UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
						if (PlacementSubsystem)
						{
							TArray<FTypedElementHandle> PlacedElements = PlacementSubsystem->PlaceAsset(placementInfo, FPlacementOptions());
							for (auto elem : PlacedElements)
							{
								const FActorElementData* ActorElement = elem.GetData<FActorElementData>(true);
								if (ActorElement)
								{
									spawnedActor = ActorElement->Actor;
								}
							}
						}
					}
					else if (SpawnableClasses.Contains(actorName))
					{
						if (GEngine)
						{
							if (UObject* ClassToSpawn = SpawnableClasses[actorName])
							{
								UBlueprint* GeneratedBP = Cast<UBlueprint>(ClassToSpawn);
								UClass* NativeClass = Cast<UClass>(ClassToSpawn);
								UClass* Class = GeneratedBP ? (UClass*)(GeneratedBP->GeneratedClass) : (NativeClass);
								spawnedActor = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(Class);
							}
						}
					}
					else
					{
						LOG("Cannot spawn actor");
					}
					if (spawnedActor)
					{
						sceneActorMap.Add(oldGuid, spawnedActor); //this will map the old id with spawned actor in order to match the old properties (imported from disk)
					}
				}
			}

			for (auto update : updates)
			{
				if (sceneActorMap.Contains(update.actorId))
				{
					auto actor = sceneActorMap.FindRef(update.actorId);
					TSharedPtr<MZProperty> mzprop = nullptr;
					if (update.componentName.IsEmpty())
					{
						auto prp = FindFProperty<FProperty>(actor->GetClass(), TCHAR_TO_UTF8(*update.propName));
						if (prp)
						{
							mzprop = MZPropertyFactory::CreateProperty(actor, prp);
						}
					}
					else
					{
						auto component = FindObject<USceneComponent>(actor, *update.componentName);
						auto prp = FindFProperty<FProperty>(component->GetClass(), TCHAR_TO_UTF8(*update.propName));
						if (component && prp)
						{
							mzprop = MZPropertyFactory::CreateProperty(component, prp);
						}

					}
					if (mzprop)
					{
						mzprop->SetPropValue(update.newVal, update.newValSize);
					}
					if (!update.displayName.IsEmpty())
					{
						mzprop->DisplayName = update.displayName;
					}
					mzprop->UpdatePinValue();
					mzprop->PinShowAs = update.pinShowAs;
					mzprop->default_val = std::vector<uint8>(update.defValSize, 0);
					memcpy(mzprop->default_val.data(), update.defVal, update.defValSize);
					Pins.Add(mzprop->Id, mzprop);
					RegisteredProperties.Add(mzprop->Id, mzprop);
					//PropertiesMap.Add(mzprop->Property, mzprop);

				}


				delete update.newVal;
				delete update.defVal;
			}
			
			SceneTree.Clear();
			RegisteredProperties = Pins;
			PropertiesMap.Empty();
			PopulateSceneTree(false);

			SendNodeUpdate(Client->NodeId);
			SendAssetList();

		});
}


void FMZClient::SetPropertyValue(FGuid pinId, void* newval, size_t size)
{
	if (!RegisteredProperties.Contains(pinId))
	{
		LOG("The property with given id is not found.");
		return;
	}

	auto mzprop = RegisteredProperties.FindRef(pinId);
	std::vector<uint8_t> copy(size, 0);
	memcpy(copy.data(), newval, size);

	TaskQueue.Enqueue([mzprop, copy, size, this]()
		{
			bool isChangedBefore = mzprop->IsChanged;
			mzprop->SetPropValue((void*)copy.data(), size);
			if (!isChangedBefore && mzprop->IsChanged)
			{
				//changed first time 
				TSharedPtr<MZProperty> newmzprop = nullptr;
				if (mzprop->GetRawObjectContainer())
				{
					newmzprop = MZPropertyFactory::CreateProperty(mzprop->GetRawObjectContainer(), mzprop->Property, &(RegisteredProperties)/*, &(mzclient->PropertiesMap)*/);
				}
				else if (mzprop->StructPtr)
				{
					newmzprop = MZPropertyFactory::CreateProperty(nullptr, mzprop->Property, &(RegisteredProperties), 0 /*, &(mzclient->PropertiesMap)*/, FString(""), mzprop->StructPtr);
				}
				if (newmzprop)
				{
					newmzprop->default_val = mzprop->default_val;
					newmzprop->PinShowAs = mz::fb::ShowAs::PROPERTY;

					UObject* container = mzprop->GetRawObjectContainer();
if (container)
{
	newmzprop->DisplayName += FString(" (") + container->GetFName().ToString() + FString(")");
	newmzprop->CategoryName = container->GetFName().ToString() + FString("|") + newmzprop->CategoryName;
}

newmzprop->transient = false;
Pins.Add(newmzprop->Id, newmzprop);
//RegisteredProperties.Add(newmzprop->Id, newmzprop);
SendPinAdded(Client->NodeId, newmzprop);
				}

			}
			if (Pins.Contains(mzprop->Id))
			{
				if (PropertiesMap.Contains(mzprop->Property))
				{
					auto otherProp = PropertiesMap.FindRef(mzprop->Property);
					otherProp->UpdatePinValue();
					SendPinValueChanged(otherProp->Id, otherProp->data);
				}

			}
			else
			{
				for (auto& [id, pin] : Pins)
				{
					if (pin->Property == mzprop->Property)
					{
						pin->UpdatePinValue();
						SendPinValueChanged(pin->Id, pin->data);
					}
				}
			}
		});
}


bool FMZClient::IsConnected()
{
	return Client && Client->IsChannelReady && Client->NodeId.IsValid();
}

void FMZClient::Connected()
{
	TaskQueue.Enqueue([&]()
		{
			PopulateSceneTree();
			SendNodeUpdate(Client->NodeId);
			SendAssetList();
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

	if (!Client)
	{

		std::string ProtoPath = (std::filesystem::path(std::getenv("PROGRAMDATA")) / "mediaz" / "core" / "Applications" / "Unreal Engine 5").string();
		// memleak
		Client = new ClientImpl("UE5", "UE5", ProtoPath.c_str());
		Client->PluginClient = this;
		LOG("AppClient instance is created");
	}

	// (Samil) TODO: This connection logic should be provided by the SDK itself. 
	// App developers should not be required implement 'always connect' behaviour.
	if (!Client->IsChannelReady)
	{
		Client->IsChannelReady = (GRPC_CHANNEL_READY == Client->Connect());
	}

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

void FMZClient::OnPostWorldInit(UWorld* world, const UWorld::InitializationValues initValues)
{
	if (world != GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World())
	{
		return;
	}

	IsWorldInitialized = true;
	FOnActorSpawned::FDelegate ActorSpawnedDelegate = FOnActorSpawned::FDelegate::CreateRaw(this, &FMZClient::OnActorSpawned);
	FOnActorDestroyed::FDelegate ActorDestroyedDelegate = FOnActorDestroyed::FDelegate::CreateRaw(this, &FMZClient::OnActorDestroyed);
	world->AddOnActorSpawnedHandler(ActorSpawnedDelegate);
	world->AddOnActorDestroyedHandler(ActorDestroyedDelegate);

	PopulateSceneTree();
	if (Client)
	{
		//SendAssetList();
		SendNodeUpdate(Client->NodeId);
	}
}

void FMZClient::OnPreWorldFinishDestroy(UWorld* World)
{
	LOG("World is destroyed");

	PopulateSceneTree();
	if (Client)
	{
		SendNodeUpdate(Client->NodeId);
	}
}

void FMZClient::OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty && !ObjectBeingModified->IsA(PropertyChangedEvent.MemberProperty->GetOwner<UClass>()))
	{
		return;
	}
	if (PropertyChangedEvent.Property && !ObjectBeingModified->IsA(PropertyChangedEvent.Property->GetOwner<UClass>()))
	{
		return;
	}
	if (!PropertyChangedEvent.Property->IsValidLowLevel())
	{
		return;
	}
	if (PropertiesMap.Contains(PropertyChangedEvent.Property))
	{
		auto mzprop = PropertiesMap.FindRef(PropertyChangedEvent.Property);
		mzprop->UpdatePinValue();
		SendPinValueChanged(mzprop->Id, mzprop->data);
		LOG("PROPERTY FOUND HURRRAAAH");
	}
	if (PropertiesMap.Contains(PropertyChangedEvent.MemberProperty))
	{
		auto mzprop = PropertiesMap.FindRef(PropertyChangedEvent.MemberProperty);
		mzprop->UpdatePinValue();
		SendPinValueChanged(mzprop->Id, mzprop->data);
		LOG("PROPERTY FOUND HURRRAAAH");
	}
	for (auto [id, pin] :  Pins)
	{
		if (pin->Property == PropertyChangedEvent.MemberProperty)
		{
			pin->UpdatePinValue();
			SendPinValueChanged(pin->Id, pin->data);
			LOG("PIN FOUND HURRRAAAH");
			break;
		}
		else if (pin->Property == PropertyChangedEvent.Property)
		{
			pin->UpdatePinValue();
			SendPinValueChanged(pin->Id, pin->data);
			LOG("PIN FOUND HURRRAAAH");
			break;
		}
		else if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->IsA<FStructProperty>())
		{
			auto structProp = (FStructProperty*)PropertyChangedEvent.MemberProperty;
			uint8* StructInst = structProp->ContainerPtrToValuePtr<uint8>(ObjectBeingModified);
			if (pin->StructPtr == StructInst)
			{
				pin->UpdatePinValue();
				SendPinValueChanged(pin->Id, pin->data);
				LOG("PIN FOUND HURRRAAAH");
				break;
			}
		}
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
				if (SceneTree.NodeMap.Contains(InActor->GetActorGuid()))
				{
					return;
				}
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
	TSet<UObject*> RemovedItems;
	RemovedItems.Add(InActor);
	auto Components = InActor->GetComponents();
	for (auto comp : Components)
	{
		RemovedItems.Add(comp);
	}

	/*TaskQueue.Enqueue([id, removedItems, this]()
		{*/
			SendActorDeleted(id, RemovedItems);
	//	});
}

void FMZClient::StartupModule() {

	auto hwinfo = FHardwareInfo::GetHardwareInfo(NAME_RHI);
	if ("D3D12" != hwinfo)
	{
		FMessageDialog::Debugf(FText::FromString("MediaZ plugin supports DirectX12 only!"), 0);
		return;
	}

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

	UENodeStatusHandler.SetClient(Client);

	//Add Delegates
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMZClient::Tick));
	FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FMZClient::OnPostWorldInit);
	FWorldDelegates::OnPreWorldFinishDestroy.AddRaw(this, &FMZClient::OnPreWorldFinishDestroy);
	FEditorDelegates::PostPIEStarted.AddRaw(this, &FMZClient::HandleBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FMZClient::HandleEndPIE);

	FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate OnPropertyChangedDelegate = FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate::CreateRaw(this, &FMZClient::OnPropertyChanged);
	OnPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.Add(OnPropertyChangedDelegate);

	//EndPlayMapDelegate
	//actor spawn
	//actor kill
	//PopulateRootGraph();

	//ADD CUSTOM FUNCTIONS
	{
		MZCustomFunction* mzcf = new MZCustomFunction;
		mzcf->Id = FGuid::NewGuid();
		FGuid actorPinId = FGuid::NewGuid();
		mzcf->Params.Add(actorPinId, "Spawn Actor");
		mzcf->Serialize = [funcid = mzcf->Id, actorPinId](flatbuffers::FlatBufferBuilder& fbb)->flatbuffers::Offset<mz::fb::Node>
		{
			//todo remove unneccessary code
			FString val("");
			auto s = StringCast<ANSICHAR>(*val);
			auto data = std::vector<uint8_t>(s.Length() + 1, 0);
			memcpy(data.data(), s.Get(), s.Length());
			std::vector<flatbuffers::Offset<mz::fb::Pin>> spawnPins = {
				mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&actorPinId, TCHAR_TO_ANSI(TEXT("Actor List")), TCHAR_TO_ANSI(TEXT("string")), mz::fb::ShowAs::PROPERTY, mz::fb::CanShowAs::PROPERTY_ONLY, "UE PROPERTY", mz::fb::CreateVisualizerDirect(fbb, mz::fb::VisualizerType::COMBO_BOX, "UE5_ACTOR_LIST"), &data),
			};
			return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&funcid, "Spawn Actor", "UE5.UE5", false, true, &spawnPins, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(fbb, mz::fb::JobType::CPU).Union(), "UE5", 0, "ENGINE FUNCTIONS");
		};
		mzcf->Function = [mzclient = this, actorPinId](TMap<FGuid, std::vector<uint8>> properties)
		{
			FString actorName((char*)properties.FindRef(actorPinId).data());
			AActor* spawnedActor = nullptr;
			if (mzclient->ActorPlacementParamMap.Contains(actorName))
			{
				auto placementInfo = mzclient->ActorPlacementParamMap.FindRef(actorName);
				UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
				if (PlacementSubsystem)
				{
					TArray<FTypedElementHandle> PlacedElements = PlacementSubsystem->PlaceAsset(placementInfo, FPlacementOptions());
					for (auto elem : PlacedElements)
					{
						const FActorElementData* ActorElement = elem.GetData<FActorElementData>(true);
						if (ActorElement)
						{
							spawnedActor = ActorElement->Actor;
						}
					}
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
						spawnedActor = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(Class);
					}
				}
			}
			else
			{
				LOG("Cannot spawn actor");
			}
			if (spawnedActor)
			{
				mzclient->SendActorAdded(spawnedActor, actorName);
				LOGF("Spawned actor %s", *spawnedActor->GetFName().ToString());
			}

			//mzclient->PopulateSceneTree();
			//mzclient->SendNodeUpdate(mzclient->Client->NodeId);
		};
		CustomFunctions.Add(mzcf->Id, mzcf);
	}
	//Add Camera function
	{
		MZCustomFunction* mzcf = new MZCustomFunction;
		mzcf->Id = FGuid::NewGuid();
		mzcf->Serialize = [funcid = mzcf->Id](flatbuffers::FlatBufferBuilder& fbb)->flatbuffers::Offset<mz::fb::Node>
		{
			return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&funcid, "Spawn Reality Camera", "UE5.UE5", false, true, 0, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(fbb, mz::fb::JobType::CPU).Union(), "UE5", 0, "ENGINE FUNCTIONS");
		};
		mzcf->Function = [mzclient = this](TMap<FGuid, std::vector<uint8>> properties)
		{
			FString actorName("Reality_Camera");


			TSoftClassPtr<AActor> ActorBpClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/RealityEngine/Actors/Reality_Camera.Reality_Camera_C'")));

			UClass* LoadedBpAsset = ActorBpClass.LoadSynchronous();

			//UBlueprint* GeneratedBP = Cast<UBlueprint>(LoadedBpAsset);
			AActor* realityCamera = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(LoadedBpAsset);
			if (realityCamera)
			{
				mzclient->SendActorAdded(realityCamera, actorName);
				LOGF("Spawned actor %s", *realityCamera->GetFName().ToString());
			}
			else
			{
				return;
			}
			//auto videoCamera = realityCamera->GetRootComponent();
			auto videoCamera = FindObject<USceneComponent>(realityCamera, TEXT("VideoCamera"));
			std::vector<TSharedPtr<MZProperty>> pinsToSpawn;
			{
				auto texture = FindFProperty<FObjectProperty>(videoCamera->GetClass(), "FrameTexture");
				auto mzprop = MZPropertyFactory::CreateProperty(videoCamera, texture, &(mzclient->RegisteredProperties));
				if (mzprop)
				{
					mzprop->PinShowAs = mz::fb::ShowAs::OUTPUT_PIN;
					pinsToSpawn.push_back(mzprop);
				}
			}
			{
				auto texture = FindFProperty<FObjectProperty>(videoCamera->GetClass(), "MaskTexture");
				auto mzprop = MZPropertyFactory::CreateProperty(videoCamera, texture, &(mzclient->RegisteredProperties));
				if (mzprop)
				{
					mzprop->PinShowAs = mz::fb::ShowAs::OUTPUT_PIN;
					pinsToSpawn.push_back(mzprop);
				}
			}
			{
				auto texture = FindFProperty<FObjectProperty>(videoCamera->GetClass(), "LightingTexture");
				auto mzprop = MZPropertyFactory::CreateProperty(videoCamera, texture, &(mzclient->RegisteredProperties));
				if (mzprop)
				{
					mzprop->PinShowAs = mz::fb::ShowAs::OUTPUT_PIN;
					pinsToSpawn.push_back(mzprop);
				}
			}
			{
				auto texture = FindFProperty<FObjectProperty>(videoCamera->GetClass(), "BloomTexture");
				auto mzprop = MZPropertyFactory::CreateProperty(videoCamera, texture, &(mzclient->RegisteredProperties));
				if (mzprop)
				{
					mzprop->PinShowAs = mz::fb::ShowAs::OUTPUT_PIN;
					pinsToSpawn.push_back(mzprop);
				}
			}
			{
				auto track = FindFProperty<FProperty>(videoCamera->GetClass(), "Track");
				auto mzprop = MZPropertyFactory::CreateProperty(videoCamera, track, &(mzclient->RegisteredProperties));
				if (mzprop)
				{
					mzprop->PinShowAs = mz::fb::ShowAs::INPUT_PIN;
					pinsToSpawn.push_back(mzprop);
				}
			}
						
			for (auto const& mzprop : pinsToSpawn)
			{
				mzprop->DisplayName = realityCamera->GetActorLabel() + " | " + mzprop->DisplayName;
				//mzclient->RegisteredProperties.Add(mzprop->Id, mzprop);
				mzprop->transient = false;
				mzclient->Pins.Add(mzprop->Id, mzprop);
				mzclient->SendPinAdded(mzclient->Client->NodeId, mzprop);
			}
					

		};
		CustomFunctions.Add(mzcf->Id, mzcf);
	}
	//Add Projection cube function
	{
		MZCustomFunction* mzcf = new MZCustomFunction;
		mzcf->Id = FGuid::NewGuid();
		mzcf->Serialize = [funcid = mzcf->Id](flatbuffers::FlatBufferBuilder& fbb)->flatbuffers::Offset<mz::fb::Node>
		{
			return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&funcid, "Spawn Reality Projection Cube", "UE5.UE5", false, true, 0, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(fbb, mz::fb::JobType::CPU).Union(), "UE5", 0, "ENGINE FUNCTIONS");
		};
		mzcf->Function = [mzclient = this](TMap<FGuid, std::vector<uint8>> properties)
		{
			FString actorName("RealityActor_ProjectionCube");

			TSoftClassPtr<AActor> ActorBpClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Script/Engine.Blueprint'/RealityEngine/Actors/RealityActor_ProjectionCube.RealityActor_ProjectionCube_C'")));

			UClass* LoadedBpAsset = ActorBpClass.LoadSynchronous();
			
			AActor* projectionCube = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(LoadedBpAsset);
			if (projectionCube)
			{
				mzclient->SendActorAdded(projectionCube, actorName);
				LOGF("Spawned actor %s", *projectionCube->GetFName().ToString());
			}
			else
			{
				return;
			}
			std::vector<TSharedPtr<MZProperty>> pinsToSpawn;
			{
				auto texture = FindFProperty<FObjectProperty>(LoadedBpAsset, "VideoInput");
				auto RenderTarget2D = NewObject<UTextureRenderTarget2D>(projectionCube);
				RenderTarget2D->InitAutoFormat(1920, 1080);
				texture->SetObjectPropertyValue_InContainer(projectionCube, RenderTarget2D);

				auto mzprop = MZPropertyFactory::CreateProperty(projectionCube, texture, &(mzclient->RegisteredProperties));
				if (mzprop)
				{
					mzprop->PinShowAs = mz::fb::ShowAs::INPUT_PIN;
					pinsToSpawn.push_back(mzprop);
				}
			}
			//{
			//	auto track = FindField<FProperty>(videoCamera->GetClass(), "Track");
			//	MZProperty* mzprop = MZPropertyFactory::CreateProperty(videoCamera, track, &(mzclient->RegisteredProperties));
			//	if (mzprop)
			//	{
			//		mzprop->PinShowAs = mz::fb::ShowAs::INPUT_PIN;
			//		pinsToSpawn.push_back(mzprop);
			//	}
			//}

			for (auto const& mzprop : pinsToSpawn)
			{
				mzprop->DisplayName = projectionCube->GetActorLabel() + " | " + mzprop->DisplayName;
				//mzclient->RegisteredProperties.Add(mzprop->Id, mzprop);
				mzprop->transient = false;
				mzclient->Pins.Add(mzprop->Id, mzprop);
				mzclient->SendPinAdded(mzclient->Client->NodeId, mzprop);
			}
					
		};
		CustomFunctions.Add(mzcf->Id, mzcf);
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
			FExecuteAction::CreateLambda([=]() {
				PopulateSceneTree();
				}));
		CommandList->MapAction(
			FMediaZPluginEditorCommands::Get().SendRootUpdate,
			FExecuteAction::CreateLambda([=](){
					SendNodeUpdate(Client->NodeId);
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
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedHandle);
#endif //WITH_EDITOR

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

    TryConnect();

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

void FMZClient::PopulateSceneTree(bool reset) //Runs in game thread
{
	if (reset)
	{
		Reset();
	}

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
				if (SceneTree.ChildMap.Contains(parent->GetActorGuid()))
				{
					SceneTree.ChildMap.Find(parent->GetActorGuid())->Add(*ActorItr);
				}
				else
				{
					SceneTree.ChildMap.FindOrAdd(parent->GetActorGuid()).Add(*ActorItr);
				}
				continue;
			}

			ActorsInScene.Add(*ActorItr);
			auto newNode = SceneTree.AddActor(ActorItr->GetFolder().GetPath().ToString(), *ActorItr);
			if (newNode)
			{
				newNode->actor = MZActorReference(*ActorItr);
			}
		}
	}
}

void FMZClient::Reset()
{
	MZTextureShareManager::GetInstance()->Reset();
	SceneTree.Clear();
	Pins.Empty();
	RegisteredProperties.Empty();
	PropertiesMap.Empty();
}

void FMZClient::SendNodeUpdate(FGuid nodeId)
{
	if (!IsConnected())
	{
		return;
	}

	if (nodeId == SceneTree.Root->Id)
	{
		MessageBuilder mb;
		std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = SceneTree.Root->SerializeChildren(mb);
		std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins;
		for (auto& [_, pin] : Pins)
		{
			graphPins.push_back(pin->Serialize(mb));
		}
		std::vector<flatbuffers::Offset<mz::fb::Node>> graphFunctions;
		for (auto& [_, cfunc] : CustomFunctions)
		{
			graphFunctions.push_back(cfunc->Serialize(mb));
		}

		auto msg = MakeAppEvent(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::ANY, 0, &graphPins, 0, &graphFunctions, 0, &graphNodes));
		Client->Write(msg);
		return;
	}

	auto val = SceneTree.NodeMap.Find(nodeId);
	TSharedPtr<TreeNode> treeNode = val ? *val : nullptr;
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
	auto msg = MakeAppEvent(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::ANY, 0, &graphPins, 0, &graphFunctions, 0, &graphNodes));
	Client->Write(msg);

	//Send list actors on the scene 
}

void FMZClient::SendPinValueChanged(FGuid propertyId, std::vector<uint8> data)
{
	if (!Client || !Client->NodeId.IsValid())
	{
		return;
	}

	MessageBuilder mb;
	auto msg = MakeAppEvent(mb, mz::CreatePinValueChangedDirect(mb, (mz::fb::UUID*)&propertyId, &data));
	Client->Write(msg);
}

void FMZClient::SendPinUpdate() //runs in game thread
{
	if (!Client || !Client->NodeId.IsValid())
	{
		return;
	}

	auto nodeId = Client->NodeId;

	MessageBuilder mb;
	std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins;
	for (auto& [_, pin] : Pins)
	{
		graphPins.push_back(pin->Serialize(mb));
	}
	auto msg = MakeAppEvent(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::CLEAR_PINS, 0, &graphPins, 0, 0, 0, 0));
	Client->Write(msg);
	
}

void FMZClient::SendActorAdded(AActor* actor, FString spawnTag) //runs in game thread
{
	TSharedPtr<ActorNode> newNode = nullptr;
	if (auto sceneParent = actor->GetSceneOutlinerParent())
	{
		if (SceneTree.NodeMap.Contains(sceneParent->GetActorGuid()))
		{
			auto parentNode = SceneTree.NodeMap.FindRef(sceneParent->GetActorGuid());
			newNode = SceneTree.AddActor(parentNode, actor);
			if (!newNode)
			{
				return;
			}
			if (!spawnTag.IsEmpty())
			{
				newNode->mzMetaData.Add("spawnTag", spawnTag);
			}
			if (!Client || !Client->NodeId.IsValid())
			{
				return;
			}
			MessageBuilder mb;
			std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = { newNode->Serialize(mb) };
			auto msg = MakeAppEvent(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&parentNode->Id, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, &graphNodes));
			Client->Write(msg);
		}
	}
	else
	{
		newNode = SceneTree.AddActor(actor->GetFolder().GetPath().ToString(), actor);
		if (!newNode)
		{
			return;
		}
		if (!spawnTag.IsEmpty())
		{
			newNode->mzMetaData.Add("spawnTag", spawnTag);
		}
		if (!Client || !Client->NodeId.IsValid())
		{
			return;
		}
		MessageBuilder mb;
		std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = { newNode->Serialize(mb) };
		auto msg = MakeAppEvent(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&Client->NodeId, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, &graphNodes));
		Client->Write(msg);
	}

	return;
}
void FMZClient::RemoveProperties(TSharedPtr<TreeNode> Node, 
	TSet<TSharedPtr<MZProperty>>& PinsToRemove, 
	TSet<TSharedPtr<MZProperty>>& PropertiesToRemove)
{
	if (auto componentNode = Node->GetAsSceneComponentNode())
	{
		for (auto& [id, pin] : Pins)
		{
			//if (pin->ComponentContainer.Get() == componentNode->sceneComponent)
			//{
			//	PinsToRemove.Add(pin);
			//}
			if (!pin->GetRawObjectContainer())
			{
				PinsToRemove.Add(pin);
			}
		}
		for (auto& prop : componentNode->Properties)
		{
			PropertiesToRemove.Add(prop);
			RegisteredProperties.Remove(prop->Id);
			PropertiesMap.Remove(prop->Property);
		}
	}
	else if (auto actorNode = Node->GetAsActorNode())
	{
		for (auto& [id, pin] : Pins)
		{
			//if (pin->ActorContainer.Get() == actorNode->actor)
			//{
			//	PinsToRemove.Add(pin);
			//}
			if (!pin->GetRawObjectContainer())
			{
				PinsToRemove.Add(pin);
			}
		}
		for (auto& prop : actorNode->Properties)
		{
			PropertiesToRemove.Add(prop);
			RegisteredProperties.Remove(prop->Id);
			PropertiesMap.Remove(prop->Property);

		}
	}
	for (auto& child : Node->Children)
	{
		RemoveProperties(child, PinsToRemove, PropertiesToRemove);
	}
}

void FMZClient::CheckPins(TSet<UObject*>& RemovedObjects, 
	TSet<TSharedPtr<MZProperty>> &PinsToRemove,
	TSet<TSharedPtr<MZProperty>> &PropertiesToRemove)
{
	for (auto& [id, pin] : Pins)
	{
		UObject* container = pin->GetRawObjectContainer();
		if (!container)
		{
			continue;
		}
		if (RemovedObjects.Contains(container))
		{
			PinsToRemove.Add(pin);
		}
	}
}

void FMZClient::SendActorDeleted(FGuid Id, TSet<UObject*>& RemovedObjects) //runs in game thread
{
	if (SceneTree.NodeMap.Contains(Id))
	{
		auto node = SceneTree.NodeMap.FindRef(Id);
		//delete properties
		// can be optimized by using raw pointers
		TSet<TSharedPtr<MZProperty>> pinsToRemove;
		TSet<TSharedPtr<MZProperty>> propertiesToRemove;
		RemoveProperties(node, pinsToRemove, propertiesToRemove);
		CheckPins(RemovedObjects, pinsToRemove, propertiesToRemove);

		std::set<MZProperty*> removedTextures;
		
		for (auto prop : pinsToRemove)
		{
			if (auto objProp = CastField<FObjectProperty>(prop->Property))
			{
				UObject* container = prop->GetRawObjectContainer();
				if (!container)
				{
					continue;
				}
				if (auto URT = Cast<UTextureRenderTarget2D>(objProp->GetObjectPropertyValue(objProp->ContainerPtrToValuePtr<UTextureRenderTarget2D>(container))))
				{
					removedTextures.insert(prop.Get());
				}
			}
		}
		auto texman = MZTextureShareManager::GetInstance();

		for (auto prop : removedTextures)
		{
			texman->TextureDestroyed(prop);
		}

		auto tmp = pinsToRemove;
		for (auto pin : tmp)
		{
			Pins.Remove(pin->Id);
			RegisteredProperties.Remove(pin->Id);
		}
		
		//delete from parent
		FGuid parentId = Client->NodeId;
		if (auto parent = node->Parent)
		{
			parentId = parent->Id;
			auto v = parent->Children;
			auto it = std::find(v.begin(), v.end(), node);
			if (it != v.end())
				v.erase(it);
		}
		//delete from map
		SceneTree.NodeMap.Remove(node->Id);

		if (!Client || !Client->NodeId.IsValid())
		{
			return;
		}

		MessageBuilder mb;
		std::vector<mz::fb::UUID> graphNodes = { *(mz::fb::UUID*)&node->Id };
		auto msg = MakeAppEvent(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&parentId, mz::ClearFlags::NONE, 0, 0, 0, 0, &graphNodes, 0));
		Client->Write(msg);

		if (!pinsToRemove.IsEmpty())
		{
			std::vector<mz::fb::UUID> pinsToDelete;
			for (auto pin : pinsToRemove)
			{
				pinsToDelete.push_back(*(mz::fb::UUID*)&pin->Id);
			}
			auto msgg = MakeAppEvent(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&Client->NodeId, mz::ClearFlags::NONE, &pinsToDelete, 0, 0, 0, 0, 0));
			Client->Write(msgg);
		}

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
				SendNodeUpdate(Client->NodeId);
			}
		});
}

void FMZClient::HandleEndPIE(bool bIsSimulating)
{
	LOG("PLAY SESSION IS ENDING");
	
	PopulateSceneTree();
	if (Client)
	{
		SendNodeUpdate(Client->NodeId);
	}

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

void FMZClient::OnPinShowAsChanged(FGuid pinId, mz::fb::ShowAs newShowAs)
{
	TaskQueue.Enqueue([this, pinId, newShowAs]()
		{
			if (Pins.Contains(pinId))
			{
				auto mzprop = Pins.FindRef(pinId);
				mzprop->PinShowAs = newShowAs;
				SendPinUpdate();
			}
			else if(RegisteredProperties.Contains(pinId))
			{
				
				auto mzprop = RegisteredProperties.FindRef(pinId);
				UObject* container = mzprop->GetRawObjectContainer();
				if (!container)
				{
					return;
				}
				auto newmzprop = MZPropertyFactory::CreateProperty(container, mzprop->Property, &(RegisteredProperties));
				if (newmzprop)
				{
					//memcpy(newmzprop, mzprop, sizeof(MZProperty));
					newmzprop->PinShowAs = newShowAs;
					//RegisteredProperties.Remove(newmzprop->Id);
					//newmzprop->Id = FGuid::NewGuid();
					newmzprop->transient = false;
					Pins.Add(newmzprop->Id, newmzprop);
					//RegisteredProperties.Add(newmzprop->Id, newmzprop);
					SendPinUpdate();
				}
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
				mzcf->Function(properties);
			}
			else if (RegisteredFunctions.Contains(funcId))
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
						mzprop->SetPropValue((void*)val.data(), val.size(),Parms);
					}
				}

				mzfunc->Invoke();

				for (auto mzprop : mzfunc->OutProperties)
				{
					SendPinValueChanged(mzprop->Id, mzprop->UpdatePinValue(Parms));
				}
				//for (TFieldIterator<FProperty> It(mzfunc->Function); It && It->HasAnyPropertyFlags(CPF_OutParm); ++It)
				//{
				//	SendPinValueChanged(It->)
				//}

				mzfunc->Parameters = nullptr;


			}
		});
}

void FMZClient::OnContexMenuFired(FGuid itemId, FVector2D pos, uint32 instigator)
{
	if (SceneTree.NodeMap.Contains(itemId))
	{
		if (auto actorNode = SceneTree.NodeMap.FindRef(itemId)->GetAsActorNode())
		{
			if (!Client || !Client->NodeId.IsValid())
			{
				return;
			}
			MessageBuilder mb;
			//auto deleteAction = 
			//std::vector<flatbuffers::Offset<mz::ContextMenuItem>> actions = { mz::CreateContextMenuItemDirect(mb, "Destroy", 0, 0) };
			std::vector<flatbuffers::Offset<mz::ContextMenuItem>> actions = menuActions.SerializeActorMenuItems(mb);
			auto posx = mz::fb::vec2(pos.X, pos.Y);
			auto msg = MakeAppEvent(mb, mz::CreateContextMenuUpdateDirect(mb, (mz::fb::UUID*)&itemId, &posx, instigator, &actions));
			Client->Write(msg);
		}
	}
}

void FMZClient::OnContexMenuActionFired(FGuid itemId, uint32 actionId)
{
	if (SceneTree.NodeMap.Contains(itemId))
	{
		if (auto actorNode = SceneTree.NodeMap.FindRef(itemId)->GetAsActorNode())
		{
			TaskQueue.Enqueue([this, actor = actorNode->actor.Get(), actionId]()
				{
					if (!actor)
					{
						return;
					}
					menuActions.ExecuteActorAction(actionId, actor);
				});
		}
	}
}

void FMZClient::OnUpdatedNodeExecuted(TMap<FGuid, std::vector<uint8>> updates)
{
	TaskQueue.Enqueue([this, updates]()
	{
		for (auto& [id, data] : updates)
		{
			if (RegisteredProperties.Contains(id))
			{
				auto mzprop = RegisteredProperties.FindRef(id);
				mzprop->SetPropValue((void*)data.data(), data.size());
			}
		}

	});
	if (MZTimeStep)
	{
		MZTimeStep->Step();
	}
}

void FMZClient::SendPinAdded(FGuid nodeId, TSharedPtr<MZProperty> const& mzprop)
{
	if (!Client || !Client->NodeId.IsValid())
	{
		return;
	}
	MessageBuilder mb;
	std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins = {mzprop->Serialize(mb)};
	auto msg = MakeAppEvent(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::NONE, 0, &graphPins, 0, 0, 0, 0));
	Client->Write(msg);
	return;
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
	auto val = SceneTree.NodeMap.Find(nodeId);
	TSharedPtr<TreeNode> treeNode = val ? *val : nullptr;

	if (!treeNode || !treeNode->NeedsReload)
	{
		return false;
	}
	if (treeNode->GetAsActorNode())
	{
		auto actorNode = StaticCastSharedPtr<ActorNode>(treeNode);
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
			auto mzprop = MZPropertyFactory::CreateProperty(actorNode->actor.Get(), AProperty, &(RegisteredProperties), &(PropertiesMap));
			if (!mzprop)
			{
				AProperty = AProperty->PropertyLinkNext;
				continue;
			}
			//RegisteredProperties.Add(mzprop->Id, mzprop);
			actorNode->Properties.push_back(mzprop);

			for (auto it : mzprop->childProperties)
			{
				//RegisteredProperties.Add(it->Id, it);
				actorNode->Properties.push_back(it);
			}

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
				auto mzprop = MZPropertyFactory::CreateProperty(Component, Property, &(RegisteredProperties), &(PropertiesMap));
				if (mzprop)
				{
					//RegisteredProperties.Add(mzprop->Id, mzprop);
					actorNode->Properties.push_back(mzprop);

					for (auto it : mzprop->childProperties)
					{
						//RegisteredProperties.Add(it->Id, it);
						actorNode->Properties.push_back(it);
					}

				}
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
				
				TSharedPtr<MZFunction> mzfunc(new MZFunction(actorNode->actor.Get(), UEFunction));
				
				// Parse all function parameters.

				for (TFieldIterator<FProperty> PropIt(UEFunction); PropIt && PropIt->HasAnyPropertyFlags(CPF_Parm); ++PropIt)
				{
					auto mzprop = MZPropertyFactory::CreateProperty(nullptr, *PropIt, &(RegisteredProperties), &(PropertiesMap));
					if (mzprop)
					{
						mzfunc->Properties.push_back(mzprop);
						//RegisteredProperties.Add(mzprop->Id, mzprop);			
						if (PropIt->HasAnyPropertyFlags(CPF_OutParm))
						{
							mzfunc->OutProperties.push_back(mzprop);
						}
					}
				}
				
				actorNode->Functions.push_back(mzfunc);
				RegisteredFunctions.Add(mzfunc->Id, mzfunc);
			}
		}
		//ITERATE FUNCTIONS END

		//ITERATE CHILD COMPONENTS TO SHOW BEGIN
		actorNode->Children.clear();

		auto unattachedChildsPtr = SceneTree.ChildMap.Find(actorNode->Id);
		TSet<AActor*> unattachedChilds = unattachedChildsPtr ? *unattachedChildsPtr : TSet<AActor*>();
		for (auto child : unattachedChilds)
		{
			SceneTree.AddActor(actorNode, child);
		}

		AActor* ActorContext = actorNode->actor.Get();
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

		TArray<TSharedPtr<SceneComponentNode>> OutArray;

		TFunction<void(USceneComponent*, TSharedPtr<TreeNode>)> AddInstancedComponentsRecursive = [&, this](USceneComponent* Component, TSharedPtr<TreeNode> ParentHandle)
		{
			if (Component != nullptr)
			{
				for (USceneComponent* ChildComponent : Component->GetAttachChildren())
				{
					if (ComponentsToAdd.Contains(ChildComponent) && ChildComponent->GetOwner() == Component->GetOwner())
					{
						ComponentsToAdd.Remove(ChildComponent);
						TSharedPtr<SceneComponentNode> NewParentHandle = nullptr;
						if (ParentHandle->GetAsActorNode())
						{
							// TODO: TSharedFromThis
							auto ParentAsActorNode = StaticCastSharedPtr<ActorNode>(ParentHandle);
							NewParentHandle = this->SceneTree.AddSceneComponent(ParentAsActorNode, ChildComponent);
						}
						else if (ParentHandle->GetAsSceneComponentNode())
						{
							auto ParentAsSceneComponentNode = StaticCastSharedPtr<SceneComponentNode>(ParentHandle);
							NewParentHandle = this->SceneTree.AddSceneComponent(ParentAsSceneComponentNode, ChildComponent);
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
			auto RootHandle = SceneTree.AddSceneComponent(actorNode, RootComponent);
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
			//OutArray.Add(SceneTree.AddSceneComponent(componentNode, ActorComp)); //TODO scene tree add actor components
		}
		//ITERATE CHILD COMPONENTS TO SHOW END

		treeNode->NeedsReload = false;
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
				
			auto mzprop = MZPropertyFactory::CreateProperty(Component.Get(), Property, &(RegisteredProperties), &(PropertiesMap));
			if (mzprop)
			{
				//RegisteredProperties.Add(mzprop->Id, mzprop);
				treeNode->GetAsSceneComponentNode()->Properties.push_back(mzprop);

				for (auto it : mzprop->childProperties)
				{
					//RegisteredProperties.Add(it->Id, it);
					treeNode->GetAsSceneComponentNode()->Properties.push_back(it);
				}

			}
		}
		treeNode->NeedsReload = false;
		return true;
	}
	return false;
}

void FMZClient::SendAssetList()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray< FString > ContentPaths;
	ContentPaths.Add(TEXT("/Game"));
	ContentPaths.Add(TEXT("/Script"));
	ContentPaths.Add(TEXT("/mediaz"));
	ContentPaths.Add(TEXT("/RealityEngine"));
	//ContentPaths.Add(TEXT("/All"));
	AssetRegistryModule.Get().ScanPathsSynchronous(ContentPaths);
	//AssetRegistryModule.Get().WaitForCompletion(); // wait in startup to completion of the scan
	
	//FName BaseClassName = AActor::StaticClass()->GetFName();
	//TSet< FName > DerivedNames;
	//{
	//	TArray< FName > BaseNames;
	//	BaseNames.Add(BaseClassName);

	//	TSet< FName > Excluded;
	//	AssetRegistryModule.Get().GetDerivedClassNames(BaseNames, Excluded, DerivedNames);
	//}
	FTopLevelAssetPath BaseClassName = FTopLevelAssetPath(AActor::StaticClass());
	TSet< FTopLevelAssetPath > DerivedAssetPaths;
	{
		TArray< FTopLevelAssetPath > BaseNames;
		BaseNames.Add(BaseClassName);

		TSet< FTopLevelAssetPath > Excluded;
		AssetRegistryModule.Get().GetDerivedClassNames(BaseNames, Excluded, DerivedAssetPaths);
	}
	TSet< FName > DerivedNames;
	for (auto assetPath : DerivedAssetPaths)
	{
		DerivedNames.Add(assetPath.GetAssetName());
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

void UENodeStatusHandler::SetClient(ClientImpl* GrpcClient)
{
	this->Client = GrpcClient;
}

void UENodeStatusHandler::Add(std::string const& Id, mz::fb::TNodeStatusMessage const& Status)
{
	StatusMessages[Id] = Status;
	SendStatus();
}

void UENodeStatusHandler::Remove(std::string const& Id)
{
	auto it = StatusMessages.find(Id);
	if (it != StatusMessages.end())
	{
		StatusMessages.erase(it);
		SendStatus();
	}
}

void UENodeStatusHandler::SendStatus() const
{
	if (!(Client && Client->NodeId.IsValid()) )
		return;
	flatbuffers::grpc::MessageBuilder Builder;
	mz::TPartialNodeUpdate UpdateRequest;
	UpdateRequest.node_id = std::make_unique<mz::fb::UUID>(*((mz::fb::UUID*)&Client->NodeId));
	for (auto& [_, StatusMsg] : StatusMessages)
	{
		UpdateRequest.status_messages.push_back(std::make_unique<mz::fb::TNodeStatusMessage>(StatusMsg));
	}
	auto Message = MakeAppEvent(Builder, mz::CreatePartialNodeUpdate(Builder, &UpdateRequest));
	Client->Write(Message);
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
	flatbuffers::grpc::MessageBuilder Builder;
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

//
//#include "DispelUnrealMadnessPostlude.h"


#else
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MZClient);
#endif

