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

FName GetGroupNameFromPresetSafe(URemoteControlPreset* preset, FGuid const& id)
{
    if (auto group = preset->Layout.FindGroupFromField(id))
    {
        return group->Name;
    }
    return FName("Default");
}

struct FMZRemoteControl : IMZRemoteControl {

  TMap<FGuid, MZRemoteValue*> EntityCache;
  TMap<FGuid, MZFunction*> FunctionCache;
  TMap<URemoteControlPreset*, TArray<FGuid>> PresetEntities;
  //TMap<FProperty*, FGuid> TrackedProperties;

  std::mutex Mutex;
  TMap<URemoteControlPreset*, TSet<FRemoteControlEntity*>> ToBeResolved;

  virtual TMap<FGuid, MZRemoteValue*>& GetExposedEntities() override
  {
      return EntityCache;
  }

  virtual TMap<FGuid, MZFunction*>& GetExposedFunctions() override
  {
      return FunctionCache;
  }

  virtual MZRemoteValue* GetExposedEntity(FGuid id) override
  {
      if (auto entity = EntityCache.Find(id))
      {
         return *entity;
      }
      return nullptr;
  }

  virtual MZFunction* GetExposedFunction(FGuid id) override
  {
      if (auto entity = FunctionCache.Find(id))
      {
          return *entity;
      }
      return nullptr;
  }

void OnEntitiesUpdated(URemoteControlPreset* preset, const TSet<FGuid>& entities)
{
	for (auto& id : entities)
	{
		auto** mzrv = EntityCache.Find(id);
		if (mzrv && *mzrv)
		{
			if (!(*mzrv)->Entity->IsBound())
			{
				if (EntityCache.Contains(id))
				{
					//TODO: SEND PIN REMOVEED.
					EntityCache.Remove(id);
					IMZClient::Get()->SendPinRemoved(id);
				}
			}
			(*mzrv)->GetAsProp()->name = preset->GetExposedEntity(id).Pin().Get()->GetLabel();
			continue;
		}
		auto** mzfn = FunctionCache.Find(id);
		if (mzfn && *mzfn)
		{
			if (!(*mzfn)->GetObject())
			{
				if (FunctionCache.Contains(id))
				{
					//TODO: SEND FUNCTION REMOVEED.
					FunctionCache.Remove(id);
					IMZClient::Get()->SendFunctionRemoved(id);
				}
			}
			(*mzfn)->name = preset->GetExposedEntity(id).Pin().Get()->GetLabel();
			continue;
		}
		OnEntityExposed(preset, id);
	}
	IMZClient::Get()->SendNameUpdate(EntityCache, FunctionCache);
	
}

  void OnExposedPropertiesModified(URemoteControlPreset* preset, const TSet<FGuid>& entities)
  {
      for (auto& id : entities)
      {
          // let mzEngine know about the changed entities
      }
  }


  MZFunction* RegisterExposedFunction(URemoteControlPreset* preset, FRemoteControlEntity* entity)
  {
      MZFunction* mzf = new MZFunction();
      auto rfunc = preset->GetFunction(entity->GetId()).GetValue();
      auto func = rfunc.GetFunction();
      for (TFieldIterator<FProperty> It(func); It; ++It)
      {
          if (It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
          {
              continue;
          }
          FProperty* Property = *It;
          FGuid id = FGuid::NewGuid();
          MZParam* mzprm = new MZParam(rfunc, MZEntity::GetType(Property), id,  entity, Property->GetFName());
          EntityCache.Add(id, mzprm);
          mzf->params.push_back(mzprm);
      }
      PresetEntities.FindOrAdd(preset).Add(entity->GetId());
      mzf->rFunction = rfunc;
      mzf->id = entity->GetId(); 
	  mzf->category = GetGroupNameFromPresetSafe(preset, entity->GetId());
	  mzf->name = entity->GetLabel();
      FunctionCache.Add(mzf->id, mzf);
      return mzf;
  }

  MZRemoteValue* RegisterExposedEntity(URemoteControlPreset* preset, FRemoteControlEntity* entity)
  {
      if (!entity->GetBoundObject())
      {
          std::unique_lock lock(Mutex);
          ToBeResolved.FindOrAdd(preset).Add(entity);
          return nullptr;
      }

	  auto UEprop = preset->GetProperty(entity->GetId());

		FRemoteControlProperty rprop;
		if (UEprop.IsSet())
		{
			rprop = UEprop.GetValue();
		}
		else
		{
			return nullptr;
		}

      FProperty* prop = rprop.GetProperty();
	  if (!prop)
	  {
		  return nullptr;
	  }

      MZProperty* mzprop = new MZProperty(rprop.GetPropertyHandle(), MZEntity::GetType(prop), entity->GetId(), entity);

	  mzprop->category = GetGroupNameFromPresetSafe(preset, entity->GetId());
	  mzprop->name = entity->GetLabel();
      EntityCache.Add(entity->GetId(), mzprop);
      PresetEntities.FindOrAdd(preset).Add(entity->GetId());
      return mzprop;

  }

  void OnEntityExposed(URemoteControlPreset* preset, FGuid const& guid)
  {
      FRemoteControlEntity* entity = preset->GetExposedEntity(guid).Pin().Get();
      MZRemoteValue* mzrv;
      if (preset->GetFunction(entity->GetId()).IsSet())
	  {
          MZFunction* mzf = RegisterExposedFunction(preset, entity);
		  if (mzf)
		  {
			IMZClient::Get()->SendFunctionAdded(mzf);
		  }
          //return;
      }
      else if (preset->GetProperty(entity->GetId()).IsSet())
      {
		  
          mzrv = RegisterExposedEntity(preset, entity);
		  if (mzrv)
		  {
			IMZClient::Get()->SendPinAdded(mzrv);
		  }
          //RegisterExposedEntity(preset, entity, mze)
      }
  }

  void OnEntityUnexposed(URemoteControlPreset* preset, FGuid const& guid)
  {
      //TrackedProperties.Remove(EntityCache[guid].fProperty);

	  if (ToBeResolved.Contains(preset))
	  {
		  if (EntityCache.Contains(guid))
		  {
			ToBeResolved[preset].Remove(EntityCache[guid]->Entity);

		  }
		  else
		  {
			  auto tmp = ToBeResolved[preset];
			  for (auto entity : tmp)
			  {
				  if (entity->GetId() == guid)
				  {
					  ToBeResolved[preset].Remove(entity);
					  
				  }
			  }
			  if (ToBeResolved[preset].IsEmpty())
			  {
				  ToBeResolved.Remove(preset);
			  }
		  }
	  }

      if (FunctionCache.Contains(guid))
      {
          auto mzf = FunctionCache[guid];
          for (auto param : mzf->params)
          {
              EntityCache.Remove(param->id);
          }
          FunctionCache.Remove(guid);
          IMZClient::Get()->SendFunctionRemoved(guid);
      }
      else
      {
        EntityCache.Remove(guid);
        IMZClient::Get()->SendPinRemoved(guid);
      }
      PresetEntities[preset].Remove(guid);

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
      for (auto& [preset, entities] : tmp)
      {
          for (auto& entity : entities)
          {
              if (!entity->GetBoundObject())
              {
                  ToBeResolved.FindOrAdd(preset).Add(entity);
                  continue;
              }
			  MZRemoteValue* mzrv;
			  if (preset->GetFunction(entity->GetId()).IsSet())
			  {
				  MZFunction* mzf = RegisterExposedFunction(preset, entity);
				  if (mzf)
				  {
					  IMZClient::Get()->SendFunctionAdded(mzf);
				  }
			  }
			  else if (preset->GetProperty(entity->GetId()).IsSet())
			  {
				  mzrv = RegisterExposedEntity(preset, entity);
				  if (mzrv)
				  {
					  IMZClient::Get()->SendPinAdded(mzrv);
				  }
			  }
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

          TMap<FGuid, MZRemoteValue*> MZRVUpdates;
          TMap<FGuid, MZFunction*> MZFUpdates;

          for (auto& entity : exposedEntities)
          {
              if (presetEntities.Contains(entity.Pin()->GetId()))
              {
                  continue;
              }
              MZRemoteValue* mzrv;
              if (preset->GetFunction(entity.Pin()->GetId()).IsSet())
              {
                  MZFunction* mzf = RegisterExposedFunction(preset, entity.Pin().Get());
                  IMZClient::Get()->SendFunctionAdded(mzf);
              }
              else if (preset->GetProperty(entity.Pin()->GetId()).IsSet())
              {
                  mzrv = RegisterExposedEntity(preset, entity.Pin().Get());
                  IMZClient::Get()->SendPinAdded(mzrv);
              }
          }
          //IMZClient::Get()->SendNodeUpdate(MZRVUpdates, MZFUpdates);
          return;
      }
      PresetEntities.FindOrAdd(preset);
      FMessageDialog::Debugf(FText::FromString("Preset loaded " + preset->GetName()), 0);

      for (auto& entity : preset->GetExposedEntities())
      {

		  if (preset->GetFunction(entity.Pin()->GetId()).IsSet())
		  {
			  MZFunction* mzf = RegisterExposedFunction(preset, entity.Pin().Get());
			  //IMZClient::Get()->SendFunctionAdded(mzf);
		  }
		  else if (preset->GetProperty(entity.Pin()->GetId()).IsSet())
		  {
			  MZRemoteValue* mzrv = RegisterExposedEntity(preset, entity.Pin().Get());
			  //IMZClient::Get()->SendPinAdded(mzrv);
		  }
      }
  
      preset->OnEntitiesUpdated().AddRaw(this, &FMZRemoteControl::OnEntitiesUpdated);
      preset->OnEntityExposed().AddRaw(this, &FMZRemoteControl::OnEntityExposed);
      preset->OnEntityUnexposed().AddRaw(this, &FMZRemoteControl::OnEntityUnexposed);
      preset->OnExposedPropertiesModified().AddRaw(this, &FMZRemoteControl::OnExposedPropertiesModified);
      preset->OnActorPropertyModified().AddRaw(this, &FMZRemoteControl::OnActorPropertyModified);
	  preset->OnPresetLayoutModified().AddRaw(this, &FMZRemoteControl::OnPresetLayoutModified);
  }

  void OnPresetLayoutModified(URemoteControlPreset* preset)
  {
	  for (auto [id, mzrv] : EntityCache)
	  {
		  if (mzrv->GetAsProp())
		  {
			  mzrv->category = GetGroupNameFromPresetSafe(preset, id);
		  }
	  }
	  for (auto [id, mzf] : FunctionCache)
	  {
		  mzf->category = GetGroupNameFromPresetSafe(preset, id);
		  for (auto param : mzf->params)
		  {
			  param->category = GetGroupNameFromPresetSafe(preset, id);
		  }
	  }
	  IMZClient::Get()->SendCategoryUpdate(EntityCache, FunctionCache);


	  //FMessageDialog::Debugf(FText::FromString("Preset layout changed " + preset->GetFName().ToString()), 0);
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
	  auto copyCat = PresetEntities[preset];
      for (auto& id : copyCat)
      {
		  OnEntityUnexposed(preset, id);
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
