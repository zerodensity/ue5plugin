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

  virtual void SendNodeUpdate(TMap<FGuid, MZRemoteValue*> const& entities, TMap<FGuid, MZFunction*> const& functions) = 0;
  virtual void SendPinValueChanged(MZRemoteValue*) = 0;
  virtual void SendPinRemoved(FGuid) = 0;
  virtual void SendPinAdded(MZRemoteValue* mzrv) = 0;
  virtual void SendFunctionAdded(MZFunction* mzFunc) = 0;
  virtual void OnNodeUpdateReceived(mz::fb::Node const&) = 0;
  virtual void FreezeTextures(TArray<FGuid>) = 0;

  virtual void Disconnect() = 0;
  virtual void NodeRemoved() = 0;

  virtual void QueueTextureCopy(FGuid id, MZRemoteValue* mzrv, mz::fb::Texture* tex) = 0;
  virtual void OnTextureReceived(FGuid id, mz::fb::Texture const& texture) = 0;
  virtual void OnPinShowAsChanged(FGuid, mz::fb::ShowAs) = 0;
  virtual void OnPinValueChanged(FGuid, const void*, size_t) = 0;
  virtual void OnFunctionCall(FGuid nodeId, FGuid funcId) = 0;
};
