#pragma once

#include "IMZProto.h"
#include "CoreMinimal.h"
#include "RemoteControlField.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "UObject/Field.h"

#include <memory>
#include <string>

#include "mediaz.h"

namespace mz::app
{
	class AddPinRequest;
	class NodeUpdateRequest;
}

namespace mz::proto
{
	class Pin;
	class Texture;
	class Pin;
	class Node;
}

struct MZCLIENT_API MZType
{
	enum
	{
		BOOL,
		STRING,
		INT,
		FLOAT,
		ARRAY,
		STRUCT,
		TRT2D,
	} Tag;

	std::string TypeName;

	//Scalar
	uint32_t Width = 0;

	// Array
	MZType* ElementType = 0;
	uint32_t ElementCount = 0;

	//Struct
	TMap<FString, MZType*> StructFields;

	static MZType* GetType(FField*);
	void SerializeToProto(mz::proto::Pin* dyn, const struct MZEntity* p);
private:
	MZType() = default;
	bool Init(FField*);
};

struct MZCLIENT_API MZEntity
{
	MZType* Type = 0;
	FRemoteControlEntity* Entity = 0;
	TSharedPtr<IRemoteControlPropertyHandle> Property = 0;
	
	void SerializeToProto(mz::proto::Pin* req) const;

	MzTextureInfo GetResourceInfo() const;
	struct ID3D12Resource* GetResource() const;
};
