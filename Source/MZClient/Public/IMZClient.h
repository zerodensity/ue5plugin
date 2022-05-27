#pragma once

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
};
