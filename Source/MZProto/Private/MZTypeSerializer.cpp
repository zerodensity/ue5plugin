// Copyright Epic Games, Inc. All Rights Reserved.


#include "MZType.h"
#include "IRemoteControlPropertyHandle.h"
#include "RemoteControlPreset.h"

#include "DispelUnrealMadnessPrelude.h"
#include "google/protobuf/message.h"

#include <Arena.h>
#include "AppService.pb.h"

#undef INT
#undef FLOAT

DEFINE_LOG_CATEGORY(LogMZProto);

template<class T>
static void SetValue(mz::proto::Dynamic* dyn, IRemoteControlPropertyHandle* p)
{
	using ValueType = decltype(T{}.val());

	mz::proto::msg<T> m;
	ValueType val;

	p->GetValue(val);
	m->set_val(val);

	dyn->set_data(m->SerializeAsString());
	dyn->set_type(m->GetTypeName());
}


void MZType::SerializeToProto(mz::proto::Dynamic* dyn, IRemoteControlPropertyHandle* p)
{
	
	switch (Tag)
	{
	case BOOL:	 
		SetValue<mz::proto::Bool>(dyn, p);
		break;

	case INT:	 
		switch (Width)
		{
		case 32: SetValue<mz::proto::i32>(dyn, p); break;
		case 64: SetValue<mz::proto::i64>(dyn, p); break;
		}
		break;
	case FLOAT:
		switch (Width)
		{
		case 32: SetValue<mz::proto::f32>(dyn, p); break;
		case 64: SetValue<mz::proto::f64>(dyn, p); break;
		}
		break;
	
	case STRING: {
		FString val;
		mz::proto::msg<mz::proto::String> m;

		p->GetValue(val);
		m->set_val(TCHAR_TO_UTF8(*val));
		dyn->set_data(m->SerializeAsString());
		dyn->set_type(m->GetTypeName());
	}
	case STRUCT:
	{
		break;
	}

	}
}

void MZEntity::SerializeToProto(mz::proto::Dynamic* req)
{
	if (Type)
	{
		Type->SerializeToProto(req, Property.Get());
	}
	else
	{
		UE_LOG(LogMZProto, Error, TEXT("Unknown Type"));
	}
}


//
//void MZEntity::SerializeToProto(mz::proto::DynamicField* field)
//{
//	//field->set_name
//}
//
//void SerializeToProto(google::protobuf::Any* val, MZType* Type, IRemoteControlPropertyHandle* Property)
//{
//	switch (Type->Tag)
//	{
//	case MZType::BOOL:		val->set_value(GetValue<bool>(Property));	 break;
//	case MZType::STRING:	val->set_value(GetValue<FString>(Property)); break;
//	case MZType::INT:
//		switch (Type->Width)
//		{
//		case 8:  val->set_value(GetValue<uint8_t >(Property)); break;
//		case 16: val->set_value(GetValue<uint16_t>(Property)); break;
//		case 32: val->set_value(GetValue<uint32_t>(Property)); break;
//		case 64: val->set_value(GetValue<uint64_t>(Property));
//		default: break;
//		}
//	case MZType::FLOAT:
//		switch (Type->Width)
//		{
//		case 32: val->set_value(GetValue<float>(Property)); break;
//		case 64: val->set_value(GetValue<double>(Property));
//		default: break;
//		}
//	case MZType::ARRAY: {
//		
//		//for (int i = 0; i < Type->ElementCount; ++i)
//		//{
//		//	auto element = Property->AsArray().Get()->GetElement(i);
//		//	SerializeToProto()
//		//}
//	}
//	case MZType::STRUCT:	val->set_value(GetValue<FString>(Property)); break;
//	default: break;
//	}
//}
//
//void MZEntity::SerializeToProto(google::protobuf::Any* val)
//{
//	//auto msg = std::make_shared<mz::proto::DynamicMessage>();
//	//field->set_name(Entity->GetLabel().ToString());
//}

