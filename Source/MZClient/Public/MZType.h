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

#include <mzFlatBuffersCommon.h>

namespace mz
{
	struct NodeUpdate;
}

namespace mz::fb
{
	struct Pin;
	struct Texture;
	struct Node;
	enum class ShowAs:uint32_t;
}

struct MZCLIENT_API MZEntity
{
	EName Type = EName::None;
	FRemoteControlEntity* Entity = 0;
	TSharedPtr<IRemoteControlPropertyHandle> Property = 0;

	flatbuffers::Offset<mz::fb::Pin> SerializeToProto(flatbuffers::FlatBufferBuilder& fbb) const;
	void SetPropertyValue(void* val);

	MzTextureInfo GetResourceInfo() const;
	struct ID3D12Resource* GetResource() const;
	FRHITexture2D* GetRHIResource() const;
	FTextureRenderTargetResource* GetRT() const;
	UObject* GetObj() const;
	UObject* GetValue() const;
	UTextureRenderTarget2D* GetURT() const;
	
	static EName GetType(FProperty* Field);
	bool IsTRT2D() const;

	void Transition(FRHICommandListImmediate& RHICmdList) const;
	static void Transition(TArray<MZEntity> entities);

	std::vector<uint8_t> GetValue(FString& TypeName) const;
};
