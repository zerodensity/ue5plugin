// Copyright Epic Games, Inc. All Rights Reserved.


#include "MZClient.h"

DEFINE_LOG_CATEGORY(LogMZProto);

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

void MZEntity::SerializeToProto(mz::proto::Pin* pin) const
{
	if (Type)
	{
		FString id = Entity->GetId().ToString();
		FString label = Entity->GetLabel().ToString();
		mz::app::SetField(pin, mz::proto::Pin::kIdFieldNumber, TCHAR_TO_UTF8(*id));
		mz::app::SetField(pin, mz::proto::Pin::kDisplayNameFieldNumber, TCHAR_TO_UTF8(*label));
		mz::app::SetField(pin, mz::proto::Pin::kNameFieldNumber, TCHAR_TO_UTF8(*label));
		Type->SerializeToProto(pin, this);
	}
	else
	{
		UE_LOG(LogMZProto, Error, TEXT("Unknown Type"));
	}
}
