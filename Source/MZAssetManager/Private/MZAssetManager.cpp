// Copyright MediaZ AS. All Rights Reserved.

#include "MZAssetManager.h"


#include "ActorFactories/ActorFactory.h"
#include "ActorFactories/ActorFactoryBasicShape.h"
#include "Subsystems/PlacementSubsystem.h"
#include "Elements/Actor/ActorElementData.h"
#include "UObject/SoftObjectPath.h"
#include "Blueprint/UserWidget.h"
#include "UObject/Object.h"
#include "Engine/StaticMeshActor.h"

#include <vector>

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

void FMZAssetManager::SendList(const char* ListName, const TArray<FString>& Value)
{
	if (!(MZClient && MZClient->IsConnected()))
		return;

	flatbuffers::FlatBufferBuilder mb;
	std::vector<std::string> NameList;
	for (const auto& name : Value)
	{
		NameList.push_back(TCHAR_TO_UTF8(*name));
	}
	
	auto offset = mz::app::CreateUpdateStringList(mb, mz::fb::CreateStringList(mb, mb.CreateString(ListName), mb.CreateVectorOfStrings(NameList)));
	mb.Finish(offset);
	auto buf = mb.Release();
	auto root = flatbuffers::GetRoot<mz::app::UpdateStringList>(buf.data());
	MZClient->AppServiceClient->UpdateStringList(*root);
}

void FMZAssetManager::SendList(const char* ListName, const TAssetNameToPathMap& Value)
{
	TArray<FString> Names;
	for (auto [Name, _] : Value)
		Names.Add(Name);

	SendList(ListName, Names);
}

void FMZAssetManager::SendList(const char* ListName, const TAssetNameToObjectMap& Value)
{
	TArray<FString> Names;
	for (auto [Name, _] : Value)
		Names.Add(Name);

	SendList(ListName, Names);
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
	
	SendList("UE5_ACTOR_LIST", SpawnTags);
}

void FMZAssetManager::SendUMGList()
{
	SendList("UE5_UMG_LIST", UMGs);
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

void FMZAssetManager::ScanAssets(
	TAssetNameToPathMap& Map,
	UClass* ParentClass)
{
	Map.Empty();
	TSet< FTopLevelAssetPath > DerivedAssetPaths = GetAssetPathsOfClass(ParentClass);
	for (auto AssetPath : DerivedAssetPaths)
	{
		FString AssetName = AssetPath.GetAssetName().ToString();
		if (AssetName.StartsWith(TEXT("SKEL_")))
		{
			continue;
		}
		AssetName.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);
		Map.Add(AssetName, AssetPath);
	}
}

void FMZAssetManager::ScanAssetObjects(
	TAssetNameToObjectMap& Map,
	const UClass* ParentClass)
{
	Map.Empty();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> ObjectList;
	AssetRegistryModule.Get().GetAssetsByClass(ParentClass->GetClassPathName(), ObjectList);
	for (const FAssetData& Object : ObjectList)
		Map.Add(Object.AssetName.ToString(), Object.GetAsset());
}

void FMZAssetManager::ScanUMGs()
{
	ScanAssets(UMGs, UUserWidget::StaticClass());
}

void FMZAssetManager::ScanAssets()
{
	ScanAssets(SpawnableAssets, AActor::StaticClass());
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
	CustomSpawns.Add("RealityParentTransform", [this]()
		{
			FActorSpawnParameters sp;
			sp.bHideFromSceneOutliner = true;
			AActor* SpawnedActor = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World()->SpawnActor(AActor::StaticClass(), 0, sp);
			SpawnedActor->Rename(*MakeUniqueObjectName(nullptr, AActor::StaticClass(), FName("Reality Parent Transform Actor")).ToString());
			auto RootComponent = NewObject<USceneComponent>(SpawnedActor, FName("DefaultSceneRoot"));
			SpawnedActor->SetRootComponent(RootComponent);
			RootComponent->CreationMethod = EComponentCreationMethod::Instance;
			RootComponent->RegisterComponent();
			SpawnedActor->AddInstanceComponent(RootComponent);
			
			return SpawnedActor;
		});
}

AActor* FMZAssetManager::SpawnBasicShape(FSoftObjectPath BasicShape)
{
	UWorld* CurrentWorld = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();

	FAssetData AssetData = FAssetData(LoadObject<UStaticMesh>(nullptr, *BasicShape.ToString()));
	UObject* Asset = AssetData.GetAsset();
	FTransform Transform;
	FActorSpawnParameters SpawnParams;
	SpawnParams.bHideFromSceneOutliner = HideFromOutliner();

	// Implemented on base of UActorFactory::CreateActor
	AActor* SpawnedActor = CurrentWorld->SpawnActor(AStaticMeshActor::StaticClass(), &Transform, SpawnParams);
	if (SpawnedActor)
	{
		FActorLabelUtilities::SetActorLabelUnique(SpawnedActor, Asset->GetName());

		// Implemented on base of UActorFactoryBasicShape::PostSpawnActor
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset);
		if (StaticMesh)
		{
			AStaticMeshActor* StaticMeshActor = CastChecked<AStaticMeshActor>(SpawnedActor);
			UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
			if (StaticMeshComponent)
			{
				StaticMeshComponent->UnregisterComponent();
				StaticMeshComponent->SetStaticMesh(StaticMesh);
				StaticMeshComponent->StaticMeshDerivedDataKey = StaticMesh->GetRenderData()->DerivedDataKey;
				StaticMeshComponent->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
				StaticMeshComponent->RegisterComponent();
			}
		}

		SpawnedActor->PostEditChange();
		SpawnedActor->PostEditMove(true);
	}
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
