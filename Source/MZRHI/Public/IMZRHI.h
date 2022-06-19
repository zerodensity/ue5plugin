#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


class IMZRHI : public IModuleInterface {
 public:
  static inline IMZRHI* Get() {
    static const FName ModuleName = "MZRHI";
    if (IsInGameThread()) {
      return FModuleManager::LoadModulePtr<IMZRHI>(ModuleName);
    } else {
      return FModuleManager::GetModulePtr<IMZRHI>(ModuleName);
    }
  }
};
