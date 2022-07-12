// Copyright Epic Games, Inc. All Rights Reserved.


#include "MZClient.h"

DEFINE_LOG_CATEGORY(LogMZProto);

namespace UE::Math
{
	template<class T>
	using TVector3 = TVector<T>;
};

#define Stringify(x) #x
#define ConcatStringify(a,b,c) Stringify(a##b##c)

#define VectorValueSpec(T, N, P) \
template<> struct VectorValue<T, N> {\
	static constexpr const char* mzName = ConcatStringify(mz.proto.vec,N,P); \
	using mz = mz::proto::vec##N##P; \
	using ue = UE::Math::TVector##N<T>; \
};

#define VectorValueSpecN(T, P) \
	VectorValueSpec(T, 2, P) \
	VectorValueSpec(T, 3, P) \
	VectorValueSpec(T, 4, P) 

template<class T, int n> struct VectorValue;

VectorValueSpecN(float, );
VectorValueSpecN(double, d);


#pragma optimize("", off)

template<class T, int N>
static void SetVectorValue(mz::proto::Pin* pin, const TSharedPtr<IRemoteControlPropertyHandle>& p)
{
	if (sizeof(typename VectorValue<T, N>::ue) != (sizeof(T) * N))
	{
		abort();
	}

	typename VectorValue<T, N>::ue val;
	p->GetValue(val);
	mz::app::SetField(pin, mz::proto::Pin::kTypeNameFieldNumber, VectorValue<T, N>::mzName);
	mz::app::SetBytes(pin, &val, sizeof(typename VectorValue<T, N>::ue));
}

template<class T>
static void SetValue(mz::proto::Pin* pin, const TSharedPtr<IRemoteControlPropertyHandle>& p)
{
	using ValueType = decltype(T{}.val());
	static mz::proto::msg<T> m;
	ValueType val;
	p->GetValue(val);
	
	mz::app::SetField(pin, mz::proto::Pin::kTypeNameFieldNumber, m->GetTypeName().c_str());
	mz::app::SetBytes(pin, &val, sizeof(val));
}

template<>
void SetValue<mz::proto::String>(mz::proto::Pin* pin, const TSharedPtr<IRemoteControlPropertyHandle>& p)
{
	FString val;
	p->GetValue(val);
	size_t len = val.Len();
	mz::app::SetField(pin, mz::proto::Pin::kTypeNameFieldNumber, "mz.proto.String");
	mz::app::SetBytes(pin, TCHAR_TO_UTF8(*val), len);
}

void SetRotator(mz::proto::Pin* pin, const TSharedPtr<IRemoteControlPropertyHandle>& p)
{
	FRotator val;
	p->GetValue(val);
	mz::app::SetField(pin, mz::proto::Pin::kTypeNameFieldNumber, "mz.proto.vec3d");
	mz::app::SetBytes(pin, &val, sizeof(FRotator));
}

void MZEntity::SerializeToProto(mz::proto::Pin* pin) const
{
	FString id = Entity->GetId().ToString();
	FString label = Entity->GetLabel().ToString();
	mz::proto::ShowAs showAs = mz::proto::ShowAs::OUTPUT_PIN;

	if (auto showAsValue = Entity->GetMetadata().Find("MZ_PIN_SHOW_AS_VALUE"))
	{
		showAs = (mz::proto::ShowAs)FCString::Atoi(**showAsValue);
	}
	else
	{
		Entity->SetMetadataValue("MZ_PIN_SHOW_AS_VALUE", FString::FromInt(showAs));
	}

	pin->set_show_as(showAs);

	mz::app::SetField(pin, mz::proto::Pin::kIdFieldNumber, TCHAR_TO_UTF8(*id));
	mz::app::SetField(pin, mz::proto::Pin::kDisplayNameFieldNumber, TCHAR_TO_UTF8(*label));
	mz::app::SetField(pin, mz::proto::Pin::kNameFieldNumber, TCHAR_TO_UTF8(*label));
	
	switch (Type)
	{
	case EName::Vector4:           SetVectorValue<double, 4>  (pin, Property); break; 
	case EName::Vector:	           SetVectorValue<double, 3>  (pin, Property); break;
	case EName::Vector2d:          SetVectorValue<double, 2>  (pin, Property); break; 
	case EName::Rotator:           SetRotator				  (pin, Property); break;
	case EName::FloatProperty:     SetValue<mz::proto::f32>   (pin, Property); break;
	case EName::DoubleProperty:    SetValue<mz::proto::f64>   (pin, Property); break;
	case EName::Int32Property:	   SetValue<mz::proto::i32>   (pin, Property); break;
	case EName::Int64Property:	   SetValue<mz::proto::i64>   (pin, Property); break;
	case EName::BoolProperty:	   SetValue<mz::proto::Bool>  (pin, Property); break;
	case EName::StrProperty:       SetValue<mz::proto::String>(pin, Property); break;
	case EName::ObjectProperty:
		if (((FObjectProperty*)Property->GetProperty())->PropertyClass->IsChildOf<UTextureRenderTarget2D>())
		{
			FMZClient::Get()->QueueTextureCopy(Entity->GetId(), this, pin);
			break;
		}
	default: UE_LOG(LogMZProto, Error, TEXT("Unknown Type")); break;
	}
}

void MZEntity::SetPropertyValue(void* val)
{
	switch (Type)
	{
	case EName::Vector4: Property->SetValue(*(FVector4*)val); break;
	case EName::Vector: Property->SetValue(*(FVector*)val); break;
	case EName::Vector2d: Property->SetValue(*(FVector2D*)val); break;
	case EName::Rotator: Property->SetValue(*(FRotator*)val); break;
	case EName::FloatProperty:  Property->SetValue(*(float*)val); break;
	case EName::DoubleProperty: Property->SetValue(*(double*)val); break;
	case EName::Int32Property: Property->SetValue(*(int32_t*)val); break;
	case EName::Int64Property: Property->SetValue(*(int64_t*)val); break;
	case EName::BoolProperty: Property->SetValue(*(bool*)val); break;
	case EName::StrProperty:  Property->SetValue(UTF8_TO_TCHAR(((char*)val))); break;
	default: UE_LOG(LogMZProto, Error, TEXT("Unknown Type")); break;
	}
}

#pragma optimize("", on)