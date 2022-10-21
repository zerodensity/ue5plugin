#pragma once
#include "MZActorProperties.h"

struct MZFunction {

	MZFunction(UObject* container, UFunction* function);

	UFunction* Function;
	UObject* Container;
	FString FunctionName;
	FString DisplayName;
	FString CategoryName;
	FGuid id;
	uint8* Parameters = nullptr;
	std::vector<MZProperty*> Properties;

	flatbuffers::Offset<mz::fb::Node> Serialize(flatbuffers::FlatBufferBuilder& fbb);

	void Invoke();
	//todo can call the function and change the arguments
};