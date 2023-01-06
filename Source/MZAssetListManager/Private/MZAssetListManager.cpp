#include "MZAssetListManager.h"


#include "ActorFactories/ActorFactory.h"
#include "ActorFactories/ActorFactoryBasicShape.h"
#include "Subsystems/PlacementSubsystem.h"
#include "Elements/Actor/ActorElementData.h"
#include "UObject/SoftObjectPath.h"
#include "Blueprint/UserWidget.h"

IMPLEMENT_MODULE(FMZAssetListManager, MZAssetListManager)

template<typename T>
inline const T& FinishBuffer(flatbuffers::FlatBufferBuilder& builder, flatbuffers::Offset<T> const& offset)
{
	builder.Finish(offset);
	auto buf = builder.Release();
	return *flatbuffers::GetRoot<T>(buf.data());
}

FMZAssetListManager::FMZAssetListManager()
{
}

void FMZAssetListManager::StartupModule()
{
	MZClient = &FModuleManager::LoadModuleChecked<FMZClient>("MZClient");

	MZClient->OnMZConnected.AddLambda([this](mz::fb::Node const& appNode)
		{
			RescanAndSendAll();
		});

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().OnAssetAdded().AddRaw(this, &FMZAssetListManager::OnAssetCreated);
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FMZAssetListManager::OnAssetDeleted);

	ScanAssets();
	ScanUMGs();
}

void FMZAssetListManager::ShutdownModule()
{
}

void FMZAssetListManager::OnAssetCreated(const FAssetData& createdAsset)
{
	RescanAndSendAll();
}

void FMZAssetListManager::OnAssetDeleted(const FAssetData& removedAsset)
{
	RescanAndSendAll();
}

void FMZAssetListManager::SendAssetList()
{
	if (!(MZClient && MZClient->IsConnected()))
	{
		return;
	}

	TArray<FString> SpawnTags;

	for (auto [spawnTag, _] : SpawnableAssets)
	{
		SpawnTags.Add(spawnTag);
	}
	for (auto [spawnTag, _] : CustomSpawns)
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
	MZClient->AppServiceClient->UpdateStringList(FinishBuffer(mb, mz::app::CreateUpdateStringList(mb, mz::fb::CreateString256ListDirect(mb, &listName, &NameList))));
}

void FMZAssetListManager::SendUMGList()
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
	MZClient->AppServiceClient->UpdateStringList(FinishBuffer(mb, mz::app::CreateUpdateStringList(mb, mz::fb::CreateString256ListDirect(mb, &listName, &NameList))));
}

TSet<FTopLevelAssetPath> FMZAssetListManager::GetAssetPathsOfClass(UClass* ParentClass)
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

void FMZAssetListManager::RescanAndSendAll()
{
	ScanAssets();
	ScanUMGs();
	SendAssetList();
	SendUMGList();
}

void FMZAssetListManager::ScanUMGs()
{
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

void FMZAssetListManager::ScanAssets()
{
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

void FMZAssetListManager::SetupCustomScans()
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

AActor* FMZAssetListManager::SpawnBasicShape(FSoftObjectPath BasicShape)
{
	AActor* SpawnedActor = nullptr;
	UWorld* currentWorld = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();
	ULevel* currentLevel = currentWorld->GetCurrentLevel();

	FAssetPlacementInfo PlacementInfo;
	PlacementInfo.AssetToPlace = FAssetData(LoadObject<UStaticMesh>(nullptr, *BasicShape.ToString()));
	PlacementInfo.FactoryOverride = UActorFactoryBasicShape::StaticClass();
	PlacementInfo.PreferredLevel = currentLevel;

	UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
	if (PlacementSubsystem)
	{
		TArray<FTypedElementHandle> PlacedElements = PlacementSubsystem->PlaceAsset(PlacementInfo, FPlacementOptions());
		for (auto elem : PlacedElements)
		{
			const FActorElementData* ActorElement = elem.GetData<FActorElementData>(true);
			if (ActorElement)
			{
				SpawnedActor = ActorElement->Actor;
			}
		}
	}
	
	return SpawnedActor;
}



AActor* FMZAssetListManager::SpawnFromTag(FString SpawnTag)
{	
	if (CustomSpawns.Contains(SpawnTag))
	{
		auto CustomSpawnFunction = CustomSpawns.FindRef(SpawnTag);
		return CustomSpawnFunction();
	}

	if (SpawnableAssets.Contains(SpawnTag))
	{
		FTopLevelAssetPath AssetPath = SpawnableAssets.FindRef(SpawnTag);
		TSoftClassPtr<AActor> ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(*AssetPath.ToString()));
		
		UClass* LoadedAsset = ActorClass.LoadSynchronous();
		
		FActorSpawnParameters sp;
		//todo look into hiding sp.bHideFromSceneOutliner = true;
		return GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(LoadedAsset, 0, sp);
	}
	
	return nullptr;
}

UUserWidget* FMZAssetListManager::CreateUMGFromTag(FString UMGTag)
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
