//mediaz plugin includes
#include "MZSceneTreeManager.h"
#include "MZClient.h"
#include "MZTextureShareManager.h"
#include "MZUMGRenderer.h"
#include "MZUMGRendererComponent.h"
#include "MZUMGRenderManager.h"
#include "MZAssetManager.h"

//unreal engine includes
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Kismet/GameplayStatics.h"
#include "EditorCategoryUtils.h"
#include "ObjectEditorUtils.h"


static const FName NAME_Reality_FolderName(TEXT("Reality Actors"));

IMPLEMENT_MODULE(FMZSceneTreeManager, MZSceneTreeManager)

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

void FMZSceneTreeManager::StartupModule()
{
	MZClient = &FModuleManager::LoadModuleChecked<FMZClient>("MZClient");
	MZAssetManager = &FModuleManager::LoadModuleChecked<FMZAssetManager>("MZAssetManager");

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

	FEditorDelegates::PostPIEStarted.AddRaw(this, &FMZSceneTreeManager::HandleBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FMZSceneTreeManager::HandleEndPIE);

	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FMZSceneTreeManager::OnPropertyChanged);

	FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FMZSceneTreeManager::OnPostWorldInit);
	FWorldDelegates::OnPreWorldFinishDestroy.AddRaw(this, &FMZSceneTreeManager::OnPreWorldFinishDestroy);

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
			AActor* SpawnedActor = MZAssetManager->SpawnFromTag(SpawnTag);

			if (SpawnedActor)
			{
				ActorsSpawnedByMediaZ.Add(SpawnedActor->GetActorGuid());
				SpawnedActor->SetFlags(RF_Transient);
				SpawnedActor->SetFolderPath(NAME_Reality_FolderName);

				SendActorAdded(SpawnedActor, SpawnTag);
				//todo fix logs LOGF("Spawned actor %s", *SpawnedActor->GetFName().ToString());
			}
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
			FActorSpawnParameters sp;
			//sp.bHideFromSceneOutliner = true;
			AActor* realityCamera = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(LoadedBpAsset, 0, sp);
			if (realityCamera)
			{
				mzclient->ActorsSpawnedByMediaZ.Add(realityCamera->GetActorGuid());
				realityCamera->SetFlags(RF_Transient);
				realityCamera->SetFolderPath(NAME_Reality_FolderName);
				mzclient->SendActorAdded(realityCamera, actorName);
				//todo fix logs LOGF("Spawned actor %s", *realityCamera->GetFName().ToString());
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
				mzclient->SendPinAdded(FMZClient::NodeId, mzprop);
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

			FActorSpawnParameters sp;
			//sp.bHideFromSceneOutliner = true;
			AActor* projectionCube = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(LoadedBpAsset, 0, sp);
			if (projectionCube)
			{
				mzclient->ActorsSpawnedByMediaZ.Add(projectionCube->GetActorGuid());
				projectionCube->SetFlags(RF_Transient);
				projectionCube->SetFolderPath(NAME_Reality_FolderName);
				mzclient->SendActorAdded(projectionCube, actorName);
				//todo fix logs LOGF("Spawned actor %s", *projectionCube->GetFName().ToString());
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
			for (auto const& mzprop : pinsToSpawn)
			{
				mzprop->DisplayName = projectionCube->GetActorLabel() + " | " + mzprop->DisplayName;
				//mzclient->RegisteredProperties.Add(mzprop->Id, mzprop);
				mzprop->transient = false;
				mzclient->Pins.Add(mzprop->Id, mzprop);
				mzclient->SendPinAdded(FMZClient::NodeId, mzprop);
			}

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
				FActorSpawnParameters sp;
				//sp.bHideFromSceneOutliner = true;
				UMGManager = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(AMZUMGRenderManager::StaticClass(), 0, sp);
				UMGManager->Rename(*MakeUniqueObjectName(nullptr, AActor::StaticClass(), FName("MZUMGRenderManager")).ToString());
				UMGManager->SetActorLabel(TEXT("MZUMGRenderManager"));

				USceneComponent* newRoot = NewObject<USceneComponent>(UMGManager);
				newRoot->Rename(TEXT("UMGs"));
				UMGManager->SetRootComponent(newRoot);
				newRoot->CreationMethod = EComponentCreationMethod::Instance;
				newRoot->RegisterComponent();
				UMGManager->AddInstanceComponent(newRoot);

				ActorsSpawnedByMediaZ.Add(UMGManager->GetActorGuid());
				UMGManager->SetFlags(RF_Transient);
				UMGManager->SetFolderPath(NAME_Reality_FolderName);
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
					{
						auto texture = FindFProperty<FProperty>(NewRendererComp->GetClass(), "UMGRenderTarget");
						auto mzprop = MZPropertyFactory::CreateProperty(NewRendererComp, texture, &(RegisteredProperties));
						mzprop->DisplayName = newWidget->GetFName().ToString() + " | " + mzprop->DisplayName;

						if (mzprop)
						{
							mzprop->PinShowAs = mz::fb::ShowAs::OUTPUT_PIN;
							pinsToSpawn.push_back(mzprop);
						}
					}
				}
			}
			for (auto const& mzprop : pinsToSpawn)
			{
				mzprop->transient = false;
				Pins.Add(mzprop->Id, mzprop);
				SendPinAdded(FMZClient::NodeId, mzprop);
			}
		};
		CustomFunctions.Add(mzcf->Id, mzcf);
	}

}

void FMZSceneTreeManager::ShutdownModule()
{


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
			texman->UpdateTexturePin(mzprop, (mz::fb::Texture*)pin->data()->Data());
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
	SetPropertyValue(*(FGuid*)&pinId, (void*)data, size);
}

void FMZSceneTreeManager::OnMZPinShowAsChanged(mz::fb::UUID const& Id, mz::fb::ShowAs newShowAs)
{
	FGuid pinId = *(FGuid*)&Id;
	if (Pins.Contains(pinId))
	{
		auto mzprop = Pins.FindRef(pinId);
		mzprop->PinShowAs = newShowAs;
		SendPinUpdate();
	}
	else if (RegisteredProperties.Contains(pinId))
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
			if (RegisteredProperties.Contains(id))
			{
				auto mzprop = RegisteredProperties.FindRef(id);
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

void FMZSceneTreeManager::OnMZExecutedApp(mz::fb::Node const& appNode)
{
	TMap<FGuid, std::vector<uint8>> updates = ParsePins(appNode);

	for (auto& [id, data] : updates)
	{
		if (RegisteredProperties.Contains(id))
		{
			auto mzprop = RegisteredProperties.FindRef(id);
			mzprop->SetPropValue((void*)data.data(), data.size());
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
			//auto deleteAction = 
			//std::vector<flatbuffers::Offset<mz::ContextMenuItem>> actions = { mz::CreateContextMenuItemDirect(mb, "Destroy", 0, 0) };
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
	RescanScene();
	SendNodeUpdate(FMZClient::NodeId);
}

void FMZSceneTreeManager::OnPreWorldFinishDestroy(UWorld* World)
{
	RescanScene();
	SendNodeUpdate(FMZClient::NodeId);
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

void FMZSceneTreeManager::OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
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
	}
	if (PropertiesMap.Contains(PropertyChangedEvent.MemberProperty))
	{
		auto mzprop = PropertiesMap.FindRef(PropertyChangedEvent.MemberProperty);
		mzprop->UpdatePinValue();
		SendPinValueChanged(mzprop->Id, mzprop->data);
	}
	for (auto [id, pin] : Pins)
	{
		if (pin->Property == PropertyChangedEvent.MemberProperty)
		{
			pin->UpdatePinValue();
			SendPinValueChanged(pin->Id, pin->data);
			break;
		}
		else if (pin->Property == PropertyChangedEvent.Property)
		{
			pin->UpdatePinValue();
			SendPinValueChanged(pin->Id, pin->data);
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
				break;
			}
		}
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
	ActorsSpawnedByMediaZ.Remove(InActor->GetActorGuid());

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
	auto node = &appNode;
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

				updates.push_back({ id, displayName, componentName, propName, valcopy, prop->data()->size(), defcopy, prop->def()->size(), prop->show_as() });
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

	//for (auto [oldGuid, spawnTag] : spawnedByMediaz)
	//{
	//	if (!sceneActorMap.Contains(oldGuid))
	//	{
	//		///spawn
	//		FString actorName(spawnTag);
	//		AActor* spawnedActor = nullptr;
	//		if (ActorPlacementParamMap.Contains(actorName))
	//		{
	//			auto placementInfo = ActorPlacementParamMap.FindRef(actorName);
	//			UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
	//			if (PlacementSubsystem)
	//			{
	//				TArray<FTypedElementHandle> PlacedElements = PlacementSubsystem->PlaceAsset(placementInfo, FPlacementOptions());
	//				for (auto elem : PlacedElements)
	//				{
	//					const FActorElementData* ActorElement = elem.GetData<FActorElementData>(true);
	//					if (ActorElement)
	//					{
	//						spawnedActor = ActorElement->Actor;
	//					}
	//				}
	//			}
	//		}
	//		else if (SpawnableClasses.Contains(actorName))
	//		{
	//			if (GEngine)
	//			{
	//				if (UObject* ClassToSpawn = SpawnableClasses[actorName])
	//				{
	//					FActorSpawnParameters sp;
	//					sp.bHideFromSceneOutliner = true;
	//					UBlueprint* GeneratedBP = Cast<UBlueprint>(ClassToSpawn);
	//					UClass* NativeClass = Cast<UClass>(ClassToSpawn);
	//					UClass* Class = GeneratedBP ? (UClass*)(GeneratedBP->GeneratedClass) : (NativeClass);
	//					spawnedActor = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(Class, 0, sp);
	//				}
	//			}
	//		}
	//		else
	//		{
	//			LOG("Cannot spawn actor");
	//		}
	//		if (spawnedActor)
	//		{
	//			sceneActorMap.Add(oldGuid, spawnedActor); //this will map the old id with spawned actor in order to match the old properties (imported from disk)
	//		}
	//	}
	//}

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
	RescanScene(false);

	SendNodeUpdate(FMZClient::NodeId);
	//SendAssetList();

}

void FMZSceneTreeManager::SetPropertyValue(FGuid pinId, void* newval, size_t size)
{
	if (!RegisteredProperties.Contains(pinId))
	{
		UE_LOG(LogTemp, Warning, TEXT("The property with given id is not found."));
		return;
	}

	auto mzprop = RegisteredProperties.FindRef(pinId);
	std::vector<uint8_t> copy(size, 0);
	memcpy(copy.data(), newval, size);

	
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
			SendPinAdded(FMZClient::NodeId, newmzprop);
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
	
}

void FMZSceneTreeManager::RescanScene(bool reset)
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
				FName CategoryName = FObjectEditorUtils::GetCategoryFName(Property);

				UClass* Class = ActorClass;
				if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryName.ToString()) || !PropertyVisible(Property))
				{
					continue;
				}
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

void FMZSceneTreeManager::SendNodeUpdate(FGuid nodeId)
{
	if (!MZClient->IsConnected())
	{
		return;
	}

	if (nodeId == SceneTree.Root->Id)
	{
		flatbuffers::FlatBufferBuilder mb;
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
	RegisteredProperties.Empty();
	PropertiesMap.Empty();
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
			RegisteredProperties.Remove(pin->Id);
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

void FMZSceneTreeManager::HandleBeginPIE(bool bIsSimulating)
{
	//todo fix logss
	//LOG("PLAY SESSION IS STARTED");
	RescanScene();
	SendNodeUpdate(FMZClient::NodeId);
}

void FMZSceneTreeManager::HandleEndPIE(bool bIsSimulating)
{
	Reset();
	RescanScene();
	SendNodeUpdate(FMZClient::NodeId);
		
}

