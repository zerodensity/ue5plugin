/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once
#include "MZActorProperties.h"

struct MZSCENETREEMANAGER_API MZFunction
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
};

struct MZSCENETREEMANAGER_API MZCustomFunction
{
	FGuid Id;
	std::function<void(TMap<FGuid, std::vector<uint8>> pins)> Function;
	TMap<FGuid, std::string> Params;
	std::function<flatbuffers::Offset<mz::fb::Node>(flatbuffers::FlatBufferBuilder& fbb)> Serialize;
};

struct MZSCENETREEMANAGER_API MZSpawnActorFunctionPinIds
{
	MZSpawnActorFunctionPinIds(FString UniqueFunctionName) : FunctionName(UniqueFunctionName)
	{
		ActorPinId = StringToFGuid(FunctionName + "Actor List");
		SpawnToWorldCoordsPinId = StringToFGuid(FunctionName + "Spawn To World Coordinates");
		SpawnLocationPinId = StringToFGuid(FunctionName + "Spawn Location");
		SpawnRotationPinId = StringToFGuid(FunctionName + "Spawn Rotation");
		SpawnScalePinId = StringToFGuid(FunctionName + "Spawn Scale");
	}
	
	FString FunctionName;
	FGuid ActorPinId;
	FGuid SpawnToWorldCoordsPinId;
	FGuid SpawnLocationPinId;
	FGuid SpawnRotationPinId;
	FGuid SpawnScalePinId;
};

struct MZSCENETREEMANAGER_API MZSpawnActorParameters
{
	bool SpawnActorToWorldCoords = false;
	FTransform SpawnTransform = FTransform::Identity;
};

void MZSCENETREEMANAGER_API FillSpawnActorFunctionTransformPins(flatbuffers::FlatBufferBuilder& Fbb,
                                                                std::vector<flatbuffers::Offset<mz::fb::Pin>>&SpawnPins,
                                                                MZSpawnActorFunctionPinIds const& PinIds);

MZSpawnActorParameters MZSCENETREEMANAGER_API GetSpawnActorParameters(TMap<FGuid, std::vector<uint8>> const& Pins, MZSpawnActorFunctionPinIds const& PinIds);