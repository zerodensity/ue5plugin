// Copyright Epic Games, Inc. All Rights Reserved.

#include "IMZRemoteControl.h"
#include "IMZProto.h"
#include "IMZClient.h"
#include "MZType.h"


#include "AssetRegistryModule.h"
#include "IRemoteControlModule.h"
#include "IRemoteControlPropertyHandle.h"
#include "RemoteControlPreset.h"
#include "RemoteControlBinding.h"

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
#include <mutex>

#define LOCTEXT_NAMESPACE "FMZRemoteControl"
#pragma optimize("", off)

struct FMZRemoteControl : IMZRemoteControl {

  TMap<FGuid, MZEntity> EntityCache;
  TMap<URemoteControlPreset*, TArray<FGuid>> PresetEntities;
  //TMap<FProperty*, FGuid> TrackedProperties;

  std::mutex Mutex;
  TMap<URemoteControlPreset*, TSet<FRemoteControlEntity*>> ToBeResolved;

  virtual TMap<FGuid, MZEntity>& GetExposedEntities() override
  {
      return EntityCache;
  }

  virtual bool GetExposedEntity(FGuid id, MZEntity& out) override
  {
      if (auto entity = EntityCache.Find(id))
      {
          out = *entity;
          return true;
      }
      return false;
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


  bool RegisterExposedEntity(URemoteControlPreset* preset, FRemoteControlEntity* entity, MZEntity& mze)
  {
      if (!entity->GetBoundObject())
      {
          std::unique_lock lock(Mutex);
          ToBeResolved.FindOrAdd(preset).Add(entity);
          return false;
      }

      FRemoteControlProperty rprop = preset->GetProperty(entity->GetId()).GetValue();
      FProperty* prop = rprop.GetProperty();
      mze.Type = MZEntity::GetType(prop);
      mze.Entity = entity; 
      mze.Property = rprop.GetPropertyHandle();
      EntityCache.Add(entity->GetId(), mze);
      PresetEntities.FindOrAdd(preset).Add(entity->GetId());
      return true;
  }

  void OnEntityExposed(URemoteControlPreset* preset, FGuid const& guid)
  {
      FRemoteControlEntity* entity = preset->GetExposedEntity(guid).Pin().Get();
      MZEntity mze;
      if (RegisterExposedEntity(preset, entity, mze))
      {
          IMZClient::Get()->SendPinAdded(mze);
      }
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
      FMessageDialog::Debugf(FText::FromString("Actor property modified" + ModifiedObject->GetFName().ToString()), 0);
  }

  void OnObjectPropertyChanged(UObject* obj, struct FPropertyChangedEvent& event)
  {
      //if (auto id = TrackedProperties.Find(event.Property))
      //{
      //    IMZClient::Get()->SendNodeUpdate(EntityCache[*id]);
      //}
  }

  bool Tick(float)
  {     
      std::unique_lock lock(Mutex);
      auto tmp = ToBeResolved;
      ToBeResolved.Empty();
      for (auto& [preset, entities] : ToBeResolved)
      {
          for (auto& entity : entities)
          {
              if (!entity->GetBoundObject())
              {
                  ToBeResolved.FindOrAdd(preset).Add(entity);
                  continue;
              }
              MZEntity mze;
              RegisterExposedEntity(preset, entity, mze);
          }
      }
      return true;
  }

  void OnPresetLoaded(URemoteControlPreset* preset)
  {
      preset->RebindUnboundEntities();
      if (PresetEntities.Contains(preset))
      {
          auto& presetEntities = PresetEntities[preset];
          auto exposedEntities = preset->GetExposedEntities();

          TMap<FGuid, MZEntity> Updates;

          for (auto& entity : exposedEntities)
          {
              if (presetEntities.Contains(entity.Pin()->GetId()))
              {
                  continue;
              }
              MZEntity mze;
              if (RegisterExposedEntity(preset, entity.Pin().Get(), mze))
              {
                  Updates.Add(mze.Entity->GetId(), mze);
              }
          }
          IMZClient::Get()->SendNodeUpdate(Updates);
          return;
      }
      PresetEntities.FindOrAdd(preset);
      FMessageDialog::Debugf(FText::FromString("Preset loaded " + preset->GetName()), 0);

      for (auto& entity : preset->GetExposedEntities())
      {
          MZEntity mze;
          RegisterExposedEntity(preset, entity.Pin().Get(), mze);
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
          // FMessageDialog::Debugf(FText::FromString("Asset loaded "), 0);
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

  void OnAssetEditorOpened(UObject* obj)
  {
      if (auto preset = Cast<URemoteControlPreset>(obj))
      {
          // IMZClient::Get()->FreezeTextures(PresetEntities[preset]);
      }
  }

  
  void StartupModule() override {

    FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMZRemoteControl::Tick));
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();
    AssetRegistry.OnAssetRemoved().AddRaw(this, &FMZRemoteControl::OnAssetRemoved);
    AssetRegistry.OnAssetRenamed().AddRaw(this, &FMZRemoteControl::OnAssetRenamed);
    AssetRegistry.OnAssetAdded().AddRaw(this, &FMZRemoteControl::OnAssetAdded);

    // GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OnAssetEditorOpened().AddRaw(this, &FMZRemoteControl::OnAssetEditorOpened);
    
    FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FMZRemoteControl::OnAssetLoaded);
    URemoteControlPreset::OnPostLoadRemoteControlPreset.AddRaw(this, &FMZRemoteControl::OnPresetLoaded);
    //TArray<TSoftObjectPtr<URemoteControlPreset>> presets;
    //IRemoteControlModule::Get().GetPresets(presets);
    //for (auto& preset : presets)
    //{
    //    OnPresetLoaded(preset.Get());
    //}
    // FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FMZRemoteControl::OnObjectPropertyChanged);

    // FMessageDialog::Debugf(FText::FromString("Loaded MZRemoteControl module"), 0);
  }

  void ShutdownModule() override 
  {

  }
};
#pragma optimize("", on)
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZRemoteControl, MZRemoteControl);
