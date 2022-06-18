#include "MZType.h"
#include "MZClient.h"
#include "Core.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Misc/MessageDialog.h"

#include <map>

static std::map<uint64_t, MZType*> GTypeMap;

static TMap<FName, std::string> UE_2_MZ_TYPE =
{
    {NAME_Vector4,  "mz.proto.vec4d"},
    {NAME_Vector,   "mz.proto.vec3d"},
    {NAME_Vector2D, "mz.proto.vec2d"},
};


bool MZType::Init(FField* Field)
{
    if (auto oprop = CastField<FObjectProperty>(Field))
    {
        if (oprop->PropertyClass == UTextureRenderTarget2D::StaticClass())
        {
            Tag = TRT2D;
            TypeName = "mz.proto.Texture";
        }
    }
    else if (auto sprop = CastField<FStructProperty>(Field))
    {
        TArray<FField*> fields;
        sprop->GetInnerFields(fields);
        Tag = STRUCT;

        FName name = sprop->Struct->GetFName();

        if (std::string* pMzName = UE_2_MZ_TYPE.Find(name))
        {
            TypeName = *pMzName;
        }
        else
        {
            return false;
        }

        for (auto field : fields)
        {
            if (auto fTy = GetType(field))
            {
                StructFields.Add(field->GetName(), fTy);
            } 
            else
            {
                return false;
            }
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
        Tag = STRING;
    }
    return true;
}

MZType* MZType::GetType(FField* Field)
{
    MZType*& ty = GTypeMap[Field->GetClass()->GetId()];

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


ID3D12Resource* MZEntity::GetResource() const
{
    UObject* obj = Entity->GetBoundObject();
    FObjectProperty* prop = CastField<FObjectProperty>(Property->GetProperty());
    return (ID3D12Resource*)Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)))->GetRenderTargetResource()->GetTextureRenderTarget2DResource()->GetTexture2DRHI()->GetNativeResource();
}

