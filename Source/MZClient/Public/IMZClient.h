#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "IMZStream.h"

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
  
  virtual IMZStream* RequestStream(uint32_t Width, uint32_t Height, EPixelFormat ImageFormat, ETextureCreateFlags ImageUsage, uint32_t ImageCount, IMZStream::Type StreamType, uint64_t DeviceId) = 0;
};
