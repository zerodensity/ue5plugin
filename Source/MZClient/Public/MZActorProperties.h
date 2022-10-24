#pragma once
#include "CoreMinimal.h"
#include <vector>
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)
#include <mzFlatBuffersCommon.h>
#include "mediaz.h"
#include "AppClient.h"

struct MZProperty {

	MZProperty(UObject* container, FProperty* uproperty, uint8* StructPtr = nullptr);

	void SetValue(void* val, size_t size, uint8* customContainer = nullptr);
	flatbuffers::Offset<mz::fb::Pin> Serialize(flatbuffers::FlatBufferBuilder& fbb);

	FProperty* Property;
	UObject* Container;
	uint8* StructPtr = nullptr;

	FString PropertyName;
	FString DisplayName;
	FString CategoryName;
	FString UIMaxString;
	FString UIMinString;
	bool IsAdvanced = false;
	bool ReadOnly = false;
	std::string TypeName;
	FGuid id;
	std::vector<uint8_t> data;
	mz::fb::ShowAs PinShowAs = mz::fb::ShowAs::PROPERTY;



};