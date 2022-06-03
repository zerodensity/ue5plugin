// Copyright Epic Games, Inc. All Rights Reserved.

#include "IMZRemoteControl.h"
#include "IMZProto.h"
#include "IMZClient.h"
#include "MZType.h"

#include "AssetRegistryModule.h"
#include "IRemoteControlModule.h"
#include "IRemoteControlPropertyHandle.h"
#include "RemoteControlPreset.h"

#include "Async/Async.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/MessageDialog.h"

#include "Editor.h"
#include "Factories/Factory.h"
#include "AssetData.h"

#include <thread>
#include <map>

#define LOCTEXT_NAMESPACE "FMZRemoteControl"



void GetAssetSafe(const FAssetData& asset)
{
    
}

struct FMZRemoteControl : IMZRemoteControl {

  TMap<FGuid, MZEntity> EntityCache;

  TMap<FName, TArray<FGuid>> PresetEntities;

  void OnEntitiesUpdated(URemoteControlPreset* preset, const TSet<FGuid>& entities)
  {
      for (auto& id : entities)
      {
          // let mzEngine know about the changed entities
      }
  }

  void OnExposedPropertiesModified(URemoteControlPreset* preset, const TSet<FGuid>& entities)
  {
      for (auto& id : entities)
      {
          // let mzEngine know about the changed entities
      }
  }

  void OnEntityExposed(URemoteControlPreset* preset, FGuid const& guid)
  {
      // FMessageDialog::Debugf(FText::FromString("Entity exposed in " + preset->GetName()), 0);
      FRemoteControlEntity* entity = preset->GetExposedEntity(guid).Pin().Get();
      FRemoteControlProperty prop = entity->GetOwner()->GetProperty(entity->GetId()).GetValue();

      MZEntity mze = { MZType::GetType(prop.GetProperty()), entity, prop.GetPropertyHandle() };
      EntityCache.Add(entity->GetId(), mze);
      PresetEntities[preset->GetFName()].Add(guid);

      IMZClient::Get()->SendNodeUpdate(mze);
  }

  void OnEntityUnexposed(URemoteControlPreset* preset, FGuid const& guid)
  {
      EntityCache.Remove(guid);
      PresetEntities[preset->GetFName()].Remove(guid);
  }

  void OnPresetLoaded(URemoteControlPreset* preset)
  {
      // FMessageDialog::Debugf(FText::FromString("Preset loaded " + preset->GetName()), 0);
      PresetEntities.Add(preset->GetFName(), {});

      preset->OnEntitiesUpdated().AddRaw(this, &FMZRemoteControl::OnEntitiesUpdated);
      preset->OnEntityExposed().AddRaw(this, &FMZRemoteControl::OnEntityExposed);
      preset->OnEntityUnexposed().AddRaw(this, &FMZRemoteControl::OnEntityUnexposed);
      preset->OnExposedPropertiesModified().AddRaw(this, &FMZRemoteControl::OnExposedPropertiesModified);
  }

  void OnPresetRemoved(URemoteControlPreset* preset)
  {
      OnPresetUnregistered(preset->GetFName());
  }

  void OnPresetRegistered(FName name)
  {
      FMessageDialog::Debugf(FText::FromString("Preset registered " + name.ToString()), 0);
      OnPresetLoaded(IRemoteControlModule::Get().ResolvePreset(name));
  }

  void OnPresetUnregistered(FName name)
  {
      for (auto& id : PresetEntities[name])
      {
          EntityCache.Remove(id);
      }
      PresetEntities.Remove(name);
  }

  void OnPresetImported(UFactory* factory, UObject* preset)
  {
      FMessageDialog::Debugf(FText::FromString("Preset imported " + preset->GetName()), 0);
  }


  void OnAssetLoaded(UObject* asset)
  {
      if (auto preset = Cast<URemoteControlPreset>(asset))
      {
          OnPresetLoaded(preset);
      }
  }
  
  void OnAssetRemoved(const FAssetData& asset)
  {
      if (asset.GetClass() != URemoteControlPreset::StaticClass())
      {
          return;
      }
      OnPresetRemoved(Cast<URemoteControlPreset>(asset.GetAsset()));
  }

  void OnAssetRenamed(const FAssetData& asset, const FString& name)
  {
      if (asset.GetClass() != URemoteControlPreset::StaticClass())
      {
          return;
      }
  }

  void StartupModule() override {

    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();
    AssetRegistry.OnAssetRemoved().AddRaw(this, &FMZRemoteControl::OnAssetRemoved);
    AssetRegistry.OnAssetRenamed().AddRaw(this, &FMZRemoteControl::OnAssetRenamed);
    FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FMZRemoteControl::OnAssetLoaded);

    // FMessageDialog::Debugf(FText::FromString("Loaded MZRemoteControl module"), 0);
  }

  void ShutdownModule() override {

  }
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZRemoteControl, MZRemoteControl);
