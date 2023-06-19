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

	void SendLevelSequencerList();

	void RescanAndSendAll();

	TSet<FTopLevelAssetPath> GetAssetPathsOfClass(UClass* ParentClass);

	//scans the assets via the asset reggistry and stores for mediaz to use them to spawn actors
	void ScanAssets();

	void SetupCustomSpawns();

	AActor* SpawnBasicShape(FSoftObjectPath BasicShape);

	void ScanUMGs();

	void ScanLevelSequencers();

	AActor* SpawnFromTag(FString SpawnTag);

	AActor* SpawnFromAssetPath(FTopLevelAssetPath AssetPath);

	UUserWidget* CreateUMGFromTag(FString UMGTag);

	typedef TMap<FString, FTopLevelAssetPath> TAssetNameToPathMap;
	typedef TMap<FString, UObject*> TAssetNameToObjectMap;

	//spawnable actor assets
	TAssetNameToPathMap SpawnableAssets;

	//UMG assets list
	TAssetNameToPathMap UMGs;

	TAssetNameToObjectMap LevelSequencers;

	//for custom spawnables like basic shapes(cube, sphere etc.)
	TMap<FString, TFunction<AActor*()>> CustomSpawns;

	//Class communicates with MediaZ
	class FMZClient* MZClient;

	static const char* LevelSequencerList;
	static const char* CustomLevelSequencerName;

private:
	void ScanAssets(TAssetNameToPathMap& Map, UClass* ParentClass);
	void ScanAssetObjects(TAssetNameToObjectMap& Map, const UClass* ParentClass);
	void SendList(const char* ListName, const TArray<FString>& Value);
	void SendList(const char* ListName, const TAssetNameToPathMap& Value);
	void SendList(const char* ListName, const TAssetNameToObjectMap& Value);
};