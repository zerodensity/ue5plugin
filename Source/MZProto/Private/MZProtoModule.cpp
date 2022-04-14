// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FMZProtoModule"

class FMZProtoModule : public IModuleInterface {
 public:
  void StartupModule() override {
    FString BaseDir = IPluginManager::Get().FindPlugin("mediaz")->GetBaseDir();

    FString DLLPath = FPaths::Combine(*BaseDir, TEXT("MediaZ"),
                                      TEXT("Binaries"), TEXT("Win64"));

    FPlatformProcess::PushDllDirectory(*DLLPath);

    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *DLLPath, TEXT("*.dll"));

    for (FString const& DLL : Files) {
     void* Lib = FPlatformProcess::GetDllHandle(*DLL);
     if (Lib) {
       LibHandles.Add(Lib);
     }
    }

    FPlatformProcess::PopDllDirectory(*DLLPath);

    FMessageDialog::Debugf(FText::FromString("Loaded MZProto module"), 0);
  }

  void ShutdownModule() override {
    // This function may be called during shutdown to clean up your module.  For
    // modules that support dynamic reloading, we call this function before
    // unloading the module.

    // Free the dll handle
    for (auto lib : LibHandles) {
      FPlatformProcess::FreeDllHandle(lib);
    }
  }

 private:
  /** Handle to the test dll we will load */
  TArray<void*> LibHandles;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMZProtoModule, MZProto)
