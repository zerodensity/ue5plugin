// Copyright Epic Games, Inc. All Rights Reserved.


#include "MZClient.h"

DEFINE_LOG_CATEGORY(LogMZProto);


flatbuffers::Offset<mz::fb::Pin> MZEntity::SerializeToProto(flatbuffers::FlatBufferBuilder& fbb) const
{
	FGuid id = Entity->GetId();
	FString label = Entity->GetLabel().ToString();
	mz::fb::ShowAs showAs = mz::fb::ShowAs::OUTPUT_PIN;

	if (auto showAsValue = Entity->GetMetadata().Find("MZ_PIN_SHOW_AS_VALUE"))
	{
		showAs = (mz::fb::ShowAs)FCString::Atoi(**showAsValue);
	}
	else
	{
		Entity->SetMetadataValue("MZ_PIN_SHOW_AS_VALUE", FString::FromInt((u32)showAs));
	}

	FString typeName = "mz.fb.Void";
	std::vector<uint8_t> data = GetValue(typeName);
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_ANSI(*label), TCHAR_TO_ANSI(*typeName), showAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, 0, &data);
}


template<class T> 
std::vector<uint8_t> GetValueAsBytes(const TSharedPtr<IRemoteControlPropertyHandle>& p)
{
	T dat;
	p->GetValue(dat);
	std::vector<uint8_t> bytes(sizeof(T));
	memcpy(bytes.data(), &dat, sizeof(T));
	return bytes;
}

std::vector<uint8_t> MZEntity::GetValue(FString& TypeName) const
{
	switch (Type)
	{
	case EName::Matrix:            TypeName = "mz.fb.mat4d";
	{
		
	}
	case EName::Vector4:           TypeName = "mz.fb.vec4d";return GetValueAsBytes<FVector4>(Property);
	case EName::Vector:	           TypeName = "mz.fb.vec3d";return GetValueAsBytes<FVector>(Property);
	case EName::Vector2d:          TypeName = "mz.fb.vec2d";return GetValueAsBytes<FVector2D>(Property);
	case EName::Rotator:           TypeName = "mz.fb.vec3d";return GetValueAsBytes<FRotator>(Property);
	case EName::FloatProperty:     TypeName = "f32";        return GetValueAsBytes<float>(Property);
	case EName::DoubleProperty:    TypeName = "f64";        return GetValueAsBytes<double>(Property);
	case EName::Int32Property:	   TypeName = "i32";        return GetValueAsBytes<int32_t>(Property);
	case EName::Int64Property:	   TypeName = "i64";        return GetValueAsBytes<int64_t>(Property);
	case EName::BoolProperty:	   TypeName = "bool";       return GetValueAsBytes<bool>(Property);
	case EName::StrProperty:       
	{
		TypeName = "string";
		FString dat;
		Property->GetValue(dat);
		auto s = StringCast<ANSICHAR>(*dat);
		std::vector<uint8_t> bytes(s.Length() + 1);
		memcpy(bytes.data(), s.Get(), s.Length());
		return bytes;
	}
	case EName::ObjectProperty:
		if (IsTRT2D())
		{
			TypeName = "mz.fb.Texture";
			std::vector<uint8_t> re(sizeof(mz::fb::Texture));
			mz::fb::Texture tex = {};
			FMZClient::Get()->QueueTextureCopy(Entity->GetId(), this, &tex);
			memcpy(re.data(), &tex, sizeof(tex));
			return re;
		}
		break;
	default: UE_LOG(LogMZProto, Error, TEXT("Unknown Type")); break;
	}
	return {};
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