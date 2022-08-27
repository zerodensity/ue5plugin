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
#include "MZType.h"
#include <any>


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
class MZParam;
class MZProperty;


template <class C> 
class Atemplate {
	C x;
public:
	Atemplate() { UE_LOG(LogTemp, Warning, TEXT("sdsds")); }
	template <class T>
	T* GetValue()
	{
		return nullptr;

	}
};


class MZCLIENT_API MZRemoteValue
{
public:
	virtual bool SetValue(void* data) = 0;
	virtual void* GetValue(EName _type) = 0;
	virtual flatbuffers::Offset<mz::fb::Pin> SerializeToFlatBuffer(flatbuffers::FlatBufferBuilder& fbb) = 0;

	virtual FProperty* GetProperty() = 0;

	UObject* GetObject() 
	{
		return Entity->GetBoundObject();
	}

	virtual MZParam* GetAsParam()
	{
		return nullptr;
	}

	virtual MZProperty* GetAsProp()
	{
		return nullptr;
	}
	
	EName GetType()
	{
		return type;
	}

	EName type = EName::None;
	FGuid id;
	FRemoteControlEntity* Entity = 0;
};


class MZCLIENT_API MZParam : public MZRemoteValue
{
public:
	MZParam(FRemoteControlFunction rFunction,
			EName type,
			FGuid id,
			FRemoteControlEntity* Entity, 
			FName name);
	MZParam* GetAsParam() override
	{
		return this;
	}

	FProperty* GetProperty() override
	{
		return rFunction.GetFunction()->FindPropertyByName(name);
	}

	std::vector<uint8_t> GetValue(FString& TypeName);
	bool SetValue(void* data);
	FString GetStringData();
	void* GetValue(EName _type);
	flatbuffers::Offset<mz::fb::Pin> SerializeToFlatBuffer(flatbuffers::FlatBufferBuilder& fbb);

private:
	FRemoteControlFunction rFunction;
	FName name;
};

class MZCLIENT_API MZProperty : public MZRemoteValue
{
public:
	MZProperty(TSharedPtr<IRemoteControlPropertyHandle> Property,
			   EName type,
			   FGuid id,
			   FRemoteControlEntity* Entity);
	MZProperty* GetAsProp()
	{
		return this;
	}
	std::vector<uint8_t> GetValue(FString& TypeName);
	bool SetValue(void* data);
	void* GetValue(EName _type);
	flatbuffers::Offset<mz::fb::Pin> SerializeToFlatBuffer(flatbuffers::FlatBufferBuilder& fbb);

	FProperty* GetProperty() override;

private:
	TSharedPtr<IRemoteControlPropertyHandle> Property = 0;

};

class MZCLIENT_API MZValueUtils
{
public:
	static MzTextureInfo GetResourceInfo(MZRemoteValue* mzrv);
	static ID3D12Resource* GetResource(MZRemoteValue* mzrv);
};

class MZCLIENT_API MZFunction
{
public:
	FGuid id;
	FRemoteControlFunction rFunction;
	std::vector<MZParam*> params;
	UObject* GetObject()
	{
		return rFunction.GetBoundObject();
	}
};