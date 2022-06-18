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


struct FMZRemoteControl : IMZRemoteControl {

  TMap<FGuid, MZEntity> EntityCache;
  TMap<URemoteControlPreset*, TArray<FGuid>> PresetEntities;
  //TMap<FProperty*, FGuid> TrackedProperties;

  virtual TMap<FGuid, MZEntity> const& GetExposedEntities() override
  {
      return EntityCache;
  }

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

  MZEntity RegisterExposedEntity(URemoteControlPreset* preset, FRemoteControlEntity* entity)
  {
      FRemoteControlProperty rprop = preset->GetProperty(entity->GetId()).GetValue();
      FProperty* prop = rprop.GetProperty();
      MZEntity mze = { MZType::GetType(prop), entity, rprop.GetPropertyHandle()};
      EntityCache.Add(entity->GetId(), mze);
      PresetEntities[preset].Add(entity->GetId());
      return mze;
  }

  void OnEntityExposed(URemoteControlPreset* preset, FGuid const& guid)
  {
      FRemoteControlEntity* entity = preset->GetExposedEntity(guid).Pin().Get();
      IMZClient::Get()->SendPinAdded(RegisterExposedEntity(preset, entity));
  }

  void OnEntityUnexposed(URemoteControlPreset* preset, FGuid const& guid)
  {
      //TrackedProperties.Remove(EntityCache[guid].fProperty);
      EntityCache.Remove(guid);
      PresetEntities[preset].Remove(guid);
      IMZClient::Get()->SendPinRemoved(guid);
  }

  void OnActorPropertyModified(URemoteControlPreset* Preset, FRemoteControlActor& /*Actor*/, UObject* ModifiedObject, FProperty* /*MemberProperty*/)
  {
      FMessageDialog::Debugf(FText::FromString("Preset registered " + ModifiedObject->GetFName().ToString()), 0);
  }

  void OnObjectPropertyChanged(UObject* obj, struct FPropertyChangedEvent& event)
  {
      //if (auto id = TrackedProperties.Find(event.Property))
      //{
      //    IMZClient::Get()->SendNodeUpdate(EntityCache[*id]);
      //}
  }

  void OnPresetLoaded(URemoteControlPreset* preset)
  {
      // FMessageDialog::Debugf(FText::FromString("Preset loaded " + preset->GetName()), 0);
      if (PresetEntities.Contains(preset))
      {
          return;
      }

      PresetEntities.Add(preset, {});

      for (auto& entity : preset->GetExposedEntities())
      {
          RegisterExposedEntity(preset, entity.Pin().Get());
      }

      preset->OnEntitiesUpdated().AddRaw(this, &FMZRemoteControl::OnEntitiesUpdated);
      preset->OnEntityExposed().AddRaw(this, &FMZRemoteControl::OnEntityExposed);
      preset->OnEntityUnexposed().AddRaw(this, &FMZRemoteControl::OnEntityUnexposed);
      preset->OnExposedPropertiesModified().AddRaw(this, &FMZRemoteControl::OnExposedPropertiesModified);
      preset->OnActorPropertyModified().AddRaw(this, &FMZRemoteControl::OnActorPropertyModified);

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
      URemoteControlPreset* preset = IRemoteControlModule::Get().ResolvePreset(name);
      for (auto& id : PresetEntities[preset])
      {
          EntityCache.Remove(id);
      }
      PresetEntities.Remove(preset);
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
  
  void OnAssetAdded(const FAssetData& asset)
  {
      static FName ClassName = "RemoteControlPreset";

      if (ClassName == asset.AssetClass)
      {
          // Only force load assets that are RemoteControlPresets
          // otherwise it slows down the startup like hell
          if (auto preset = Cast<URemoteControlPreset>(asset.GetAsset()))
          {
              OnPresetLoaded(preset);
          }
      }
  }

  void OnAssetRemoved(const FAssetData& asset)
  {
      if (asset.GetClass() != URemoteControlPreset::StaticClass())
      {
          return;
      }
      // OnPresetRemoved(Cast<URemoteControlPreset>(asset.GetAsset()));
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
    AssetRegistry.OnAssetAdded().AddRaw(this, &FMZRemoteControl::OnAssetAdded);
    FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FMZRemoteControl::OnAssetLoaded);
    
    TArray<TSoftObjectPtr<URemoteControlPreset>> presets;
    IRemoteControlModule::Get().GetPresets(presets);
    for (auto& preset : presets)
    {
        OnPresetLoaded(preset.Get());
    }
   // FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FMZRemoteControl::OnObjectPropertyChanged);

    // FMessageDialog::Debugf(FText::FromString("Loaded MZRemoteControl module"), 0);
  }

  void ShutdownModule() override 
  {

  }
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZRemoteControl, MZRemoteControl);
