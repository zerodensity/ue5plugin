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
	std::vector<MZProperty*> OutProperties;

	flatbuffers::Offset<mz::fb::Node> Serialize(flatbuffers::FlatBufferBuilder& fbb);

	void Invoke();
	//todo can call the function and change the arguments
};

struct MZCustomFunction {
	FGuid id;
	std::function<void(TMap<FGuid, std::vector<uint8>> pins)> function;
	TMap<FGuid, std::string> params;
	std::function<flatbuffers::Offset<mz::fb::Node>(flatbuffers::FlatBufferBuilder& fbb)> serialize;
};