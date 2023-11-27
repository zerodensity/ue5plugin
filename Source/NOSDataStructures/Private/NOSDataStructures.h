/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once
#include "Modules/ModuleInterface.h"

class FMZDataStructures : public IModuleInterface
{
public:
	//Empty constructor
	FMZDataStructures();

	//Called on startup of the module on Unreal Engine start
	virtual void StartupModule() override;

	//Called on shutdown of the module on Unreal Engine exit
	virtual void ShutdownModule() override;
};