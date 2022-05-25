// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

#include "IMZClient.h"
#include "MZClient.h"

#include <vector>

#define LOCTEXT_NAMESPACE "FMZClientModule"

class FMZClientModule : public IMZClient {
 public:

  void StartupModule() override {

    FMessageDialog::Debugf(FText::FromString("Loaded MZClient module"), 0);
  }

  void ShutdownModule() override {
  }
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZClientModule, MZClient)