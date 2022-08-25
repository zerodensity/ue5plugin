#include "MZFunction.h"
#include "MZClient.h"
#include "Core.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"

#include "Misc/MessageDialog.h"

#include <map>


MZParam::MZParam(FRemoteControlFunction _rFunction,
	UObject* _object,
	EName _type,
	FGuid _id,
	FProperty* _fprop,
	FRemoteControlEntity* _Entity) : rFunction(_rFunction)
{
	object = _object;
	type = _type;
	id = _id;
	fprop = _fprop;
	Entity = _Entity;
}


bool MZParam::SetValue(void* val)
{
    switch (type)
    {
    case EName::Vector4: *(fprop->ContainerPtrToValuePtr<FVector4>(rFunction.FunctionArguments->GetStructMemory())) = *(FVector4*)val; break;
    case EName::Vector: *(fprop->ContainerPtrToValuePtr<FVector>(rFunction.FunctionArguments->GetStructMemory())) = *(FVector*)val; break;
    case EName::Vector2d: *(fprop->ContainerPtrToValuePtr<FVector2D>(rFunction.FunctionArguments->GetStructMemory())) = *(FVector2D*)val; break;
    case EName::Rotator: *(fprop->ContainerPtrToValuePtr<FRotator>(rFunction.FunctionArguments->GetStructMemory())) = *(FRotator*)val; break;
    case EName::FloatProperty:  *(fprop->ContainerPtrToValuePtr<float>(rFunction.FunctionArguments->GetStructMemory())) = *(float*)val; break;
    case EName::DoubleProperty: *(fprop->ContainerPtrToValuePtr<double>(rFunction.FunctionArguments->GetStructMemory())) = *(double*)val; break;
    case EName::Int32Property: *(fprop->ContainerPtrToValuePtr<int32_t>(rFunction.FunctionArguments->GetStructMemory())) = *(int32_t*)val; break;
    case EName::Int64Property: *(fprop->ContainerPtrToValuePtr<int64_t>(rFunction.FunctionArguments->GetStructMemory())) = *(int64_t*)val; break;
    case EName::BoolProperty: *(fprop->ContainerPtrToValuePtr<bool>(rFunction.FunctionArguments->GetStructMemory())) = *(bool*)val; break;
    case EName::StrProperty:  *(fprop->ContainerPtrToValuePtr<FString>(rFunction.FunctionArguments->GetStructMemory())) = FString((char*)val); break;
    default: UE_LOG(LogMZProto, Error, TEXT("Unknown Type")); break;
    }
    return true;

}

template<class T>
std::vector<uint8_t> GetValueAsBytes(void* value)
{
    std::vector<uint8_t> bytes(sizeof(T));
    memcpy(bytes.data(), value, sizeof(T));
    return bytes;

}

FString MZParam::GetStringData()
{
    return *(fprop->ContainerPtrToValuePtr<FString>(rFunction.FunctionArguments->GetStructMemory()));
}

std::vector<uint8_t> MZParam::GetValue(FString& TypeName)
{
    switch (type)
    {
    case EName::Matrix:            TypeName = "mz.fb.mat4d";
    {

    }
    case EName::Vector4:           TypeName = "mz.fb.vec4d"; return GetValueAsBytes<FVector4>(GetValue(EName::Vector4));
    case EName::Vector:	           TypeName = "mz.fb.vec3d"; return GetValueAsBytes<FVector>(GetValue(EName::Vector));
    case EName::Vector2d:          TypeName = "mz.fb.vec2d"; return GetValueAsBytes<FVector2D>(GetValue(EName::Vector2d));
    case EName::Rotator:           TypeName = "mz.fb.vec3d"; return GetValueAsBytes<FRotator>(GetValue(EName::Rotator));
    case EName::FloatProperty:     TypeName = "f32";        return GetValueAsBytes<float>(GetValue(EName::FloatProperty));
    case EName::DoubleProperty:    TypeName = "f64";        return GetValueAsBytes<double>(GetValue(EName::DoubleProperty));
    case EName::Int32Property:	   TypeName = "i32";        return GetValueAsBytes<int32_t>(GetValue(EName::Int32Property));
    case EName::Int64Property:	   TypeName = "i64";        return GetValueAsBytes<int64_t>(GetValue(EName::Int64Property));
    case EName::BoolProperty:	   TypeName = "bool";       return GetValueAsBytes<bool>(GetValue(EName::BoolProperty));
    case EName::StrProperty:
    {
        TypeName = "string";
        FString dat = GetStringData();
        auto s = StringCast<ANSICHAR>(*dat);
        std::vector<uint8_t> bytes(s.Length() + 1);
        memcpy(bytes.data(), s.Get(), s.Length());
        return bytes;
    }
    case EName::ObjectProperty:

        if (((FObjectProperty*)fprop)->PropertyClass->IsChildOf<UTextureRenderTarget2D>())
        {
            TypeName = "mz.fb.Texture";
            std::vector<uint8_t> re(sizeof(mz::fb::Texture));
            mz::fb::Texture tex = {};
            FMZClient::Get()->QueueTextureCopy(id, this, &tex);
            memcpy(re.data(), &tex, sizeof(tex));
            return re;
        }
        break;
    default: UE_LOG(LogMZProto, Error, TEXT("Unknown Type")); break;
    }
    return {};
}


flatbuffers::Offset<mz::fb::Pin> MZParam::SerializeToFlatBuffer(flatbuffers::FlatBufferBuilder& fbb)
{
   
    //FString label = Entity->GetLabel().ToString();
    FString label = fprop->GetFullName();
    mz::fb::ShowAs showAs = mz::fb::ShowAs::INPUT_PIN;

    //if (auto showAsValue = Entity->GetMetadata().Find("MZ_PIN_SHOW_AS_VALUE"))
    //{
    //    showAs = (mz::fb::ShowAs)FCString::Atoi(**showAsValue);
    //}
    //else
    //{
    //    Entity->SetMetadataValue("MZ_PIN_SHOW_AS_VALUE", FString::FromInt((u32)showAs));
    //}

    FString typeName = "mz.fb.Void";
    std::vector<uint8_t> data = GetValue(typeName);
    return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_ANSI(*label), TCHAR_TO_ANSI(*typeName), showAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, 0, mz::fb::Visualizer::NONE, &data);
}

MZProperty::MZProperty(TSharedPtr<IRemoteControlPropertyHandle> _Property,
	UObject* _object,
	EName _type,
	FGuid _id,
	FProperty* _fprop,
	FRemoteControlEntity* _Entity) : Property(_Property)
{
	object = _object;
	type = _type;
	id = _id;
	fprop = _fprop;
	Entity = _Entity;
}

bool MZProperty::SetValue(void* val)
{
    switch (type)
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
	return true;
}


void* MZParam::GetValue(EName _type)
{
    if (type != _type)
    {
        return nullptr;
    }
    switch (type)
    {
    case EName::ObjectProperty:
    {
        FObjectProperty* prop = CastField<FObjectProperty>(fprop);
        void* ValueAddress = prop->ContainerPtrToValuePtr< void >(rFunction.FunctionArguments->GetStructMemory());
        UObject* PropertyValue = prop->GetObjectPropertyValue(ValueAddress);
        return PropertyValue;
    };
    break;
    case EName::Vector:            return fprop->ContainerPtrToValuePtr<FVector>(rFunction.FunctionArguments->GetStructMemory());
    case EName::Vector4:           return fprop->ContainerPtrToValuePtr<FVector4>(rFunction.FunctionArguments->GetStructMemory());
    case EName::Vector2d:          return fprop->ContainerPtrToValuePtr<FVector2D>(rFunction.FunctionArguments->GetStructMemory());
    case EName::Rotator:           return fprop->ContainerPtrToValuePtr<FRotator>(rFunction.FunctionArguments->GetStructMemory());
    case EName::FloatProperty:     return fprop->ContainerPtrToValuePtr<float>(rFunction.FunctionArguments->GetStructMemory());
    case EName::DoubleProperty:    return fprop->ContainerPtrToValuePtr<double>(rFunction.FunctionArguments->GetStructMemory());
    case EName::Int32Property:	   return fprop->ContainerPtrToValuePtr<int32_t>(rFunction.FunctionArguments->GetStructMemory());
    case EName::Int64Property:	   return fprop->ContainerPtrToValuePtr<int64_t>(rFunction.FunctionArguments->GetStructMemory());
    case EName::BoolProperty:	   return fprop->ContainerPtrToValuePtr<bool>(rFunction.FunctionArguments->GetStructMemory());
    case EName::StrProperty:	   return fprop->ContainerPtrToValuePtr<FString>(rFunction.FunctionArguments->GetStructMemory());
    default:
        break;
    }
    return nullptr;


}

void* MZProperty::GetValue(EName _type)
{
    if (type != _type)
    {
        return nullptr;
    }
    switch (type)
    {   
        case EName::ObjectProperty:
        {
            FObjectProperty* prop = CastField<FObjectProperty>(fprop);
            void* ValueAddress = prop->ContainerPtrToValuePtr< void >(object);
            UObject* PropertyValue = prop->GetObjectPropertyValue(ValueAddress);
            return PropertyValue;
        };
            break;
        case EName::Vector:            return fprop->ContainerPtrToValuePtr<FVector>(object);
        case EName::Vector4:           return fprop->ContainerPtrToValuePtr<FVector4>(object);
        case EName::Vector2d:          return fprop->ContainerPtrToValuePtr<FVector2D>(object);
        case EName::Rotator:           return fprop->ContainerPtrToValuePtr<FRotator>(object);
        case EName::FloatProperty:     return fprop->ContainerPtrToValuePtr<float>(object);
        case EName::DoubleProperty:    return fprop->ContainerPtrToValuePtr<double>(object);
        case EName::Int32Property:	   return fprop->ContainerPtrToValuePtr<int32_t>(object);
        case EName::Int64Property:	   return fprop->ContainerPtrToValuePtr<int64_t>(object);
        case EName::BoolProperty:	   return fprop->ContainerPtrToValuePtr<bool>(object);
        case EName::StrProperty:	   return fprop->ContainerPtrToValuePtr<FString>(object);
        default:
            break;
    }
    return nullptr;

    
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

std::vector<uint8_t> MZProperty::GetValue(FString& TypeName)
{
    switch (type)
    {
    case EName::Matrix:            TypeName = "mz.fb.mat4d";
    {

    }
    case EName::Vector4:           TypeName = "mz.fb.vec4d"; return GetValueAsBytes<FVector4>(Property);
    case EName::Vector:	           TypeName = "mz.fb.vec3d"; return GetValueAsBytes<FVector>(Property);
    case EName::Vector2d:          TypeName = "mz.fb.vec2d"; return GetValueAsBytes<FVector2D>(Property);
    case EName::Rotator:           TypeName = "mz.fb.vec3d"; return GetValueAsBytes<FRotator>(Property);
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
        
        if (((FObjectProperty*)Property->GetProperty())->PropertyClass->IsChildOf<UTextureRenderTarget2D>())
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

flatbuffers::Offset<mz::fb::Pin> MZProperty::SerializeToFlatBuffer(flatbuffers::FlatBufferBuilder& fbb)
{
    FGuid idx = Entity->GetId();
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
    return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&idx, TCHAR_TO_ANSI(*label), TCHAR_TO_ANSI(*typeName), showAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, 0, mz::fb::Visualizer::NONE, &data);
}



MzTextureInfo MZValueUtils::GetResourceInfo(MZRemoteValue* mzrv)
{
    UObject* obj = mzrv->object;
    FObjectProperty* prop = CastField<FObjectProperty>(mzrv->fprop);

    UTextureRenderTarget2D* trt2d = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));

    MzTextureInfo info = {
        .width = (uint32_t)trt2d->GetSurfaceWidth(),
        .height = (uint32_t)trt2d->GetSurfaceHeight(),
        .usage = (MzImageUsage)(MZ_IMAGE_USAGE_RENDER_TARGET | MZ_IMAGE_USAGE_SAMPLED | MZ_IMAGE_USAGE_TRANSFER_SRC | MZ_IMAGE_USAGE_TRANSFER_DST),
    };

    switch (trt2d->RenderTargetFormat)
    {
    case ETextureRenderTargetFormat::RTF_R8:
        info.format = MZ_FORMAT_R8_UNORM;
        break;
    case ETextureRenderTargetFormat::RTF_RG8:
        info.format = MZ_FORMAT_R8G8_UNORM;
        break;
    case ETextureRenderTargetFormat::RTF_RGBA8:
        info.format = MZ_FORMAT_R8G8B8A8_UNORM;
        break;
    case ETextureRenderTargetFormat::RTF_RGBA8_SRGB:
        info.format = MZ_FORMAT_R8G8B8A8_SRGB;
        break;

    case ETextureRenderTargetFormat::RTF_R16f:
        info.format = MZ_FORMAT_R16_SFLOAT;
        break;
    case ETextureRenderTargetFormat::RTF_RG16f:
        info.format = MZ_FORMAT_R16G16_SFLOAT;
        break;
    case ETextureRenderTargetFormat::RTF_RGBA16f:
        info.format = MZ_FORMAT_R16G16B16A16_SFLOAT;
        break;

    case ETextureRenderTargetFormat::RTF_R32f:
        info.format = MZ_FORMAT_R32_SFLOAT;
        break;
    case ETextureRenderTargetFormat::RTF_RG32f:
        info.format = MZ_FORMAT_R32G32_SFLOAT;
        break;
    case ETextureRenderTargetFormat::RTF_RGBA32f:
        info.format = MZ_FORMAT_R32G32B32A32_SFLOAT;
        break;

    case ETextureRenderTargetFormat::RTF_RGB10A2:
        info.format = MZ_FORMAT_A2R10G10B10_UNORM_PACK32;
        break;
    }

    return info;
}


ID3D12Resource* MZValueUtils::GetResource(MZRemoteValue* mzrv)
{
    UObject* obj = mzrv->object;
    if (!obj) return nullptr;
    auto prop = CastField<FObjectProperty>(mzrv->fprop);
    if (!prop) return nullptr;
    auto urt = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));
    if (!urt) return nullptr;
    auto rt = urt->GetRenderTargetResource();
    if (!rt) return nullptr;
    auto rhires = rt->GetTexture2DRHI();
    if (!rhires) return nullptr;
    return (ID3D12Resource*)rhires->GetNativeResource();
}

