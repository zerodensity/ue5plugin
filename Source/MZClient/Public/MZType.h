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
	enum ShowAs;
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

	FFieldClass* FieldClass = 0;

	std::string TypeName;

	//Scalar
	uint32_t Width = 0;

	// Array
	MZType* ElementType = 0;
	uint32_t ElementCount = 0;

	struct Member
	{
		FField* Field;
		MZType* Type;
	};
	//Struct
	TArray<Member> StructFields;

	static MZType* GetType(FField*);
	void SerializeToProto(mz::proto::Pin* dyn, const struct MZEntity* p);
private:
	MZType() = default;
	bool Init(FField*);
};

struct MZCLIENT_API MZEntity
{
	EName Type = EName::None;
	FRemoteControlEntity* Entity = 0;
	TSharedPtr<IRemoteControlPropertyHandle> Property = 0;
	
	void SerializeToProto(mz::proto::Pin* req) const;
	void SetPropertyValue(void* val);

	MzTextureInfo GetResourceInfo() const;
	struct ID3D12Resource* GetResource() const;
	FRHITexture2D* GetRHIResource() const;
	FTextureRenderTargetResource* GetRT() const;
	
	static EName GetType(FProperty* Field);

	void Transition(FRHICommandListImmediate& RHICmdList) const;
	static void Transition(TArray<MZEntity> entities);
};
