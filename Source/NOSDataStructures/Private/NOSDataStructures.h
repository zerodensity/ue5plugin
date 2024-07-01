/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once
#include "Modules/ModuleInterface.h"

class FNOSDataStructures : public IModuleInterface
{
public:
	//Empty constructor
	FNOSDataStructures();

	//Called on startup of the module on Unreal Engine start
	virtual void StartupModule() override;

	//Called on shutdown of the module on Unreal Engine exit
	virtual void ShutdownModule() override;
};