// Copyright Epic Games, Inc. All Rights Reserved.


#include "MZClient.h"

DEFINE_LOG_CATEGORY(LogMZProto);

namespace UE::Math
{
	template<class T>
	using TVector3 = TVector<T>;
};

#define VectorValueSpec(T, N, P) \
template<> struct VectorValue<T, N> {\
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


template<class T, int N>
static void SetVectorValue(mz::proto::Pin* pin, const TSharedPtr<IRemoteControlPropertyHandle>& p)
{
	mz::proto::msg<typename VectorValue<T, N>::mz> m;
	typename VectorValue<T, N>::ue v;
	p->GetValue(v);
	m->set_x(v.X);
	m->set_y(v.Y);
	if constexpr (N >= 3) m->set_z(v.Z);
	if constexpr (N >= 4) m->set_w(v.W);
	mz::app::SetPin(pin, m.m_Ptr);
}

template<class T>
static void SetValue(mz::proto::Pin* pin, const TSharedPtr<IRemoteControlPropertyHandle>& p)
{
	using ValueType = decltype(T{}.val());
	mz::proto::msg<T> m;
	ValueType val;
	p->GetValue(val);
	m->set_val(val);
	mz::app::SetPin(pin, m.m_Ptr);
}

template<>
void SetValue<mz::proto::String>(mz::proto::Pin* pin, const TSharedPtr<IRemoteControlPropertyHandle>& p)
{
	mz::proto::msg<mz::proto::String> m;
	FString val;
	p->GetValue(val);
	mz::app::SetField(m.m_Ptr, m->kValFieldNumber, TCHAR_TO_UTF8(*val));
	mz::app::SetPin(pin, m.m_Ptr);
}


#pragma optimize("", off)
void MZType::SerializeToProto(mz::proto::Pin* pin, const MZEntity* e)
{
	switch (Tag)
	{
	case BOOL:	 
		SetValue<mz::proto::Bool>(pin, e->Property);
		break;

	case INT:	 
		switch (Width)
		{
		case 32: SetValue<mz::proto::i32>(pin, e->Property); break;
		case 64: SetValue<mz::proto::i64>(pin, e->Property); break;
		}
		break;
	case FLOAT:
		switch (Width)
		{
		case 32: SetValue<mz::proto::f32>(pin, e->Property); break;
		case 64: SetValue<mz::proto::f64>(pin, e->Property); break;
		}
		break;
	
	case STRING: {
		FString val;
		mz::proto::msg<mz::proto::String> m;
		e->Property->GetValue(val);

		mz::app::SetField(m.m_Ptr,  m->kValFieldNumber, TCHAR_TO_UTF8(*val));
		mz::app::SetPin(pin, m.m_Ptr);
	}
	break;
	case STRUCT:
	{
		break;
	}
	case TRT2D:
	{
		FMZClient::Get()->QueueTextureCopy(e->Entity->GetId(), e, pin);
		break;
	}
	}
}

#pragma optimize("", off)

void MZEntity::SerializeToProto(mz::proto::Pin* pin) const
{
	FString id = Entity->GetId().ToString();
	FString label = Entity->GetLabel().ToString();
	mz::app::SetField(pin, mz::proto::Pin::kIdFieldNumber, TCHAR_TO_UTF8(*id));
	mz::app::SetField(pin, mz::proto::Pin::kDisplayNameFieldNumber, TCHAR_TO_UTF8(*label));
	mz::app::SetField(pin, mz::proto::Pin::kNameFieldNumber, TCHAR_TO_UTF8(*label));
	
	switch (Type)
	{
	case EName::Vector4:           SetVectorValue<double, 4>  (pin, Property); break; 
	case EName::Vector:	           SetVectorValue<double, 3>  (pin, Property); break;
	case EName::Vector2d:          SetVectorValue<double, 2>  (pin, Property); break; 
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

#pragma optimize("", on)