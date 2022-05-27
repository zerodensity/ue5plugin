#pragma once

#include "IMZProto.h"
#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "UObject/Field.h"

#include <memory>
#include <string>

struct MZPROTO_API MZType
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

struct MZPROTO_API MZEntity
{
	MZType* Type;
	struct FRemoteControlEntity* Entity;
	TSharedPtr<class IRemoteControlPropertyHandle> Property;
	//void SerializeToProto(google::protobuf::Any* value);
	//void SerializeToProto(mz::proto::DynamicField* field);
};
