#include "MZType.h"
#include "MZClient.h"
#include "Core.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"

#include "Misc/MessageDialog.h"

#include <map>

static TMap<FName, MZType*> GTypeMap;

static TMap<EName, std::string> UE_2_MZ_TYPE =
{
    {NAME_Vector4,  "mz.proto.vec4d"},
    {NAME_Vector,   "mz.proto.vec3d"},
    {NAME_Vector2D, "mz.proto.vec2d"},
};

#pragma optimize("", off)
bool MZType::Init(FField* Field)
{
    FieldClass = Field->GetClass();
    
    if (auto oprop = CastField<FObjectProperty>(Field))
    {
        if (oprop->PropertyClass == UTextureRenderTarget2D::StaticClass())
        {
            Tag = TRT2D;
            TypeName = "mz.proto.Texture";
        }
        else
        {
            abort();
        }
    }
    else if (auto sprop = CastField<FStructProperty>(Field))
    {
        Tag = STRUCT;

        EName name = *sprop->Struct->GetFName().ToEName();

        if (std::string* pMzName = UE_2_MZ_TYPE.Find(name))
        {
            TypeName = *pMzName;
        }
        else
        {
            return false;
        }

        FField* member = sprop->Struct->ChildProperties;

        while (member)
        {
            StructFields.Add(Member{ member, GetType(member) });
            member = member->Next;
        }
    }
    else if (auto aprop = CastField<FArrayProperty>(Field))
    {
        Tag = ARRAY;
        ElementCount = aprop->ArrayDim;
        ElementType = GetType(aprop->Inner);
    }
    else if (auto nprop = CastField<FNumericProperty>(Field))
    {
        Tag = (nprop->IsFloatingPoint() ? FLOAT : INT);
        Width = nprop->ElementSize * 8;
    }
    else if (CastField<FBoolProperty>(Field))
    {
        Tag = BOOL;
        Width = 1;
    }
    else if (CastField<FStrProperty>(Field))
    {
        FStrProperty::StaticClassCastFlags();
        Tag = STRING;
    }
    return true;
}


FName GetHash(FField* Field)
{
    if (Field->HasAnyCastFlags(CASTCLASS_FObjectProperty))
    {
        return ((FObjectProperty*)Field)->PropertyClass->GetFName();
    }
    if (Field->HasAnyCastFlags(CASTCLASS_FStructProperty))
    {
        return ((FStructProperty*)Field)->Struct->GetFName();
    }
    if (Field->HasAnyCastFlags(CASTCLASS_FArrayProperty))
    {
        FArrayProperty* Arr = ((FArrayProperty*)Field);
        return Arr->GetFName();
    }

    return Field->GetClass()->GetFName();
}


MZType* MZType::GetType(FField* Field)
{
    MZType*& ty = GTypeMap.FindOrAdd(GetHash(Field));

    if (!ty)
    {
        ty = new MZType();
        if (!ty->Init(Field))
        {
            delete ty;
            ty = nullptr;
        }
    }

    return ty;
}

#pragma optimize("", on)


EName MZEntity::GetType(FProperty* Field)
{
    FString name = Field->GetNameCPP();
    FString ty = Field->GetCPPType();

    if (Field->HasAnyCastFlags(CASTCLASS_FStructProperty))
    {
        return *((FStructProperty*)Field)->Struct->GetFName().ToEName();
    }

    return *Field->GetClass()->GetFName().ToEName();
}

MzTextureInfo MZEntity::GetResourceInfo() const
{
    UObject* obj = Entity->GetBoundObject();
    FObjectProperty* prop = CastField<FObjectProperty>(Property->GetProperty());
    
    UTextureRenderTarget2D* trt2d = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));

    MzTextureInfo info = {
        .width  = (uint32_t)trt2d->GetSurfaceWidth(),
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

FRHITexture2D* MZEntity::GetRHIResource() const
{
    UObject* obj = Entity->GetBoundObject();
    FObjectProperty* prop = CastField<FObjectProperty>(Property->GetProperty()); 
    UTextureRenderTarget2D* trt2d =  prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj);
    trt2d = Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(trt2d));
    FTextureRenderTargetResource* res = trt2d->GetRenderTargetResource();
    return res->GetTexture2DRHI();
}

ID3D12Resource* MZEntity::GetResource() const
{
    return (ID3D12Resource*)GetRHIResource()->GetNativeResource();
}

