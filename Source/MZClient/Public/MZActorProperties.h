#pragma once
#include "CoreMinimal.h"
#include <vector>
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)
#include <mzFlatBuffersCommon.h>
#include "mediaz.h"
#include "AppClient.h"

struct MZProperty {

	MZProperty(UObject* container, FProperty* uproperty);

	void SetValue(void* val, size_t size);
	flatbuffers::Offset<mz::fb::Pin> Serialize(flatbuffers::FlatBufferBuilder& fbb);

	FProperty* Property;
	UObject* Container;
	FString PropertyName;
	FString DisplayName;
	FString CategoryName;
	FString UIMaxString;
	FString UIMinString;
	bool IsAdvanced = false;
	std::string TypeName;
	FGuid id;
	std::vector<uint8_t> data;



};