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

	UObject* object = 0;
	EName type = EName::None;
	FGuid id;
	FProperty* fprop = 0;
	FRemoteControlEntity* Entity = 0;

	//template <class T>
	//T* GetValue()
	//{
	//	if (type == EName::ObjectProperty)
	//	{
	//		FObjectProperty* prop = CastField<FObjectProperty>(fprop);
	//		void* ValueAddress = prop->ContainerPtrToValuePtr< void >(object);
	//		T* PropertyValue = prop->GetObjectPropertyValue(ValueAddress);
	//		return PropertyValue;
	//	//	FObjectProperty* prop = CastField<FObjectProperty>(fprop);
	//	//	if (!prop) return nullptr;
	//	//	return (T*)Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(object)));
	//	}
	//	else
	//	{
	//		//return nullptr;
	//		return fprop->ContainerPtrToValuePtr<T>(object);
	//	}

	//}


};


class MZCLIENT_API MZParam : public MZRemoteValue
{
public:
	MZParam(FRemoteControlFunction rFunction,
		UObject* object,
		EName type,
		FGuid id,
		FProperty* fprop,
		FRemoteControlEntity* Entity);
	MZParam* GetAsParam()
	{
		return this;
	}

	std::vector<uint8_t> GetValue(FString& TypeName);
	bool SetValue(void* data);
	FString GetStringData();
	void* GetValue(EName _type);

	flatbuffers::Offset<mz::fb::Pin> SerializeToFlatBuffer(flatbuffers::FlatBufferBuilder& fbb);
	

private:
	FRemoteControlFunction rFunction;

};

class MZCLIENT_API MZProperty : public MZRemoteValue
{
public:
	MZProperty(TSharedPtr<IRemoteControlPropertyHandle> Property,
		UObject* object,
		EName type,
		FGuid id,
		FProperty* fprop,
		FRemoteControlEntity* Entity);
	MZProperty* GetAsProp()
	{
		return this;
	}
	std::vector<uint8_t> GetValue(FString& TypeName);
	bool SetValue(void* data);
	void* GetValue(EName _type);
	

	flatbuffers::Offset<mz::fb::Pin> SerializeToFlatBuffer(flatbuffers::FlatBufferBuilder& fbb);
	

private:
	TSharedPtr<IRemoteControlPropertyHandle> Property = 0;

};

class MZCLIENT_API MZValueUtils
{
public:
	static MzTextureInfo GetResourceInfo(MZRemoteValue* mzrv);
	static ID3D12Resource* GetResource(MZRemoteValue* mzrv);
};

//class MZCLIENT_API MZParam
//{
//public:
//	EName Type = EName::None;
//	FRemoteControlEntity* Entity = 0;
//
//	FRemoteControlFunction rFunction;
//	FProperty* fprop = 0;
//	FGuid id;
//
//	flatbuffers::Offset<mz::fb::Pin> SerializeToProto(flatbuffers::FlatBufferBuilder& fbb);
//	void SetPropertyValue(void* val);
//
//	MzTextureInfo GetResourceInfo() const;
//	struct ID3D12Resource* GetResource() const;
//	FRHITexture2D* GetRHIResource() const;
//	FTextureRenderTargetResource* GetRT() const;
//	UObject* GetObj() const;
//	UObject* GetValue() const;
//	UTextureRenderTarget2D* GetURT() const;
//
//
//	bool IsTRT2D() const;
//
//	void Transition(FRHICommandListImmediate& RHICmdList) const;
//
//	std::vector<uint8_t> GetValue(FString& TypeName);
//};

class MZCLIENT_API MZFunction
{
public:
	FGuid id;
	UObject* object = 0;
	FRemoteControlFunction rFunction;
	std::vector<MZParam*> params;

};