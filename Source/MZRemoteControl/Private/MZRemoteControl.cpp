// Copyright Epic Games, Inc. All Rights Reserved.

#include "IMZRemoteControl.h"

#include "AssetRegistryModule.h"
#include "IRemoteControlModule.h"
#include "IRemoteControlPropertyHandle.h"
#include "RemoteControlPreset.h"

#include "Async/Async.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/MessageDialog.h"

#include "Editor.h"
#include "Factories/Factory.h"
#include "AssetData.h"

#include <thread>
#include <map>

#define LOCTEXT_NAMESPACE "FMZRemoteControl"

std::map<uint64_t, MZType*> GTypeMap;

void MZType::Init(FField* Field_)
{
    Field = Field_;
    if (auto sprop = CastField<FStructProperty>(Field))
    {
        TArray<FField*> fields;
        sprop->GetInnerFields(fields);
        Tag = STRUCT;
        for (auto field : fields)
        {
            StructFields.Add(field->GetName(), GetType(field));
        }
    }
    else if (auto aprop = CastField<FArrayProperty>(Field))
    {
        Tag = ARRAY;
        ElementCount = aprop->ArrayDim;
        ElementType = GetType(aprop->Inner);
    }
    else if (auto nprop = CastField<FNumericProperty>(Field))
    {
        Tag = (nprop->IsFloatingPoint() ? FLOAT : INT);
        Width = nprop->ElementSize * 8;
    }
    else if (CastField<FBoolProperty>(Field))
    {
        Tag = BOOL;
        Width = 1;
    }
    else if (CastField<FStrProperty>(Field))
    {
        Tag = STRING;
    }
}

MZType* MZType::GetType(FField* Field)
{
    MZType*& ty = GTypeMap[Field->GetClass()->GetId()];
    
    if (!ty)
    {
        ty = new MZType();
        ty->Init(Field);
    }

    return ty;
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
      FMessageDialog::Debugf(FText::FromString("Entity exposed in " + preset->GetName()), 0);
      FRemoteControlEntity* entity = preset->GetExposedEntity(guid).Pin().Get();
      FRemoteControlProperty prop = entity->GetOwner()->GetProperty(entity->GetId()).GetValue();
      EntityCache.Add(entity->GetId(), MZEntity{ MZType::GetType(prop.GetProperty()), entity });
      PresetEntities[preset->GetFName()].Add(guid);
  }

  void OnEntityUnexposed(URemoteControlPreset* preset, FGuid const& guid)
  {
      EntityCache.Remove(guid);
      PresetEntities[preset->GetFName()].Remove(guid);
  }

  void OnPresetLoaded(URemoteControlPreset* preset)
  {
      FMessageDialog::Debugf(FText::FromString("Preset loaded " + preset->GetName()), 0);
      PresetEntities.Add(preset->GetFName(), {});

      preset->OnEntitiesUpdated().AddStatic([](URemoteControlPreset* preset, const TSet<FGuid>& entities, FMZRemoteControl* mzrc) {
          mzrc->OnEntitiesUpdated(preset, entities);
          }, this);

      preset->OnEntityExposed().AddStatic([](URemoteControlPreset* preset, FGuid const& entity, FMZRemoteControl* mzrc) {
          mzrc->OnEntityExposed(preset, entity);
          }, this);

      preset->OnEntityUnexposed().AddStatic([](URemoteControlPreset* preset, FGuid const& entity, FMZRemoteControl* mzrc) {
          mzrc->OnEntityUnexposed(preset, entity);
          }, this);

      preset->OnExposedPropertiesModified().AddStatic([](URemoteControlPreset* preset, const TSet<FGuid>& entities, FMZRemoteControl* mzrc) {
          mzrc->OnExposedPropertiesModified(preset, entities);
          }, this);

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


  void OnAssetAdded(const FAssetData& asset)
  {
      if (asset.GetClass() != URemoteControlPreset::StaticClass())
      {
          return;
      }
      OnPresetLoaded(Cast<URemoteControlPreset>(asset.GetAsset()));
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

    AssetRegistry.OnAssetAdded().AddRaw(this,  &FMZRemoteControl::OnAssetAdded);
    AssetRegistry.OnAssetRemoved().AddRaw(this, &FMZRemoteControl::OnAssetRemoved);
    AssetRegistry.OnAssetRenamed().AddRaw(this, &FMZRemoteControl::OnAssetRenamed);

    FMessageDialog::Debugf(FText::FromString("Loaded MZRemoteControl module"), 0);
  }

  void ShutdownModule() override {

  }
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZRemoteControl, MZRemoteControl);
