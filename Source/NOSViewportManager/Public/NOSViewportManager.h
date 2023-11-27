/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once
#include "CoreMinimal.h"

class NOSVIEWPORTMANAGER_API FNOSViewportManager : public IModuleInterface
{
public:
	//Empty constructor
	FNOSViewportManager();

	//Called on startup of the module on Unreal Engine start
	virtual void StartupModule() override;

	//Called on shutdown of the module on Unreal Engine exit
	virtual void ShutdownModule() override;
};