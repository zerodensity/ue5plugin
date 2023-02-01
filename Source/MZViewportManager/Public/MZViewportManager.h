#pragma once
#include "CoreMinimal.h"

class MZVIEWPORTMANAGER_API FMZViewportManager : public IModuleInterface
{
public:
	//Empty constructor
	FMZViewportManager();

	//Called on startup of the module on Unreal Engine start
	virtual void StartupModule() override;

	//Called on shutdown of the module on Unreal Engine exit
	virtual void ShutdownModule() override;

	

};