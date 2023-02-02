//mediaz plugin includes
#include "MZSceneTreeManager.h"
#include "MZClient.h"
#include "MZTextureShareManager.h"
#include "MZUMGRenderer.h"
#include "MZUMGRendererComponent.h"
#include "MZUMGRenderManager.h"
#include "MZAssetManager.h"
#include "MZViewportManager.h"
#include "MZViewportClient.h"

//unreal engine includes
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Kismet/GameplayStatics.h"
#include "EditorCategoryUtils.h"
#include "ObjectEditorUtils.h"
#include "HardwareInfo.h"

//#define VIEWPORT_TEXTURE

static const FName NAME_Reality_FolderName(TEXT("Reality Actors"));

IMPLEMENT_MODULE(FMZSceneTreeManager, MZSceneTreeManager)

UWorld* FMZSceneTreeManager::daWorld = nullptr;

template<typename T>
inline const T& FinishBuffer(flatbuffers::FlatBufferBuilder& builder, flatbuffers::Offset<T> const& offset)
{
	builder.Finish(offset);
	auto buf = builder.Release();
	return *flatbuffers::GetRoot<T>(buf.data());
}

TMap<FGuid, std::vector<uint8>> ParsePins(mz::fb::Node const& archive)
{
	TMap<FGuid, std::vector<uint8>> re;
	if (!flatbuffers::IsFieldPresent(&archive, mz::fb::Node::VT_PINS))
	{
		return re;
	}

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
	if (!flatbuffers::IsFieldPresent(archive, mz::fb::Node::VT_PINS))
	{
		return re;
	}
	for (auto pin : *(archive->pins()))
	{
		re.Add(*(FGuid*)pin->id()->bytes()->Data(), pin);
	}
	return re;
}


FMZSceneTreeManager::FMZSceneTreeManager()
{

}
void FMZSceneTreeManager::OnMapChange(uint32 MapFlags)
{
	FString WorldName = GEditor->GetEditorWorldContext().World()->GetMapName();
	UE_LOG(LogTemp, Warning, TEXT("OnMapChange with editor world contexts world %s"), *WorldName);
	FMZSceneTreeManager::daWorld = GEditor->GetEditorWorldContext().World();
}

void FMZSceneTreeManager::OnNewCurrentLevel()
{
	FString WorldName = GEditor->GetEditorWorldContext().World()->GetMapName();
	UE_LOG(LogTemp, Warning, TEXT("OnNewCurrentLevel with editor world contexts world %s"), *WorldName);
	//todo we may need to fill these according to the level system
}

void FMZSceneTreeManager::StartupModule()
{
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

	MZClient = &FModuleManager::LoadModuleChecked<FMZClient>("MZClient");
	MZAssetManager = &FModuleManager::LoadModuleChecked<FMZAssetManager>("MZAssetManager");
	MZViewportManager = &FModuleManager::LoadModuleChecked<FMZViewportManager>("MZViewportManager");

	MZPropertyManager.MZClient = MZClient;

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMZSceneTreeManager::Tick));
	MZActorManager = new FMZActorManager(SceneTree);
	//Bind to MediaZ events
	MZClient->OnMZNodeSelected.AddRaw(this, &FMZSceneTreeManager::OnMZNodeSelected);
	MZClient->OnMZConnected.AddRaw(this, &FMZSceneTreeManager::OnMZConnected);
	MZClient->OnMZNodeUpdated.AddRaw(this, &FMZSceneTreeManager::OnMZNodeUpdated);
	MZClient->OnMZConnectionClosed.AddRaw(this, &FMZSceneTreeManager::OnMZConnectionClosed);
	MZClient->OnMZPinValueChanged.AddRaw(this, &FMZSceneTreeManager::OnMZPinValueChanged);
	MZClient->OnMZPinShowAsChanged.AddRaw(this, &FMZSceneTreeManager::OnMZPinShowAsChanged);
	MZClient->OnMZFunctionCalled.AddRaw(this, &FMZSceneTreeManager::OnMZFunctionCalled);
	MZClient->OnMZExecutedApp.AddRaw(this, &FMZSceneTreeManager::OnMZExecutedApp);
	MZClient->OnMZContextMenuRequested.AddRaw(this, &FMZSceneTreeManager::OnMZContextMenuRequested);
	MZClient->OnMZContextMenuCommandFired.AddRaw(this, &FMZSceneTreeManager::OnMZContextMenuCommandFired);
	MZClient->OnMZNodeImported.AddRaw(this, &FMZSceneTreeManager::OnMZNodeImported);

	FEditorDelegates::PostPIEStarted.AddRaw(this, &FMZSceneTreeManager::HandleBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FMZSceneTreeManager::HandleEndPIE);
	FEditorDelegates::NewCurrentLevel.AddRaw(this, &FMZSceneTreeManager::OnNewCurrentLevel);
	FEditorDelegates::MapChange.AddRaw(this, &FMZSceneTreeManager::OnMapChange);

	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FMZSceneTreeManager::OnPropertyChanged);

	FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FMZSceneTreeManager::OnPostWorldInit);
	FWorldDelegates::OnPreWorldFinishDestroy.AddRaw(this, &FMZSceneTreeManager::OnPreWorldFinishDestroy);
	
	UMZViewportClient::MZViewportDestroyedDelegate.AddRaw(this, &FMZSceneTreeManager::DisconnectViewportTexture);
#ifdef VIEWPORT_TEXTURE
	//custom pins
	{
		auto mzprop = MZPropertyFactory::CreateProperty(nullptr, UMZViewportClient::StaticClass()->FindPropertyByName("ViewportTexture"));
		mzprop->PinShowAs = mz::fb::ShowAs::INPUT_PIN;
		CustomProperties.Add(mzprop->Id, mzprop);
		ViewportTextureProperty = mzprop.Get();
	}
#endif

	//custom functions 
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
				mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&actorPinId, TCHAR_TO_ANSI(TEXT("Actor List")), TCHAR_TO_ANSI(TEXT("string")), mz::fb::ShowAs::PROPERTY, mz::fb::CanShowAs::PROPERTY_ONLY, "UE PROPERTY", mz::fb::CreateVisualizerDirect(fbb, mz::fb::VisualizerType::COMBO_BOX, "UE5_ACTOR_LIST"), &data, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  mz::fb::PinContents::JobPin),
			};
			return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&funcid, "Spawn Actor", "UE5.UE5", false, true, &spawnPins, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(fbb, mz::fb::JobType::CPU).Union(), "UE5", 0, "ENGINE FUNCTIONS");
		};
		mzcf->Function = [this, actorPinId](TMap<FGuid, std::vector<uint8>> properties)
		{
			FString SpawnTag((char*)properties.FindRef(actorPinId).data());
			AActor* SpawnedActor = MZActorManager->SpawnActor(SpawnTag);
		};
		CustomFunctions.Add(mzcf->Id, mzcf);
	}
	//Add Camera function
	{
		MZAssetManager->CustomSpawns.Add("CustomRealityCamera", [this]()
			{
				AActor* realityCamera = MZAssetManager->SpawnFromAssetPath(FTopLevelAssetPath("/Script/Engine.Blueprint'/RealityEngine/Actors/Reality_Camera.Reality_Camera_C'"));

				return realityCamera;
			});

		MZCustomFunction* mzcf = new MZCustomFunction;
		mzcf->Id = FGuid::NewGuid();
		mzcf->Serialize = [funcid = mzcf->Id](flatbuffers::FlatBufferBuilder& fbb)->flatbuffers::Offset<mz::fb::Node>
		{
			return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&funcid, "Spawn Reality Camera", "UE5.UE5", false, true, 0, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(fbb, mz::fb::JobType::CPU).Union(), "UE5", 0, "ENGINE FUNCTIONS");
		};
		mzcf->Function = [this](TMap<FGuid, std::vector<uint8>> properties)
		{

			AActor* realityCamera = MZActorManager->SpawnActor("CustomRealityCamera");
			if (!realityCamera || !SceneTree.NodeMap.Contains(realityCamera->GetActorGuid()))
			{
				return;
			}

			auto cameraNode = SceneTree.NodeMap.FindRef(realityCamera->GetActorGuid());

			PopulateNode(cameraNode->Id);
			SendNodeUpdate(cameraNode->Id);
			for (auto ChildNode : cameraNode->Children)
			{
				PopulateNode(ChildNode->Id);
				SendNodeUpdate(ChildNode->Id);
			}
			//SendNodeUpdate(cameraNode->Id);

			auto videoCamera = FindObject<USceneComponent>(realityCamera, TEXT("VideoCamera"));
			auto FrameTexture = FindFProperty<FObjectProperty>(videoCamera->GetClass(), "FrameTexture");
			auto MaskTexture = FindFProperty<FObjectProperty>(videoCamera->GetClass(), "MaskTexture");
			auto LightingTexture = FindFProperty<FObjectProperty>(videoCamera->GetClass(), "LightingTexture");
			auto BloomTexture = FindFProperty<FObjectProperty>(videoCamera->GetClass(), "BloomTexture");
			auto Track = FindFProperty<FProperty>(videoCamera->GetClass(), "Track");

			MZPropertyManager.CreatePortal(FrameTexture, mz::fb::ShowAs::OUTPUT_PIN);
			MZPropertyManager.CreatePortal(MaskTexture, mz::fb::ShowAs::OUTPUT_PIN);
			MZPropertyManager.CreatePortal(LightingTexture, mz::fb::ShowAs::OUTPUT_PIN);
			MZPropertyManager.CreatePortal(BloomTexture, mz::fb::ShowAs::OUTPUT_PIN);
			MZPropertyManager.CreatePortal(Track, mz::fb::ShowAs::INPUT_PIN);
			


		};
		CustomFunctions.Add(mzcf->Id, mzcf);
	}
	//Add Projection cube function
	{
		MZAssetManager->CustomSpawns.Add("CustomProjectionCube", [this]()
			{
				AActor* projectionCube = MZAssetManager->SpawnFromAssetPath(FTopLevelAssetPath("/Script/Engine.Blueprint'/RealityEngine/Actors/RealityActor_ProjectionCube.RealityActor_ProjectionCube_C'"));

				return projectionCube;
			});
		MZCustomFunction* mzcf = new MZCustomFunction;
		mzcf->Id = FGuid::NewGuid();
		mzcf->Serialize = [funcid = mzcf->Id](flatbuffers::FlatBufferBuilder& fbb)->flatbuffers::Offset<mz::fb::Node>
		{
			return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&funcid, "Spawn Reality Projection Cube", "UE5.UE5", false, true, 0, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(fbb, mz::fb::JobType::CPU).Union(), "UE5", 0, "ENGINE FUNCTIONS");
		};
		mzcf->Function = [this](TMap<FGuid, std::vector<uint8>> properties)
		{

			AActor* projectionCube = MZActorManager->SpawnActor("CustomProjectionCube");
			if (!projectionCube || !SceneTree.NodeMap.Contains(projectionCube->GetActorGuid()))
			{
				return;
			}
			auto projectionNode = SceneTree.NodeMap.FindRef(projectionCube->GetActorGuid());

			PopulateNode(projectionNode->Id);
			SendNodeUpdate(projectionNode->Id);
			for (auto ChildNode : projectionNode->Children)
			{
				PopulateNode(ChildNode->Id);
				SendNodeUpdate(ChildNode->Id);
			}

			auto InputTexture = FindFProperty<FObjectProperty>(projectionCube->GetClass(), "VideoInput");
			MZPropertyManager.CreatePortal(InputTexture, mz::fb::ShowAs::INPUT_PIN);

		};
		CustomFunctions.Add(mzcf->Id, mzcf);
	}
	//add umg renderer function
	{
		MZCustomFunction* mzcf = new MZCustomFunction;
		mzcf->Id = FGuid::NewGuid();
		FGuid actorPinId = FGuid::NewGuid();
		mzcf->Params.Add(actorPinId, "UMG to spawn");
		mzcf->Serialize = [funcid = mzcf->Id, actorPinId](flatbuffers::FlatBufferBuilder& fbb)->flatbuffers::Offset<mz::fb::Node>
		{
			//todo remove unneccessary code
			FString val("");
			auto s = StringCast<ANSICHAR>(*val);
			auto data = std::vector<uint8_t>(s.Length() + 1, 0);
			memcpy(data.data(), s.Get(), s.Length());
			std::vector<flatbuffers::Offset<mz::fb::Pin>> spawnPins = {
				mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&actorPinId, TCHAR_TO_ANSI(TEXT("UMG to spawn")), TCHAR_TO_ANSI(TEXT("string")), mz::fb::ShowAs::PROPERTY, mz::fb::CanShowAs::PROPERTY_ONLY, "UE PROPERTY", mz::fb::CreateVisualizerDirect(fbb, mz::fb::VisualizerType::COMBO_BOX, "UE5_UMG_LIST"), &data, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  mz::fb::PinContents::JobPin),
			};
return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&funcid, "Spawn UMG Renderer", "UE5.UE5", false, true, &spawnPins, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(fbb, mz::fb::JobType::CPU).Union(), "UE5", 0, "ENGINE FUNCTIONS");
		};
		mzcf->Function = [this, actorPinId](TMap<FGuid, std::vector<uint8>> properties)
		{
			FString umgName((char*)properties.FindRef(actorPinId).data());
			static AActor* UMGManager = nullptr;
			if (!IsValid(UMGManager))
			{
				UMGManager = nullptr;
			}
			//check if manager already present in the scene
			if (!UMGManager)
			{
				TArray<AActor*> FoundActors;
				UGameplayStatics::GetAllActorsOfClass(GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World(), AMZUMGRenderManager::StaticClass(), FoundActors);

				if (!FoundActors.IsEmpty())
				{
					UMGManager = FoundActors[0];
				}
			}
			if (!UMGManager)
			{
				UMGManager = MZActorManager->SpawnActor(AMZUMGRenderManager::StaticClass());

				UMGManager->Rename(*MakeUniqueObjectName(nullptr, AActor::StaticClass(), FName("MZUMGRenderManager")).ToString());
				UMGManager->SetActorLabel(TEXT("MZUMGRenderManager"));

				USceneComponent* newRoot = NewObject<USceneComponent>(UMGManager);
				newRoot->Rename(TEXT("UMGs"));
				UMGManager->SetRootComponent(newRoot);
				newRoot->CreationMethod = EComponentCreationMethod::Instance;
				newRoot->RegisterComponent();
				UMGManager->AddInstanceComponent(newRoot);

			}

			std::vector<TSharedPtr<MZProperty>> pinsToSpawn;
			if (UMGManager)
			{
				UUserWidget* newWidget = MZAssetManager->CreateUMGFromTag(umgName);
				if (newWidget)
				{
					UMZUMGRendererComponent* NewRendererComp = NewObject<UMZUMGRendererComponent>(UMGManager);
					NewRendererComp->Widget = newWidget;

					NewRendererComp->SetupAttachment(UMGManager->GetRootComponent());
					NewRendererComp->CreationMethod = EComponentCreationMethod::Instance;
					NewRendererComp->RegisterComponent();
					UMGManager->AddInstanceComponent(NewRendererComp);

					if (!SceneTree.NodeMap.Contains(UMGManager->GetActorGuid()))
					{
						return;
					}
					auto umgManagerNode = SceneTree.NodeMap.FindRef(UMGManager->GetActorGuid());


					PopulateNode(umgManagerNode->Id);
					SendNodeUpdate(umgManagerNode->Id);
					for (auto ChildNode : umgManagerNode->Children)
					{
						for (auto UMGS : ChildNode->Children)
						{
							PopulateNode(UMGS->Id);
							SendNodeUpdate(UMGS->Id);
						}
						PopulateNode(ChildNode->Id);
						SendNodeUpdate(ChildNode->Id);
					}

					auto OutputTexture = FindFProperty<FObjectProperty>(NewRendererComp->GetClass(), "UMGRenderTarget");
					MZPropertyManager.CreatePortal(OutputTexture, mz::fb::ShowAs::OUTPUT_PIN);

				}
			}

		};
		CustomFunctions.Add(mzcf->Id, mzcf);
	}

}

void FMZSceneTreeManager::ShutdownModule()
{

}

bool FMZSceneTreeManager::Tick(float dt)
{
	if (MZClient)
	{
		MZTextureShareManager::GetInstance()->EnqueueCommands(MZClient->AppServiceClient.Get());
	}

	return true;
}

void FMZSceneTreeManager::OnMZConnected(mz::fb::Node const& appNode)
{
	SceneTree.Root->Id = *(FGuid*)appNode.id();
	RescanScene();
	SendNodeUpdate(FMZClient::NodeId);
}

void FMZSceneTreeManager::OnMZNodeUpdated(mz::fb::Node const& appNode)
{
	//todo fix LOG("Node update from mediaz");
	if (FMZClient::NodeId != SceneTree.Root->Id)
	{
		SceneTree.Root->Id = *(FGuid*)appNode.id();
		RescanScene();
		SendNodeUpdate(FMZClient::NodeId);
	}
	auto texman = MZTextureShareManager::GetInstance();
	std::unique_lock lock1(texman->PendingCopyQueueMutex);
	for (auto& [id, pin] : ParsePins(&appNode))
	{
		if (texman->PendingCopyQueue.Contains(id))
		{
			auto mzprop = texman->PendingCopyQueue.FindRef(id);
			texman->UpdateTexturePin(mzprop, flatbuffers::GetRoot<mz::fb::Texture>(pin->data()->Data()));
		}
	}
}

void FMZSceneTreeManager::OnMZNodeSelected(mz::fb::UUID const& nodeId)
{
	//todo fix logs
	UE_LOG(LogTemp, Warning, TEXT("Node with id bla bla is selected and got this with an event broadcast"));

	FGuid id = *(FGuid*)&nodeId;
	if (PopulateNode(id))
	{
		SendNodeUpdate(id);
	}
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
		IsValidChecked(Actor);// &&																// We don't want to show actors that are about to go away
		//!Actor->IsHidden();
}

void FMZSceneTreeManager::OnMZConnectionClosed()
{
}

void FMZSceneTreeManager::OnMZPinValueChanged(mz::fb::UUID const& pinId, uint8_t const* data, size_t size)
{
	FGuid Id = *(FGuid*)&pinId;
	if (CustomProperties.Contains(Id))
	{
		auto mzprop = CustomProperties.FindRef(Id);
		std::vector<uint8_t> copy(size, 0);
		memcpy(copy.data(), data, size);

		mzprop->SetPropValue((void*)copy.data(), size);
		return;
	}
	SetPropertyValue(Id, (void*)data, size);
}

void FMZSceneTreeManager::OnMZPinShowAsChanged(mz::fb::UUID const& Id, mz::fb::ShowAs newShowAs)
{
	FGuid pinId = *(FGuid*)&Id;
	if (CustomProperties.Contains(pinId))
	{
		UE_LOG(LogTemp, Warning, TEXT("Custom Property ShowAs changed."));
	}
	else if (MZPropertyManager.PropertiesById.Contains(pinId))
	{
		MZPropertyManager.CreatePortal(pinId, newShowAs);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Property with given id is not found."));
	}
}

void FMZSceneTreeManager::OnMZFunctionCalled(mz::fb::UUID const& nodeId, mz::fb::Node const& function)
{
	FGuid funcId = *(FGuid*)function.id();
	TMap<FGuid, std::vector<uint8>> properties = ParsePins(function);

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
			if (MZPropertyManager.PropertiesById.Contains(id))
			{
				auto mzprop = MZPropertyManager.PropertiesById.FindRef(id);
				mzprop->SetPropValue((void*)val.data(), val.size(), Parms);
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
}

void FMZSceneTreeManager::OnMZExecutedApp(mz::app::AppExecute const& appExecute)
{
	if (!flatbuffers::IsFieldPresent(&appExecute, mz::app::AppExecute::VT_PINVALUEUPDATES))
	{
		return;
	}
	for (auto update : *appExecute.PinValueUpdates())
	{
		auto id = *(FGuid*)update->pin_id()->bytes()->Data();

		if (MZPropertyManager.PropertiesById.Contains(id))
		{
			auto mzprop = MZPropertyManager.PropertiesById.FindRef(id);
			mzprop->SetPropValue((void*)update->value()->data(), update->value()->size());
		}
	}
}

void FMZSceneTreeManager::OnMZContextMenuRequested(mz::ContextMenuRequest const& request)
{
	FVector2D pos(request.pos()->x(), request.pos()->y());
	FGuid itemId = *(FGuid*)request.item_id();
	uint32 instigator = request.instigator();

	if (SceneTree.NodeMap.Contains(itemId))
	{
		if (auto actorNode = SceneTree.NodeMap.FindRef(itemId)->GetAsActorNode())
		{
			if (!MZClient->IsConnected())
			{
				return;
			}
			flatbuffers::FlatBufferBuilder mb;
			std::vector<flatbuffers::Offset<mz::ContextMenuItem>> actions = menuActions.SerializeActorMenuItems(mb);
			auto posx = mz::fb::vec2(pos.X, pos.Y);
			MZClient->AppServiceClient->SendContextMenuUpdate(FinishBuffer(mb, mz::CreateContextMenuUpdateDirect(mb, (mz::fb::UUID*)&itemId, &posx, instigator, &actions)));

		}
	}
}

void FMZSceneTreeManager::OnMZContextMenuCommandFired(mz::ContextMenuAction const& action)
{
	FGuid itemId = *(FGuid*)action.item_id();
	uint32 actionId = action.command();
	if (SceneTree.NodeMap.Contains(itemId))
	{
		if (auto actorNode = SceneTree.NodeMap.FindRef(itemId)->GetAsActorNode())
		{
			auto actor = actorNode->actor.Get();
			if (!actor)
			{
				return;
			}
			menuActions.ExecuteActorAction(actionId, actor);
		}
	}
}

void FMZSceneTreeManager::OnPostWorldInit(UWorld* World, const UWorld::InitializationValues InitValues)
{
	auto WorldContext = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport);
	if (World != WorldContext->World())
	{
		return;
	}
	FOnActorSpawned::FDelegate ActorSpawnedDelegate = FOnActorSpawned::FDelegate::CreateRaw(this, &FMZSceneTreeManager::OnActorSpawned);
	FOnActorDestroyed::FDelegate ActorDestroyedDelegate = FOnActorDestroyed::FDelegate::CreateRaw(this, &FMZSceneTreeManager::OnActorDestroyed);
	World->AddOnActorSpawnedHandler(ActorSpawnedDelegate);
	World->AddOnActorDestroyedHandler(ActorDestroyedDelegate);

	RescanScene();
	SendNodeUpdate(FMZClient::NodeId);
}


void FMZSceneTreeManager::OnPreWorldFinishDestroy(UWorld* World)
{
	//TODO check if we actually need this function
	return;
#if 0
	SceneTree.Clear();
	RegisteredProperties = Pins;
	PropertiesMap.Empty();
	MZActorManager->ReAddActorsToSceneTree();
	RescanScene(false);
	SendNodeUpdate(FMZClient::NodeId, false);
	//RescanScene();
	//SendNodeUpdate(FMZClient::NodeId);
#endif
}

struct PropUpdate
{
	FGuid actorId;
	FGuid pinId;
	FString displayName;
	FString componentName;
	FString propName;
	void* newVal;
	size_t newValSize;
	void* defVal;
	size_t defValSize;
	mz::fb::ShowAs pinShowAs;
	bool IsPortal;
};

void GetNodesSpawnedByMediaz(const mz::fb::Node* node, TMap<FGuid, FString>& spawnedByMediaz)
{
	if (flatbuffers::IsFieldPresent(node, mz::fb::Node::VT_META_DATA_MAP))
	{
		if (auto entry = node->meta_data_map()->LookupByKey("spawnTag"))
		{
			spawnedByMediaz.Add(*(FGuid*)node->id(), FString(entry->value()->c_str()));
		}
	}
	if (flatbuffers::IsFieldPresent(node->contents_as_Graph(), mz::fb::Graph::VT_NODES))
	{
		for (auto child : *node->contents_as_Graph()->nodes())
		{
			GetNodesSpawnedByMediaz(child, spawnedByMediaz);
		}
	}
}

void GetNodesWithProperty(const mz::fb::Node* node, std::vector<const mz::fb::Node*>& out)
{
	if (flatbuffers::IsFieldPresent(node, mz::fb::Node::VT_PINS) && node->pins()->size() > 0)
	{
		out.push_back(node);
	}

	if (flatbuffers::IsFieldPresent(node->contents_as_Graph(), mz::fb::Graph::VT_NODES))
	{
		for (auto child : *node->contents_as_Graph()->nodes())
		{
			GetNodesWithProperty(child, out);
		}
	}
	

}

void FMZSceneTreeManager::OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{ 
	if (!PropertyChangedEvent.MemberProperty || !PropertyChangedEvent.Property)
	{
		return;
	}
	if (!ObjectBeingModified->IsA(PropertyChangedEvent.MemberProperty->GetOwner<UClass>()))
	{
		return;
	}
	//not sure do we need this check
	//if (PropertyChangedEvent.Property && !ObjectBeingModified->IsA(PropertyChangedEvent.Property->GetOwner<UClass>()))
	//{
	//	return;
	//}
	if (!PropertyChangedEvent.Property->IsValidLowLevel() || CastField<FObjectProperty>(PropertyChangedEvent.Property))
	{
		return;
	}
	if (MZPropertyManager.PropertiesByPointer.Contains(PropertyChangedEvent.Property))
	{
		auto mzprop = MZPropertyManager.PropertiesByPointer.FindRef(PropertyChangedEvent.Property);
		mzprop->UpdatePinValue();
		SendPinValueChanged(mzprop->Id, mzprop->data);
		return;
	}
	if (MZPropertyManager.PropertiesByPointer.Contains(PropertyChangedEvent.MemberProperty))
	{
		auto mzprop = MZPropertyManager.PropertiesByPointer.FindRef(PropertyChangedEvent.MemberProperty);
		mzprop->UpdatePinValue();
		SendPinValueChanged(mzprop->Id, mzprop->data);
		return;
	}
}

void FMZSceneTreeManager::OnActorSpawned(AActor* InActor)
{
	if (IsActorDisplayable(InActor))
	{
		//todo fix logs
		//LOG("Actor spawned");
		//LOGF("%s", *(InActor->GetFName().ToString()));
		if (SceneTree.NodeMap.Contains(InActor->GetActorGuid()))
		{
			return;
		}
		SendActorAdded(InActor);
	}
}

void FMZSceneTreeManager::OnActorDestroyed(AActor* InActor)
{
	

	//todo fix logs
	//LOG("Actor destroyed");
	//LOG(*(InActor->GetFName().ToString()));
	//LOGF("%s", *(InActor->GetFName().ToString()));
	auto id = InActor->GetActorGuid();
	TSet<UObject*> RemovedItems;
	RemovedItems.Add(InActor);
	auto Components = InActor->GetComponents();
	for (auto comp : Components)
	{
		RemovedItems.Add(comp);
	}


	SendActorDeleted(id, RemovedItems);
}

void FMZSceneTreeManager::OnMZNodeImported(mz::fb::Node const& appNode)
{
	SceneTree.Root->Id = FMZClient::NodeId;
	//if (!MZClient->IsConnected())
	//{
	//	return;
	//}

	auto node = &appNode;

	std::vector<flatbuffers::Offset<mz::PartialPinUpdate>> PinUpdates;
	flatbuffers::FlatBufferBuilder fb1;
	for (auto pin : *node->pins())
	{
		PinUpdates.push_back(mz::CreatePartialPinUpdate(fb1, pin->id(), true, 0));
	}
	MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(fb1, mz::CreatePartialNodeUpdateDirect(fb1, (mz::fb::UUID*)&FMZClient::NodeId, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, 0, 0, &PinUpdates)));
	
	flatbuffers::FlatBufferBuilder fb3;
	MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer<mz::PartialNodeUpdate>(fb3, mz::CreatePartialNodeUpdateDirect(fb3, (mz::fb::UUID*)&FMZClient::NodeId, mz::ClearFlags::CLEAR_FUNCTIONS | mz::ClearFlags::CLEAR_NODES)));
	//flatbuffers::FlatBufferBuilder fb1;
	//std::vector<mz::fb::UUID> pinsToDelete;
	//for (auto pin : *node->pins())
	//{
	//	pinsToDelete.push_back(*pin->id());
	//}
	//MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(fb1, mz::CreatePartialNodeUpdateDirect(fb1, (mz::fb::UUID*)&FMZClient::NodeId, mz::ClearFlags::NONE, &pinsToDelete)));


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
				char* valcopy = nullptr;
				char* defcopy = nullptr;
				size_t valsize = 0;
				size_t defsize = 0;
				if (flatbuffers::IsFieldPresent(prop, mz::fb::Pin::VT_DATA))
				{
					valcopy = new char[prop->data()->size()];
					valsize = prop->data()->size();
					memcpy(valcopy, prop->data()->data(), prop->data()->size());
				}
				if (flatbuffers::IsFieldPresent(prop, mz::fb::Pin::VT_DEF))
				{
					defcopy = new char[prop->def()->size()];
					defsize = prop->def()->size();
					memcpy(defcopy, prop->def()->data(), prop->def()->size());
				}

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

				bool IsPortal = prop->contents_type() == mz::fb::PinContents::PortalPin;
				

				updates.push_back({ id, *(FGuid*)prop->id(),displayName, componentName, propName, valcopy, valsize, defcopy, defsize, prop->show_as(), IsPortal});
			}

		}
	}

	TMap<FGuid, FString> spawnedByMediaz; //old guid (imported) x spawn tag
	GetNodesSpawnedByMediaz(node, spawnedByMediaz);

	
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
			AActor* spawnedActor = MZActorManager->SpawnActor(spawnTag);
			if (spawnedActor)
			{
				sceneActorMap.Add(oldGuid, spawnedActor); //this will map the old id with spawned actor in order to match the old properties (imported from disk)
			}
		}
	}

	for (auto update : updates)
	{
		FGuid ActorId = update.actorId;
		
		if (update.IsPortal)
		{
			continue;
		}


		if (sceneActorMap.Contains(ActorId))
		{
			auto actor = sceneActorMap.FindRef(ActorId);
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
			if (mzprop && update.newValSize > 0)
			{
				mzprop->SetPropValue(update.newVal, update.newValSize);
			}
			if (!update.displayName.IsEmpty())
			{
				mzprop->DisplayName = update.displayName;
			}
			mzprop->UpdatePinValue();
			mzprop->PinShowAs = update.pinShowAs;
			if (mzprop && update.defValSize > 0)
			{
				mzprop->default_val = std::vector<uint8>(update.defValSize, 0);
				memcpy(mzprop->default_val.data(), update.defVal, update.defValSize);
			}
			//add portal pin logic
			/*Pins.Add(mzprop->Id, mzprop);
			RegisteredProperties.Add(mzprop->Id, mzprop);*/
			//PropertiesMap.Add(mzprop->Property, mzprop);

		}


		delete update.newVal;
		delete update.defVal;
	}

	//SceneTree.Clear();
	//MZPropertyManager.Reset();
	RescanScene(true);
	SendNodeUpdate(FMZClient::NodeId, false);

	PinUpdates.clear();
	flatbuffers::FlatBufferBuilder fb2;

	for (auto update : updates)
	{
		FGuid ActorId = update.actorId;

		if (update.IsPortal)
		{
			if (sceneActorMap.Contains(ActorId))
			{
				auto actor = sceneActorMap.FindRef(ActorId);
				if (!actor)
				{
					return;
				}

				PopulateAllChilds(actor);

				FProperty* Property = nullptr;
				if (update.componentName.IsEmpty())
				{
					Property = FindFProperty<FProperty>(actor->GetClass(), TCHAR_TO_UTF8(*update.propName));
				}
				else
				{
					auto component = FindObject<USceneComponent>(actor, *update.componentName);
					if(component) Property = FindFProperty<FProperty>(component->GetClass(), TCHAR_TO_UTF8(*update.propName));
				}

				if (Property)
				{
					//MZPropertyManager.CreatePortal(Property, update.pinShowAs);
					if (MZPropertyManager.PropertiesByPointer.Contains(Property))
					{
						auto MzProperty = MZPropertyManager.PropertiesByPointer.FindRef(Property);
						PinUpdates.push_back(mz::CreatePartialPinUpdate(fb2, (mz::fb::UUID*)&update.pinId, false, (mz::fb::UUID*)&MzProperty->Id));
					}
				}
			}
			else
			{
				//orphan
			}
		}

	}
	if (!PinUpdates.empty())
	{
		MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(fb2, mz::CreatePartialNodeUpdateDirect(fb2, (mz::fb::UUID*)&FMZClient::NodeId, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, 0, 0, &PinUpdates)));
	}
}

void FMZSceneTreeManager::SetPropertyValue(FGuid pinId, void* newval, size_t size)
{
	if (!MZPropertyManager.PropertiesById.Contains(pinId))
	{
		UE_LOG(LogTemp, Warning, TEXT("The property with given id is not found."));
		return;
	}

	auto mzprop = MZPropertyManager.PropertiesById.FindRef(pinId);
	std::vector<uint8_t> copy(size, 0);
	memcpy(copy.data(), newval, size);

	
	bool isChangedBefore = mzprop->IsChanged;
	mzprop->SetPropValue((void*)copy.data(), size);
	if (!isChangedBefore && mzprop->IsChanged && !MZPropertyManager.PropertyToPortalPin.Contains(pinId))
	{
		
		MZPropertyManager.CreatePortal(pinId, mz::fb::ShowAs::PROPERTY);
		
		//changed frst time
		//create portal code will be here
		//TSharedPtr<MZProperty> newmzprop = nullptr;
		//if (mzprop->GetRawObjectContainer())
		//{
		//	newmzprop = MZPropertyManager.CreateProperty(mzprop->GetRawObjectContainer(), mzprop->Property);
		//}
		//else if (mzprop->StructPtr)
		//{
		//	newmzprop = MZPropertyManager.CreateProperty(nullptr, mzprop->Property, &(RegisteredProperties), 0 /*, &(mzclient->PropertiesMap)*/, FString(""), mzprop->StructPtr);
		//}
		//if (newmzprop)
		//{
		//	newmzprop->default_val = mzprop->default_val;
		//	newmzprop->PinShowAs = mz::fb::ShowAs::PROPERTY;

		//	UObject* container = mzprop->GetRawObjectContainer();
		//	if (container)
		//	{
		//		newmzprop->DisplayName += FString(" (") + container->GetFName().ToString() + FString(")");
		//		newmzprop->CategoryName = container->GetFName().ToString() + FString("|") + newmzprop->CategoryName;
		//	}

		//	newmzprop->transient = false;
		//	Pins.Add(newmzprop->Id, newmzprop);
		//	//RegisteredProperties.Add(newmzprop->Id, newmzprop);
		//	SendPinAdded(FMZClient::NodeId, newmzprop);
		//}

	}
	//if (Pins.Contains(mzprop->Id))
	//{
	//	if (PropertiesMap.Contains(mzprop->Property))
	//	{
	//		auto otherProp = PropertiesMap.FindRef(mzprop->Property);
	//		otherProp->UpdatePinValue();
	//		SendPinValueChanged(otherProp->Id, otherProp->data);
	//	}

	//}
	//else
	//{
	//	for (auto& [id, pin] : Pins)
	//	{
	//		if (pin->Property == mzprop->Property)
	//		{
	//			pin->UpdatePinValue();
	//			SendPinValueChanged(pin->Id, pin->data);
	//		}
	//	}
	//}
	
}

void FMZSceneTreeManager::ConnectViewportTexture()
{
#ifdef VIEWPORT_TEXTURE
	auto viewport = Cast<UMZViewportClient>(GEngine->GameViewport);
	if (IsValid(viewport))
	{
		auto mzprop = ViewportTextureProperty;
		mzprop->ObjectPtr = viewport;

		auto tex = MZTextureShareManager::GetInstance()->AddTexturePin(mzprop);
		mzprop->data = mz::Buffer::FromNativeTable(tex);
	}
#endif
}

void FMZSceneTreeManager::DisconnectViewportTexture()
{
#ifdef VIEWPORT_TEXTURE
	MZTextureShareManager::GetInstance()->TextureDestroyed(ViewportTextureProperty);
	ViewportTextureProperty->ObjectPtr = nullptr;
	auto tex = MZTextureShareManager::GetInstance()->AddTexturePin(ViewportTextureProperty);
	ViewportTextureProperty->data = mz::Buffer::FromNativeTable(tex);
	MZTextureShareManager::GetInstance()->TextureDestroyed(ViewportTextureProperty);
#endif
}

void FMZSceneTreeManager::RescanScene(bool reset)
{
	if (reset)
	{
		Reset();
	}
	//TODO decide which is better
	//UWorld* World = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();

	UWorld* World = FMZSceneTreeManager::daWorld;

	flatbuffers::FlatBufferBuilder fbb;
	std::vector<flatbuffers::Offset<mz::fb::Node>> actorNodes;
	TArray<AActor*> ActorsInScene;
	if (IsValid(World))
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
		ConnectViewportTexture();
	}
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

bool FMZSceneTreeManager::PopulateNode(FGuid nodeId)
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
		//todo fix crash actor comes null
		if (!IsValid(actorNode->actor.Get()))
		{
			return false;
		}
			
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
			auto mzprop = MZPropertyManager.CreateProperty(actorNode->actor.Get(), AProperty);
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
				FName CategoryName = FObjectEditorUtils::GetCategoryFName(Property);

				UClass* Class = ActorClass;
				if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryName.ToString()) || !PropertyVisible(Property))
				{
					continue;
				}
				auto mzprop = MZPropertyManager.CreateProperty(Component, Property);
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
					auto mzprop = MZPropertyManager.CreateProperty(nullptr, *PropIt);
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
				&& (!ActorComp->IsVisualizationComponent())
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
							// todo fix UE_LOG(LogMediaZ, Error, TEXT("A Child node other than actor or component is present!"));
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

			auto mzprop = MZPropertyManager.CreateProperty(Component.Get(), Property);
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

void FMZSceneTreeManager::SendNodeUpdate(FGuid nodeId, bool bResetRootPins)
{
	if (!MZClient->IsConnected())
	{
		return;
	}

	if (nodeId == SceneTree.Root->Id)
	{
		if (!bResetRootPins)
		{
			flatbuffers::FlatBufferBuilder mb;
			std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = SceneTree.Root->SerializeChildren(mb);
			std::vector<flatbuffers::Offset<mz::fb::Node>> graphFunctions;
			for (auto& [_, cfunc] : CustomFunctions)
			{
				graphFunctions.push_back(cfunc->Serialize(mb));
			}

			MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer<mz::PartialNodeUpdate>(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::CLEAR_FUNCTIONS | mz::ClearFlags::CLEAR_NODES, 0, 0, 0, &graphFunctions, 0, &graphNodes)));

			return;
		}

		flatbuffers::FlatBufferBuilder mb;
		std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = SceneTree.Root->SerializeChildren(mb);
		std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins;
		for (auto& [_, property] : CustomProperties)
		{
			graphPins.push_back(property->Serialize(mb));
		}
		for (auto& [_, pin] : Pins)
		{
			graphPins.push_back(pin->Serialize(mb));
		}
		std::vector<flatbuffers::Offset<mz::fb::Node>> graphFunctions;
		for (auto& [_, cfunc] : CustomFunctions)
		{
			graphFunctions.push_back(cfunc->Serialize(mb));

		}

		MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer<mz::PartialNodeUpdate>(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::ANY, 0, &graphPins, 0, &graphFunctions, 0, &graphNodes)));

		return;
	}

	auto val = SceneTree.NodeMap.Find(nodeId);
	TSharedPtr<TreeNode> treeNode = val ? *val : nullptr;
	if (!(treeNode))
	{
		return;
	}

	flatbuffers::FlatBufferBuilder mb;
	std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = treeNode->SerializeChildren(mb);
	std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins;
	if (treeNode->GetAsActorNode())
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
	MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::ANY, 0, &graphPins, 0, &graphFunctions, 0, &graphNodes)));
}

void FMZSceneTreeManager::SendPinValueChanged(FGuid propertyId, std::vector<uint8> data)
{
	if (!MZClient->IsConnected())
	{
		return;
	}

	flatbuffers::FlatBufferBuilder mb;
	MZClient->AppServiceClient->NotifyPinValueChanged(FinishBuffer(mb, mz::CreatePinValueChangedDirect(mb, (mz::fb::UUID*)&propertyId, &data)));

}

void FMZSceneTreeManager::SendPinUpdate()
{
	if (!MZClient->IsConnected())
	{
		return;
	}

	auto nodeId = FMZClient::NodeId;

	flatbuffers::FlatBufferBuilder mb;
	std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins;
	for (auto& [_, pin] : CustomProperties)
	{
		graphPins.push_back(pin->Serialize(mb));
	}
	for (auto& [_, pin] : Pins)
	{
		graphPins.push_back(pin->Serialize(mb));
	}
	MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&nodeId, mz::ClearFlags::CLEAR_PINS, 0, &graphPins, 0, 0, 0, 0)));

}

void FMZSceneTreeManager::SendPinAdded(FGuid NodeId, TSharedPtr<MZProperty> const& mzprop)
{
	if (!MZClient->IsConnected())
	{
		return;
	}
	flatbuffers::FlatBufferBuilder mb;
	std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins = { mzprop->Serialize(mb) };
	MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&NodeId, mz::ClearFlags::NONE, 0, &graphPins, 0, 0, 0, 0)));

	return;
}

void FMZSceneTreeManager::SendActorAdded(AActor* actor, FString spawnTag)
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
			if (!MZClient->IsConnected())
			{
				return;
			}
			flatbuffers::FlatBufferBuilder mb;
			std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = { newNode->Serialize(mb) };
			MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&parentNode->Id, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, &graphNodes)));

		}
	}
	else
	{
		TSharedPtr<TreeNode> mostRecentParent;
		newNode = SceneTree.AddActor(actor->GetFolder().GetPath().ToString(), actor, mostRecentParent);
		if (!newNode)
		{
			return;
		}
		if (!spawnTag.IsEmpty())
		{
			newNode->mzMetaData.Add("spawnTag", spawnTag);
		}
		if (!MZClient->IsConnected())
		{
			return;
		}

		flatbuffers::FlatBufferBuilder mb;
		std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = { mostRecentParent->Serialize(mb) };
		MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&mostRecentParent->Parent->Id, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, &graphNodes)));

	}

	return;
}

void FMZSceneTreeManager::RemoveProperties(TSharedPtr<TreeNode> Node,
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
			MZPropertyManager.PropertiesById.Remove(prop->Id);
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
			MZPropertyManager.PropertiesById.Remove(prop->Id);
			PropertiesMap.Remove(prop->Property);

		}
	}
	for (auto& child : Node->Children)
	{
		RemoveProperties(child, PinsToRemove, PropertiesToRemove);
	}
}

void FMZSceneTreeManager::CheckPins(TSet<UObject*>& RemovedObjects,
	TSet<TSharedPtr<MZProperty>>& PinsToRemove,
	TSet<TSharedPtr<MZProperty>>& PropertiesToRemove)
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

void FMZSceneTreeManager::Reset()
{
	MZTextureShareManager::GetInstance()->Reset();
	SceneTree.Clear();
	Pins.Empty();
	MZPropertyManager.Reset();
	MZActorManager->ReAddActorsToSceneTree();
}

void FMZSceneTreeManager::SendActorDeleted(FGuid Id, TSet<UObject*>& RemovedObjects)
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
			MZPropertyManager.PropertiesById.Remove(pin->Id);
		}

		//delete from parent
		FGuid parentId = FMZClient::NodeId;
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

		if (!MZClient->IsConnected())
		{
			return;
		}

		flatbuffers::FlatBufferBuilder mb;
		std::vector<mz::fb::UUID> graphNodes = { *(mz::fb::UUID*)&node->Id };
		MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&parentId, mz::ClearFlags::NONE, 0, 0, 0, 0, &graphNodes, 0)));


		if (!pinsToRemove.IsEmpty())
		{
			std::vector<mz::fb::UUID> pinsToDelete;
			for (auto pin : pinsToRemove)
			{
				pinsToDelete.push_back(*(mz::fb::UUID*)&pin->Id);
			}
			MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&FMZClient::NodeId, mz::ClearFlags::NONE, &pinsToDelete, 0, 0, 0, 0, 0)));

		}

	}
}

void FMZSceneTreeManager::PopulateAllChilds(AActor* actor)
{
	FGuid ActorId = actor->GetActorGuid();
	if (PopulateNode(ActorId))
	{
		SendNodeUpdate(ActorId);
	}

	if(SceneTree.NodeMap.Contains(ActorId))
	{
		auto ActorNode = SceneTree.NodeMap.FindRef(ActorId);
		
		for (auto ChildNode : ActorNode->Children)
		{
			if (ChildNode->GetAsActorNode())
			{
				PopulateAllChilds(ChildNode->GetAsActorNode()->actor.Get());
			}
			else if (ChildNode->GetAsSceneComponentNode())
			{
				PopulateAllChildsOfSceneComponentNode(ChildNode->GetAsSceneComponentNode());
			}
		}

	}
}

void FMZSceneTreeManager::PopulateAllChildsOfSceneComponentNode(SceneComponentNode* SceneComponentNode)
{
	if (!SceneComponentNode)
	{
		return;
	}

	if (PopulateNode(SceneComponentNode->Id))
	{
		SendNodeUpdate(SceneComponentNode->Id);
	}

	for (auto ChildNode : SceneComponentNode->Children)
	{
		if (ChildNode->GetAsActorNode())
		{
			PopulateAllChilds(ChildNode->GetAsActorNode()->actor.Get());
		}
		else if (ChildNode->GetAsSceneComponentNode())
		{
			PopulateAllChildsOfSceneComponentNode(ChildNode->GetAsSceneComponentNode());
		}
	}
}



void FMZSceneTreeManager::HandleWorldChange()
{
	SceneTree.Clear();
	MZTextureShareManager::GetInstance()->Reset();

	TMap<FProperty*, MZPortal> Portals;
	TSet<AActor*> ActorsToRescan;

	flatbuffers::FlatBufferBuilder mb;
	std::vector<mz::fb::UUID> graphPins;// = { *(mz::fb::UUID*)&node->Id };
	std::vector<flatbuffers::Offset<mz::PartialPinUpdate>> PinUpdates;

	for (auto [id, portal] : MZPropertyManager.PortalPinsById)
	{
		if (!MZPropertyManager.PropertiesById.Contains(portal.SourceId))
		{
			continue;
		}
		auto MzProperty = MZPropertyManager.PropertiesById.FindRef(portal.SourceId);
		Portals.Add(MzProperty->Property, portal);

		if (MzProperty->ActorContainer)
		{
			ActorsToRescan.Add(MzProperty->ActorContainer.Get());
		}
		else if (MzProperty->ComponentContainer)
		{
			if (MzProperty->ComponentContainer.Actor)
			{
				ActorsToRescan.Add(MzProperty->ComponentContainer.Actor.Get());
			}
		}

		graphPins.push_back(*(mz::fb::UUID*)&portal.Id);
		//MZClient->AppServiceClient->Send(FinishBuffer(mb, mz::app::CreateSetPortalSourcePin(mb, (mz::fb::UUID*)&portal.Id, (mz::fb::UUID*)&portal.Id)));
		PinUpdates.push_back(mz::CreatePartialPinUpdate(mb, (mz::fb::UUID*)&portal.Id, true, 0));
	}

	if (!MZClient->IsConnected())
	{
		return;
	}

	MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&FMZClient::NodeId, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, 0, 0, &PinUpdates)));
	PinUpdates.clear();
	
	//MZClient->AppServiceClient->Send(FinishBuffer(mb, mz::app::CreateSetPortalSourcePin(mb, )));

	MZPropertyManager.Reset(false);
	MZActorManager->ReAddActorsToSceneTree();
	RescanScene(false);
	SendNodeUpdate(FMZClient::NodeId, false);


	for (auto actor : ActorsToRescan)
	{
		PopulateAllChilds(actor);
	}

	flatbuffers::FlatBufferBuilder mbb;
	for (auto& [property, portal] : Portals)
	{
		if (MZPropertyManager.PropertiesByPointer.Contains(property))
		{
			auto MzProperty = MZPropertyManager.PropertiesByPointer.FindRef(property);
			bool notOrphan = false;
			if (MZPropertyManager.PortalPinsById.Contains(portal.Id))
			{
				auto pPortal = MZPropertyManager.PortalPinsById.Find(portal.Id);
				pPortal->SourceId = MzProperty->Id;
			}
			portal.SourceId = MzProperty->Id;
			PinUpdates.push_back(mz::CreatePartialPinUpdate(mbb, (mz::fb::UUID*)&portal.Id, notOrphan, (mz::fb::UUID*)&MzProperty->Id));
		}

		//MZPropertyManager.CreatePortal(property, portal.ShowAs);
	}
	if (!PinUpdates.empty())
	{
		MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mbb, mz::CreatePartialNodeUpdateDirect(mbb, (mz::fb::UUID*)&FMZClient::NodeId, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, 0, 0, &PinUpdates)));
	}
	
}

void FMZSceneTreeManager::HandleBeginPIE(bool bIsSimulating)
{
	//todo fix logss
	FString WorldName = GEditor->GetEditorWorldContext().World()->GetMapName();
	UE_LOG(LogTemp, Warning, TEXT("Play session is started with editor world contexts world %s"), *WorldName);
	WorldName = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->GetMapName();
	UE_LOG(LogTemp,Warning, TEXT("Play session is started with viewports world %s"), *WorldName);

	FMZSceneTreeManager::daWorld = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();
	//update pins container referencess
	/*for (auto [_, pin] : Pins)
	{
		if (pin->ActorContainer)
		{
			pin->ActorContainer.UpdateActualActorPointer();
		}
		else if (pin->ComponentContainer)
		{
			pin->ComponentContainer.Actor.UpdateActualActorPointer();
			pin->ComponentContainer.UpdateActualComponentPointer();
		}
	}*/

	HandleWorldChange();


}

void FMZSceneTreeManager::HandleEndPIE(bool bIsSimulating)
{
	FString WorldName = GEditor->GetEditorWorldContext().World()->GetMapName();
	UE_LOG(LogTemp, Warning, TEXT("Play session is ended with editor world contexts world %s"), *WorldName);

	FMZSceneTreeManager::daWorld = GEditor->GetEditorWorldContext().World();
	HandleWorldChange();
	//update pins container referencess
	//TSet<FGuid> IdsToDelete;
	//auto TextureShareManager = MZTextureShareManager::GetInstance();
	//for (auto [id, pin] : Pins)
	//{
	//	if (pin->ActorContainer)
	//	{
	//		if (!pin->ActorContainer.UpdateActualActorPointer())
	//		{
	//			IdsToDelete.Add(id);
	//			TextureShareManager->TextureDestroyed(pin.Get());
	//		}
	//	}
	//	else if (pin->ComponentContainer)
	//	{
	//		pin->ComponentContainer.Actor.UpdateActualActorPointer();
	//		if (!pin->ComponentContainer.UpdateActualComponentPointer())
	//		{
	//			IdsToDelete.Add(id);
	//			TextureShareManager->TextureDestroyed(pin.Get());
	//		}
	//	}
	//}
	//for (auto Id : IdsToDelete)
	//{
	//	Pins.Remove(Id);
	//}

	//if (!IdsToDelete.IsEmpty())
	//{
	//	flatbuffers::FlatBufferBuilder mb;
	//	std::vector<mz::fb::UUID> pinsToDelete;
	//	for (auto Id : IdsToDelete)
	//	{
	//		pinsToDelete.push_back(*(mz::fb::UUID*)&Id);
	//	}
	//	MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&FMZClient::NodeId, mz::ClearFlags::NONE, &pinsToDelete, 0, 0, 0, 0, 0)));
	//}

	////MZTextureShareManager::GetInstance()->Reset();
	//SceneTree.Clear();
	//MZPropertyManager.PropertiesById.Empty();
	//MZPropertyManager.PropertiesById = Pins;
	//PropertiesMap.Empty();
	//MZActorManager->ReAddActorsToSceneTree();
	//RescanScene(false);
	//SendNodeUpdate(FMZClient::NodeId, /*update pins*/ false);

	//If you also want to reset pins
	//RescanScene();
	//SendNodeUpdate(FMZClient::NodeId);
		
}

AActor* FMZActorManager::SpawnActor(FString SpawnTag)
{
	if (!MZAssetManager)
	{
		return nullptr;
	}

	AActor* SpawnedActor = MZAssetManager->SpawnFromTag(SpawnTag);
	if (!SpawnedActor)
	{
		return nullptr;
	}
	

	ActorIds.Add(SpawnedActor->GetActorGuid());
	Actors.Add({ MZActorReference(SpawnedActor),SpawnTag });
	TSharedPtr<TreeNode> mostRecentParent;
	TSharedPtr<ActorNode> ActorNode = SceneTree.AddActor(NAME_Reality_FolderName.ToString(), SpawnedActor, mostRecentParent);
	ActorNode->mzMetaData.Add("spawnTag", SpawnTag);
	
	
	if (!MZClient->IsConnected())
	{
		return SpawnedActor;
	}

	flatbuffers::FlatBufferBuilder mb;
	std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = { mostRecentParent->Serialize(mb) };
	MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&mostRecentParent->Parent->Id, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, &graphNodes)));

	return SpawnedActor;
}

AActor* FMZActorManager::SpawnActor(UClass* ClassToSpawn)
{
	if (!MZAssetManager)
	{
		return nullptr;
	}

	FActorSpawnParameters sp;
	sp.bHideFromSceneOutliner = true;
	AActor* SpawnedActor = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(ClassToSpawn, 0, sp);
	if (!SpawnedActor)
	{
		return nullptr;
	}


	ActorIds.Add(SpawnedActor->GetActorGuid());
	Actors.Add({MZActorReference(SpawnedActor), ClassToSpawn->GetClassPathName().ToString()});
	TSharedPtr<TreeNode> mostRecentParent;
	TSharedPtr<ActorNode> ActorNode = SceneTree.AddActor(NAME_Reality_FolderName.ToString(), SpawnedActor, mostRecentParent);

	if (!MZClient->IsConnected())
	{
		return SpawnedActor;
	}

	flatbuffers::FlatBufferBuilder mb;
	std::vector<flatbuffers::Offset<mz::fb::Node>> graphNodes = { mostRecentParent->Serialize(mb) };
	MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&mostRecentParent->Parent->Id, mz::ClearFlags::NONE, 0, 0, 0, 0, 0, &graphNodes)));

	return SpawnedActor;
}

void FMZActorManager::ReAddActorsToSceneTree()
{
	for (auto& [Actor, spawnTag] : Actors)
	{
		if (Actor.UpdateActualActorPointer())
		{
			AActor* actor = Actor.Get();
			if (!actor)
			{
				continue;
			}

			auto ActorNode = SceneTree.AddActor(NAME_Reality_FolderName.ToString(), actor);
			ActorNode->mzMetaData.Add("spawnTag", spawnTag);
		}
		else
		{
			Actor = MZActorReference();
			spawnTag = "-";
		}
	}
	Actors = Actors.FilterByPredicate([](const TPair<MZActorReference, FString>& Actor)
		{
			return Actor.Key;
		});
}

void FMZActorManager::RegisterDelegates()
{
	FEditorDelegates::PreSaveWorld.AddRaw(this, &FMZActorManager::PreSave);
	FEditorDelegates::PostSaveWorld.AddRaw(this, &FMZActorManager::PostSave);
}

void FMZActorManager::PreSave(uint32 SaveFlags, UWorld* World)
{
	for (auto [Actor, spawnTag] : Actors)
	{
		AActor* actor = Actor.Get();
		if (!actor)
		{
			continue;
		}

		actor->SetFlags(actor->GetFlags() | RF_Transient);
	}
}

void FMZActorManager::PostSave(uint32 SaveFlags, UWorld* World, bool bSuccess)
{
	for (auto [Actor, spawnTag] : Actors)
	{
		AActor* actor = Actor.Get();
		if (!actor)
		{
			continue;
		}

		actor->SetFlags(actor->GetFlags() & ~RF_Transient);
	}
}

FMZPropertyManager::FMZPropertyManager()
{
}

void FMZPropertyManager::CreatePortal(FGuid PropertyId, mz::fb::ShowAs ShowAs)
{
	if (!PropertiesById.Contains(PropertyId))
	{
		return;
	}
	auto MZProperty = PropertiesById.FindRef(PropertyId);
	
	MZPortal NewPortal{FGuid::NewGuid() ,PropertyId};
	NewPortal.DisplayName = FString("");
	UObject* parent = MZProperty->GetRawObjectContainer();
	while (parent)
	{
		NewPortal.DisplayName = parent->GetFName().ToString() + FString(".") + NewPortal.DisplayName;
		parent = parent->GetTypedOuter<AActor>();
	}

	NewPortal.DisplayName += MZProperty->DisplayName + "(Portal)";
	NewPortal.TypeName = FString(MZProperty->TypeName.c_str());
	NewPortal.CategoryName = MZProperty->CategoryName;
	NewPortal.ShowAs = ShowAs;

	PortalPinsById.Add(NewPortal.Id, NewPortal);
	PropertyToPortalPin.Add(PropertyId, NewPortal.Id);

	if (!MZClient->IsConnected())
	{
		return;
	}
	flatbuffers::FlatBufferBuilder mb;
	std::vector<flatbuffers::Offset<mz::fb::Pin>> graphPins = { SerializePortal(mb, NewPortal, MZProperty.Get()) };
	MZClient->AppServiceClient->SendPartialNodeUpdate(FinishBuffer(mb, mz::CreatePartialNodeUpdateDirect(mb, (mz::fb::UUID*)&FMZClient::NodeId, mz::ClearFlags::NONE, 0, &graphPins, 0, 0, 0, 0)));
}

void FMZPropertyManager::CreatePortal(FProperty* uproperty, mz::fb::ShowAs ShowAs)
{
	if (PropertiesByPointer.Contains(uproperty))
	{
		auto MzProperty = PropertiesByPointer.FindRef(uproperty);
		CreatePortal(MzProperty->Id, ShowAs);
	}
}

TSharedPtr<MZProperty> FMZPropertyManager::CreateProperty(UObject* container, FProperty* uproperty, FString parentCategory)
{
	TSharedPtr<MZProperty> MzProperty = MZPropertyFactory::CreateProperty(container, uproperty, 0, 0, parentCategory);
	
	if (!MzProperty)
	{
		return TSharedPtr<MZProperty>();
	}

	PropertiesById.Add(MzProperty->Id, MzProperty);
	PropertiesByPointer.Add(MzProperty->Property, MzProperty);

	if (MzProperty->ActorContainer)
	{
		ActorsPropertyIds.FindOrAdd(MzProperty->ActorContainer.Get()->GetActorGuid()).Add(MzProperty->Id);
	}
	else if (MzProperty->ComponentContainer)
	{
		ActorsPropertyIds.FindOrAdd(MzProperty->ComponentContainer.Actor.Get()->GetActorGuid()).Add(MzProperty->Id);
	}

	for (auto Child : MzProperty->childProperties)
	{
		PropertiesById.Add(Child->Id, Child);
		PropertiesByPointer.Add(Child->Property, Child);
		
		if (Child->ActorContainer)
		{
			ActorsPropertyIds.FindOrAdd(Child->ActorContainer.Get()->GetActorGuid()).Add(Child->Id);
		}
		else if (Child->ComponentContainer)
		{
			ActorsPropertyIds.FindOrAdd(Child->ComponentContainer.Actor.Get()->GetActorGuid()).Add(Child->Id);
		}
	}

	return MzProperty;
}

void FMZPropertyManager::SetPropertyValue()
{
}

void FMZPropertyManager::ActorDeleted(FGuid DeletedActorId)
{
}

flatbuffers::Offset<mz::fb::Pin> FMZPropertyManager::SerializePortal(flatbuffers::FlatBufferBuilder& fbb, MZPortal Portal, MZProperty* SourceProperty)
{
	auto SerializedMetadata = SourceProperty->SerializeMetaData(fbb);
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&Portal.Id, TCHAR_TO_UTF8(*Portal.DisplayName), TCHAR_TO_UTF8(*Portal.TypeName), Portal.ShowAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*Portal.CategoryName), 0, 0, 0, 0, 0, 0, 0, 0, 0, false, &SerializedMetadata, 0, mz::fb::PinContents::PortalPin, mz::fb::CreatePortalPin(fbb, (mz::fb::UUID*)&Portal.SourceId).Union());
}

void FMZPropertyManager::Reset(bool ResetPortals)
{
	if (ResetPortals)
	{
		PropertyToPortalPin.Empty();
		PortalPinsById.Empty();
	}

	PropertiesById.Empty();
	PropertiesByPointer.Empty();
	ActorsPropertyIds.Empty();
}
