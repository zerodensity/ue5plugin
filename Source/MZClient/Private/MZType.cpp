#include "MZType.h"
#include "MZClient.h"
#include "Core.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"

#include "Misc/MessageDialog.h"

#include <map>


EName MZEntity::GetType(FProperty* Field)
{
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

UObject* MZEntity::GetObj() const
{
    UObject* obj = Entity->GetBoundObject();
    return obj;
}

UTextureRenderTarget2D* MZEntity::GetURT() const
{
    UObject* obj = GetObj();
    if (!obj)
    {
        return nullptr;
    }
    auto prop = CastField<FObjectProperty>(Property->GetProperty());
    return Cast<UTextureRenderTarget2D>(prop->GetObjectPropertyValue(prop->ContainerPtrToValuePtr<UTextureRenderTarget2D>(obj)));
}

FTextureRenderTargetResource* MZEntity::GetRT() const
{
    UTextureRenderTarget2D* urt = GetURT();
    if (!urt)
    {
        return nullptr;
    }
    return urt->GetRenderTargetResource();
}

FRHITexture2D* MZEntity::GetRHIResource() const
{
    if (auto RT = GetRT())
    {
        return RT->GetTexture2DRHI();
    }
    return nullptr;
}

ID3D12Resource* MZEntity::GetResource() const
{
    if (auto Res = GetRHIResource())
    {
        return (ID3D12Resource*)Res->GetNativeResource();
    }
    return nullptr;
}


void MZEntity::Transition(FRHICommandListImmediate& RHICmdList) const
{
    RHICmdList.TransitionResource(ERHIAccess::RTV, GetRHIResource());
}

void MZEntity::Transition(TArray<MZEntity> entities)
{
    ENQUEUE_RENDER_COMMAND(MZEntity_Transition)([&entities](FRHICommandListImmediate& RHICmdList) {
        for (auto& entity : entities)
        {
            if (entity.Type == EName::ObjectProperty)
            {
                entity.Transition(RHICmdList);
            }
        }
        });
    FlushRenderingCommands();
}