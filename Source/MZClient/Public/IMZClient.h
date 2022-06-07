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

  virtual void SendNodeUpdate(MZEntity) = 0 ;
  virtual void Disconnect() = 0 ;
  virtual void QueueTextureCopy(struct ID3D12Resource* res) = 0;
  virtual void OnTextureReceived(mz::proto::Texture const& texture) = 0;
};
