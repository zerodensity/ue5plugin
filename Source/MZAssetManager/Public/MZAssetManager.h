#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "EngineUtils.h"
#include "Engine/EngineTypes.h"
#include "MZClient.h"

class MZASSETMANAGER_API FMZAssetManager : public IModuleInterface {

public:
	//Empty constructor
	FMZAssetManager();

	//Called on startup of the module on Unreal Engine start
	virtual void StartupModule() override;

	//Called on shutdown of the module on Unreal Engine exit
	virtual void ShutdownModule() override;

	//update asset lists when a new asset is created
	void OnAssetCreated(const FAssetData& createdAsset);

	//update asset lists when an asset deleted
	void OnAssetDeleted(const FAssetData& removedAsset);

	void SendAssetList();

	void SendUMGList();

	void RescanAndSendAll();

	TSet<FTopLevelAssetPath> GetAssetPathsOfClass(UClass* ParentClass);

	//scans the assets via the asset reggistry and stores for mediaz to use them to spawn actors
	void ScanAssets();

	void SetupCustomScans();

	AActor* SpawnBasicShape(FSoftObjectPath BasicShape);

	void ScanUMGs();

	AActor* SpawnFromTag(FString SpawnTag);

	UUserWidget* CreateUMGFromTag(FString UMGTag);

	//spawnable actor assets
	TMap<FString, FTopLevelAssetPath> SpawnableAssets;

	//UMG assets list
	TMap<FString, FTopLevelAssetPath> UMGs;

	//for custom spawnables like basic shapes(cube, sphere etc.)
	TMap<FString, std::function<AActor*()>> CustomSpawns;

	//Class communicates with MediaZ
	class FMZClient* MZClient;
};