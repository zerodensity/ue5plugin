#pragma once
#if WITH_EDITOR
#include "MZActorProperties.h"

struct MZFunction
{
	MZFunction(UObject* container, UFunction* function);

	UFunction* Function;
	UObject* Container;
	FString FunctionName;
	FString DisplayName;
	FString CategoryName;
	FGuid Id;
	uint8* Parameters = nullptr;
	std::vector<TSharedPtr<MZProperty>> Properties;
	std::vector<TSharedPtr<MZProperty>> OutProperties;

	flatbuffers::Offset<mz::fb::Node> Serialize(flatbuffers::FlatBufferBuilder& fbb);

	void Invoke();
	//todo can call the function and change the arguments
};

struct MZCustomFunction
{
	FGuid Id;
	std::function<void(TMap<FGuid, std::vector<uint8>> pins)> Function;
	TMap<FGuid, std::string> Params;
	std::function<flatbuffers::Offset<mz::fb::Node>(flatbuffers::FlatBufferBuilder& fbb)> Serialize;
};





#endif