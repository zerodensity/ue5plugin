#include "MZUMGRendererComponent.h"
#include "EngineModule.h"
#include "Engine/TextureRenderTarget2D.h"


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
		WidgetRenderer = nullptr;
	}
	Super::OnUnregister();
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
		return;
	}
}
