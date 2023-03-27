#include "MZAssetManager.h"


#include "ActorFactories/ActorFactory.h"
#include "ActorFactories/ActorFactoryBasicShape.h"
#include "Subsystems/PlacementSubsystem.h"
#include "Elements/Actor/ActorElementData.h"
#include "UObject/SoftObjectPath.h"
#include "Blueprint/UserWidget.h"
#include "UObject/Object.h"

IMPLEMENT_MODULE(FMZAssetManager, MZAssetManager)

template<typename T>
inline const T& FinishBuffer(flatbuffers::FlatBufferBuilder& builder, flatbuffers::Offset<T> const& offset)
{
	builder.Finish(offset);
	auto buf = builder.Release();
	return *flatbuffers::GetRoot<T>(buf.data());
}

FMZAssetManager::FMZAssetManager()
{
}

void FMZAssetManager::StartupModule()
{
	MZClient = &FModuleManager::LoadModuleChecked<FMZClient>("MZClient");
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().OnAssetAdded().AddRaw(this, &FMZAssetManager::OnAssetCreated);
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FMZAssetManager::OnAssetDeleted);

	MZClient->OnMZConnected.AddLambda([this](mz::fb::Node const& appNode)
		{
			RescanAndSendAll();
		});

	ScanAssets();
	ScanUMGs();
	SetupCustomSpawns();
}

void FMZAssetManager::ShutdownModule()
{
}

bool FMZAssetManager::HideFromOutliner() const
{
	return true;
}

void FMZAssetManager::OnAssetCreated(const FAssetData& createdAsset)
{
	if (!MZClient || !MZClient->IsConnected())
	{
		return;
	}
	RescanAndSendAll();
}

void FMZAssetManager::OnAssetDeleted(const FAssetData& removedAsset)
{
	if (!MZClient || !MZClient->IsConnected())
	{
		return;
	}
	RescanAndSendAll();
}

void FMZAssetManager::SendAssetList()
{
	if (!(MZClient && MZClient->IsConnected()))
	{
		return;
	}

	TArray<FString> SpawnTags;

	for (auto& [spawnTag, _] : SpawnableAssets)
	{
		SpawnTags.Add(spawnTag);
	}
	for (auto& [spawnTag, x] : CustomSpawns)
	{
		SpawnTags.Add(spawnTag);
	}
	SpawnTags.Sort();
	
	flatbuffers::FlatBufferBuilder mb;
	std::vector<mz::fb::String256> NameList;
	for (auto name : SpawnTags)
	{
		mz::fb::String256 str256;
		auto val = str256.mutable_val();
		auto size = name.Len() < 256 ? name.Len() : 256;
		memcpy(val->data(), TCHAR_TO_UTF8(*name), size);
		NameList.push_back(str256);
	}
	mz::fb::String256 listName;
	strcat((char*)listName.mutable_val()->data(), "UE5_ACTOR_LIST");
	auto offset = mz::app::CreateUpdateStringList(mb, mz::fb::CreateString256ListDirect(mb, &listName, &NameList));
	mb.Finish(offset);
	auto buf = mb.Release();
	auto root = flatbuffers::GetRoot<mz::app::UpdateStringList>(buf.data());
	MZClient->AppServiceClient->UpdateStringList(*root);
	// MZClient->AppServiceClient->UpdateStringList(FinishBuffer(mb, mz::app::CreateUpdateStringList(mb, mz::fb::CreateString256ListDirect(mb, &listName, &NameList))));
}

void FMZAssetManager::SendUMGList()
{
	if (!(MZClient && MZClient->IsConnected()))
	{
		return;
	}

	TArray<FString> UMGNames;

	for (auto [UMGName, _] : UMGs)
	{
		UMGNames.Add(UMGName);
	}

	flatbuffers::FlatBufferBuilder mb;
	std::vector<mz::fb::String256> NameList;
	for (auto name : UMGNames)
	{
		mz::fb::String256 str256;
		auto val = str256.mutable_val();
		auto size = name.Len() < 256 ? name.Len() : 256;
		memcpy(val->data(), TCHAR_TO_UTF8(*name), size);
		NameList.push_back(str256);
	}
	mz::fb::String256 listName;
	strcat((char*)listName.mutable_val()->data(), "UE5_UMG_LIST");
	auto offset = mz::app::CreateUpdateStringList(mb, mz::fb::CreateString256ListDirect(mb, &listName, &NameList));
	mb.Finish(offset);
	auto buf = mb.Release();
	auto root = flatbuffers::GetRoot<mz::app::UpdateStringList>(buf.data());
	MZClient->AppServiceClient->UpdateStringList(*root);
	// MZClient->AppServiceClient->UpdateStringList(FinishBuffer(mb, mz::app::CreateUpdateStringList(mb, mz::fb::CreateString256ListDirect(mb, &listName, &NameList))));
}

TSet<FTopLevelAssetPath> FMZAssetManager::GetAssetPathsOfClass(UClass* ParentClass)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray< FString > ContentPaths;
	ContentPaths.Add(TEXT("/Game"));
	ContentPaths.Add(TEXT("/Script"));
	ContentPaths.Add(TEXT("/mediaz"));
	ContentPaths.Add(TEXT("/RealityEngine"));
	AssetRegistryModule.Get().ScanPathsSynchronous(ContentPaths);
	//AssetRegistryModule.Get().WaitForCompletion(); // wait in startup to completion of the scan

	FTopLevelAssetPath BaseClassName = FTopLevelAssetPath(ParentClass);
	TSet< FTopLevelAssetPath > DerivedAssetPaths;
	{
		TArray< FTopLevelAssetPath > BaseNames;
		BaseNames.Add(BaseClassName);

		TSet< FTopLevelAssetPath > Excluded;
		AssetRegistryModule.Get().GetDerivedClassNames(BaseNames, Excluded, DerivedAssetPaths);
	}
	return DerivedAssetPaths;
}

void FMZAssetManager::RescanAndSendAll()
{
	ScanAssets();
	ScanUMGs();
	SendAssetList();
	SendUMGList();
}

void FMZAssetManager::ScanUMGs()
{
	UMGs.Empty();
	TSet< FTopLevelAssetPath > DerivedAssetPaths = GetAssetPathsOfClass(UUserWidget::StaticClass());
	for (auto AssetPath : DerivedAssetPaths)
	{
		FString AssetName = AssetPath.GetAssetName().ToString();
		if (AssetName.StartsWith(TEXT("SKEL_")))
		{
			continue;
		}
		AssetName.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);
		UMGs.Add(AssetName, AssetPath);
	}
	return;
}

void FMZAssetManager::ScanAssets()
{
	SpawnableAssets.Empty();
	TSet<FTopLevelAssetPath> DerivedAssetPaths = GetAssetPathsOfClass(AActor::StaticClass());
	for (auto AssetPath : DerivedAssetPaths)
	{
		FString AssetName = AssetPath.GetAssetName().ToString();
		if (AssetName.StartsWith(TEXT("SKEL_")))
		{
			continue;
		}
		AssetName.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);
		SpawnableAssets.Add(AssetName, AssetPath);
	}
	return;
}

void FMZAssetManager::SetupCustomSpawns()
{
	CustomSpawns.Add("Cube", [this]()
		{
			return SpawnBasicShape(UActorFactoryBasicShape::BasicCube);
		});
	CustomSpawns.Add("Sphere", [this]()
		{
			return SpawnBasicShape(UActorFactoryBasicShape::BasicSphere);
		});
	CustomSpawns.Add("Cylinder", [this]()
		{
			return SpawnBasicShape(UActorFactoryBasicShape::BasicCylinder);
		});
	CustomSpawns.Add("Cone", [this]()
		{
			return SpawnBasicShape(UActorFactoryBasicShape::BasicCone);
		});
	CustomSpawns.Add("Plane", [this]()
		{
			return SpawnBasicShape(UActorFactoryBasicShape::BasicPlane);
		});
}

AActor* FMZAssetManager::SpawnBasicShape(FSoftObjectPath BasicShape)
{
	UWorld* CurrentWorld = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();
	ULevel* CurrentLevel = CurrentWorld->GetCurrentLevel();

	FAssetData AssetToPlace = FAssetData(LoadObject<UStaticMesh>(nullptr, *BasicShape.ToString()));

	UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
	TScriptInterface<IAssetFactoryInterface> FactoryInterface = PlacementSubsystem->FindAssetFactoryFromAssetData(AssetToPlace);
	IAssetFactoryInterface* Interface = FactoryInterface.GetInterface();
	UActorFactory* ActorInterface = dynamic_cast<UActorFactory*>(Interface);
	if (!ActorInterface)
		return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.bHideFromSceneOutliner = HideFromOutliner();
	SpawnParams.OverrideLevel = CurrentLevel;

	// SpawnParams.bHideFromSceneOutliner is overwritten in "ActorInterface->CreateActor" by state of PlacementSubsystem.
	PlacementSubsystem->SetIsCreatingPreviewElements(HideFromOutliner());
	AActor* SpawnedActor = ActorInterface->CreateActor(AssetToPlace.GetAsset(), CurrentLevel, FTransform(), SpawnParams);
	PlacementSubsystem->SetIsCreatingPreviewElements(false);
	return SpawnedActor;
}

AActor* FMZAssetManager::SpawnFromAssetPath(FTopLevelAssetPath AssetPath)
{
	TSoftClassPtr<AActor> ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(*AssetPath.ToString()));

	UClass* LoadedAsset = ActorClass.LoadSynchronous();

	FActorSpawnParameters sp;
	sp.bHideFromSceneOutliner = HideFromOutliner();
	//todo look into hiding sp.bHideFromSceneOutliner = true;
	AActor* SpawnedActor = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(LoadedAsset, 0, sp);
	if (!SpawnedActor)
	{
		return nullptr;
	}
	if (!SpawnedActor->GetRootComponent())
	{
		auto RootComponent = NewObject<USceneComponent>(SpawnedActor, FName("DefaultSceneRoot"));

		SpawnedActor->SetRootComponent(RootComponent);
		RootComponent->CreationMethod = EComponentCreationMethod::Instance;
		RootComponent->RegisterComponent();
		SpawnedActor->AddInstanceComponent(RootComponent);
	}
	return SpawnedActor;
}

AActor* FMZAssetManager::SpawnFromTag(FString SpawnTag)
{	
	if (CustomSpawns.Contains(SpawnTag))
	{
		return CustomSpawns[SpawnTag]();
	}

	if (SpawnableAssets.Contains(SpawnTag))
	{
		FTopLevelAssetPath AssetPath = SpawnableAssets.FindRef(SpawnTag);
		return SpawnFromAssetPath(AssetPath);
	}
	
	return nullptr;
}

UUserWidget* FMZAssetManager::CreateUMGFromTag(FString UMGTag)
{
	if (UMGs.Contains(UMGTag))
	{
		FTopLevelAssetPath AssetPath = UMGs.FindRef(UMGTag);
		TSoftClassPtr<UUserWidget> WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(*AssetPath.ToString()));
		UClass* LoadedWidget = WidgetClass.LoadSynchronous();

		UWorld* CurrentWorld = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();
		return CreateWidget(CurrentWorld, WidgetClass.Get());
	}

	return nullptr;
}
