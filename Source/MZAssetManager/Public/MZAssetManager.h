/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "EngineUtils.h"
#include "Engine/EngineTypes.h"
#include "MZClient.h"

#include "GenericPlatform/GenericPlatformMisc.h"

class MZASSETMANAGER_API FMZAssetManager : public IModuleInterface {

public:
	//Empty constructor
	FMZAssetManager();

	//Called on startup of the module on Unreal Engine start
	virtual void StartupModule() override;

	//Called on shutdown of the module on Unreal Engine exit
	virtual void ShutdownModule() override;

	bool HideFromOutliner() const;

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

	void SetupCustomSpawns();

	AActor* SpawnBasicShape(FSoftObjectPath BasicShape);

	void ScanUMGs();

	AActor* SpawnFromTag(FString SpawnTag);

	AActor* SpawnFromAssetPath(FTopLevelAssetPath AssetPath);

	UUserWidget* CreateUMGFromTag(FString UMGTag);

	//spawnable actor assets
	TMap<FString, FTopLevelAssetPath> SpawnableAssets;

	//UMG assets list
	TMap<FString, FTopLevelAssetPath> UMGs;

	//for custom spawnables like basic shapes(cube, sphere etc.)
	TMap<FString, TFunction<AActor*()>> CustomSpawns;

	//Class communicates with MediaZ
	class FMZClient* MZClient;
};