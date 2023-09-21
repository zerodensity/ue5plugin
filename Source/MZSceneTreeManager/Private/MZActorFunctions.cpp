// Copyright MediaZ AS. All Rights Reserved.

#include "MZActorFunctions.h"


MZFunction::MZFunction(UObject* container, UFunction* function)
{
	Function = function;
	Container = container;
	Id = FGuid::NewGuid();

	static const FName NAME_DisplayName("DisplayName");
	static const FName NAME_Category("Category");
	FunctionName = function->GetFName().ToString();

	DisplayName = function->GetDisplayNameText().ToString();
	CategoryName = function->HasMetaData(NAME_Category) ? function->GetMetaData(NAME_Category) : "Default";
}

flatbuffers::Offset<mz::fb::Node> MZFunction::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins;
	for (auto property : Properties)
	{
		pins.push_back(property->Serialize(fbb));
	}
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*DisplayName), TCHAR_TO_UTF8(*Function->GetClass()->GetFName().ToString()), false, true, &pins, 0, mz::fb::NodeContents::Job, mz::fb::CreateJob(fbb, mz::fb::JobType::CPU).Union(), TCHAR_TO_ANSI(*FMZClient::AppKey), 0, TCHAR_TO_UTF8(*CategoryName));
}

void MZFunction::Invoke() // runs in game thread
{
	Container->Modify();
	Container->ProcessEvent(Function, Parameters);
}

void FillSpawnActorFunctionTransformPins(flatbuffers::FlatBufferBuilder& Fbb,
	std::vector<flatbuffers::Offset<mz::fb::Pin>>& SpawnPins,
	MZSpawnActorFunctionPinIds const& PinIds)
{
	SpawnPins.push_back(mz::fb::CreatePinDirect(Fbb, (mz::fb::UUID*)&PinIds.SpawnToWorldCoordsPinId,
	                                            TCHAR_TO_ANSI(TEXT("Spawn To World Coordinates")),
	                                            TCHAR_TO_ANSI(TEXT("bool")), mz::fb::ShowAs::PROPERTY,
	                                            mz::fb::CanShowAs::PROPERTY_ONLY, "UE PROPERTY", 0, 0, 0, 0, 0, 0, 0, 0,
	                                            0, 0, 0, 0, mz::fb::PinContents::JobPin, 0, 0, false,
	                                            mz::fb::PinValueDisconnectBehavior::KEEP_LAST_VALUE,
	                                            "Set actor spawn transform with respect to the world transform. If set to false, it keeps the relative transform with respect to the parent actor."));

	SpawnPins.push_back(mz::fb::CreatePinDirect(Fbb, (mz::fb::UUID*)&PinIds.SpawnLocationPinId,
												TCHAR_TO_ANSI(TEXT("Spawn Location")),
												TCHAR_TO_ANSI(TEXT("mz.fb.vec3d")), mz::fb::ShowAs::PROPERTY, mz::fb::CanShowAs::PROPERTY_ONLY, "UE PROPERTY", 0, 0, 0, 0, 0, 0, 0, 0,
												0, 0, 0, 0, mz::fb::PinContents::JobPin));
	
	SpawnPins.push_back(mz::fb::CreatePinDirect(Fbb, (mz::fb::UUID*)&PinIds.SpawnRotationPinId,
													TCHAR_TO_ANSI(TEXT("Spawn Rotation")),
													TCHAR_TO_ANSI(TEXT("mz.fb.vec3d")), mz::fb::ShowAs::PROPERTY, mz::fb::CanShowAs::PROPERTY_ONLY, "UE PROPERTY", 0, 0, 0, 0, 0, 0, 0, 0,
													0, 0, 0, 0, mz::fb::PinContents::JobPin));
	FVector3d SpawnScale = FVector3d(1, 1, 1);
	std::vector<uint8_t> SpawnScaleData((uint8_t*)&SpawnScale, (uint8_t*)&SpawnScale + sizeof(FVector3d));
	SpawnPins.push_back(mz::fb::CreatePinDirect(Fbb, (mz::fb::UUID*)&PinIds.SpawnScalePinId,
													TCHAR_TO_ANSI(TEXT("Spawn Scale")),
													TCHAR_TO_ANSI(TEXT("mz.fb.vec3d")), mz::fb::ShowAs::PROPERTY, mz::fb::CanShowAs::PROPERTY_ONLY, "UE PROPERTY", 0, &SpawnScaleData, 0, 0, 0, 0, 0, 0,
													0, 0, 0, 0, mz::fb::PinContents::JobPin));
}

MZSpawnActorParameters GetSpawnActorParameters(TMap<FGuid, std::vector<uint8>> const& Pins, MZSpawnActorFunctionPinIds const& PinIds)
{
	bool SpawnToWorldCoords = *(bool*)Pins.FindChecked(PinIds.SpawnToWorldCoordsPinId).data();
	FTransform SpawnTransform = FTransform::Identity;
	SpawnTransform.SetLocation(*(FVector*)Pins.FindChecked(PinIds.SpawnLocationPinId).data());
	SpawnTransform.SetRotation(FQuat::MakeFromEuler(*(FVector*)Pins.FindChecked(PinIds.SpawnRotationPinId).data()));
	SpawnTransform.SetScale3D(*(FVector*)Pins.FindChecked(PinIds.SpawnScalePinId).data());
	return { {}, SpawnToWorldCoords, SpawnTransform };
}
