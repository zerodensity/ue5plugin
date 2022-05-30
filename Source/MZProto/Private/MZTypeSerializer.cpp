// Copyright Epic Games, Inc. All Rights Reserved.


#include "MZType.h"
#include "IRemoteControlPropertyHandle.h"
#include "RemoteControlPreset.h"

#include "DispelUnrealMadnessPrelude.h"
#include "google/protobuf/message.h"

#include "AppService.pb.h"

#undef INT
#undef FLOAT


template<class T>
auto GetValue(IRemoteControlPropertyHandle* p)
{
	T val;
	p->GetValue(val);
	return val;
}


void MZEntity::SerializeToProto(mz::app::AddPinRequest* req)
{
	
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

