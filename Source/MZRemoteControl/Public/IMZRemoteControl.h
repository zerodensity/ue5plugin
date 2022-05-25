#pragma once

#include "Modules/ModuleInterface.h"
#include "CoreMinimal.h"

struct MZType
{
	enum
	{
		BOOL,
		STRING,
		INT,
		FLOAT,
		ARRAY,
		STRUCT,
	} Tag;
	FField* Field = 0;

	//Scalar
	uint32_t Width = 0;

	// Array
	MZType* ElementType = 0;
	uint32_t ElementCount = 0;
	
	//Struct
	TMap<FString, MZType*> StructFields;

	static MZType* GetType(FField*);

private:
	MZType() = default;
	void Init(FField*);
};

struct MZEntity
{
	MZType* Type;
	struct FRemoteControlEntity* Entity;
};

class IMZRemoteControl : public IModuleInterface 
{

};

