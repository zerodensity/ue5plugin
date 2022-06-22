#pragma once

#include "MZType.h"

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


class IMZClient : public IModuleInterface {
 public:
  static inline IMZClient* Get() {
    static const FName ModuleName = "MZClient";
    if (IsInGameThread()) {
      return FModuleManager::LoadModulePtr<IMZClient>(ModuleName);
    } else {
      return FModuleManager::GetModulePtr<IMZClient>(ModuleName);
    }
  }

  virtual void SendNodeUpdate(TMap<FGuid, MZEntity> const& entities) = 0;
  virtual void SendPinValueChanged(MZEntity) = 0;
  virtual void SendPinRemoved(FGuid) = 0;
  virtual void SendPinAdded(MZEntity entity) = 0;
  virtual void OnNodeUpdateReceived(mz::proto::Node const&) = 0;
  virtual void FreezeTextures(TArray<FGuid>) = 0;
  virtual void ThawTextures(TArray<FGuid>) = 0;

  virtual void Disconnect() = 0;
  virtual void NodeRemoved() = 0;
  virtual void QueueTextureCopy(FGuid id, const struct MZEntity* entity, mz::proto::Pin* dyn) = 0;
  virtual void OnTextureReceived(FGuid id, mz::proto::Texture const& texture) = 0;
  virtual void OnPinShowAsChanged(FGuid, mz::proto::ShowAs) = 0;
};
