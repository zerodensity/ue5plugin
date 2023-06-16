// Copyright MediaZ AS. All Rights Reserved.

#include "MZUMGRendererComponent.h"
#include "EngineModule.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Serialization/BufferArchive.h"
#include "ImageUtils.h"


UMZUMGRendererComponent::UMZUMGRendererComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// PrimaryComponentTick.TickGroup = ETickingGroup::TG_DuringPhysics;
	bTickInEditor = true;
}

void UMZUMGRendererComponent::OnRegister()
{
	Super::OnRegister();
	if (!IsTemplate())
	{
		{
			TObjectPtr<UTextureRenderTarget2D> RenderTarget2D = NewObject<UTextureRenderTarget2D>(this);
			RenderTarget2D->ClearColor = FLinearColor::Transparent;
			RenderTarget2D->InitCustomFormat(1920, 1080, PF_FloatRGBA, true);
			RenderTarget2D->UpdateResourceImmediate(true);
			UMGRenderTarget = RenderTarget2D;

			WidgetRenderer = new FWidgetRenderer(true);

		}
	}
}

void UMZUMGRendererComponent::OnUnregister()
{
	if (!IsTemplate())
	{
		UMGRenderTarget->ReleaseResource();
		Widget = nullptr;
		WidgetRenderer = nullptr;
		SlateWidget = nullptr;
	}
	Super::OnUnregister();
}

void UMZUMGRendererComponent::FinishDestroy()
{
	if (Widget) Widget = nullptr;
	if (WidgetRenderer) WidgetRenderer = nullptr;
	if (SlateWidget) SlateWidget = nullptr;
	if (UMGRenderTarget) UMGRenderTarget->ReleaseResource();

	Super::FinishDestroy();
}

void UMZUMGRendererComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UMZUMGRendererComponent* This = CastChecked<UMZUMGRendererComponent>(InThis);
	USceneComponent::AddReferencedObjects(This, Collector);
}

void UMZUMGRendererComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// As long as the slate application is initialized and the widget passed in is not null continue...
	if (FSlateApplication::IsInitialized() && Widget != nullptr)
	{
		// Get the slate widget as a smart pointer. Return if null.
		if (!SlateWidget)
		{
			SlateWidget = TSharedPtr<SWidget>(Widget->TakeWidget());
		}

		if (!SlateWidget) return;
		if (!WidgetRenderer) return;
		// Update/Create the render target 2D.
		WidgetRenderer->DrawWidget(UMGRenderTarget, SlateWidget.ToSharedRef(), FVector2D(UMGRenderTarget->SizeX, UMGRenderTarget->SizeY), DeltaTime, false);
		// Return the updated render target 2D.
		
		//for debug purposes writes texture to a file
		//TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*FString("TEXTURE_DEBUG_MZ.png")));
		//FBufferArchive Buffer;
		//bool bSuccess = FImageUtils::ExportRenderTarget2DAsPNG(UMGRenderTarget, Buffer);
		//if (bSuccess)
		//{
		//	Ar->Serialize(const_cast<uint8*>(Buffer.GetData()), Buffer.Num());
		//}

		return;
	}
}
