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

  void OnPresetRegistered(FName name)
  {
      TArray<TSoftObjectPtr<URemoteControlPreset>> Presets;
      IRemoteControlModule::Get().GetPresets(Presets);
      URemoteControlPreset* preset = Presets.FindByPredicate([name](auto preset) { return preset.Get()->GetFName() == name; })->Get();
      
      PresetEntities.Add(name, {});

      preset->OnEntitiesUpdated().Add(TDelegate<void(URemoteControlPreset*, TSet<FGuid> const&)>::
          CreateStatic([](URemoteControlPreset* preset, const TSet<FGuid>& entities, FMZRemoteControl* mzrc) {
              mzrc->OnEntitiesUpdated(preset, entities);
              }, this));

      preset->OnEntityExposed().Add(TDelegate<void(URemoteControlPreset*, FGuid const&)>::
          CreateStatic([](URemoteControlPreset* preset, FGuid const& entity, FMZRemoteControl* mzrc) {
              mzrc->OnEntityExposed(preset, entity);
              }, this));

      preset->OnEntityUnexposed().Add(TDelegate<void(URemoteControlPreset*, FGuid const&)>::
          CreateStatic([](URemoteControlPreset* preset, FGuid const& entity, FMZRemoteControl* mzrc) {
              mzrc->OnEntityUnexposed(preset, entity);
              }, this));

      preset->OnExposedPropertiesModified().Add(TDelegate<void(URemoteControlPreset*, TSet<FGuid> const&)>::
          CreateStatic([](URemoteControlPreset* preset, const TSet<FGuid>& entities, FMZRemoteControl* mzrc) {
              mzrc->OnExposedPropertiesModified(preset, entities);
              }, this));
  }

  void OnPresetUnregistered(FName name)
  {
      for (auto& id : PresetEntities[name])
      {
          EntityCache.Remove(id);
      }
      PresetEntities.Remove(name);
  }

  void StartupModule() override {
    auto& rc = IRemoteControlModule::Get();

    rc.OnPresetRegistered().Add(TDelegate<void(FName)>::CreateStatic([](FName name, FMZRemoteControl* mzrc) {
        mzrc->OnPresetRegistered(name);
        }, this));

    rc.OnPresetUnregistered().Add(TDelegate<void(FName)>::CreateStatic([](FName name, FMZRemoteControl* mzrc) {
        mzrc->OnPresetUnregistered(name);
        }, this));

    FMessageDialog::Debugf(FText::FromString("Loaded MZRemoteControl module"), 0);
  }

  void ShutdownModule() override {

  }
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZRemoteControl, MZRemoteControl);
