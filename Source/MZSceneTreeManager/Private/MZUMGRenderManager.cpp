// Copyright MediaZ AS. All Rights Reserved.

#include "MZUMGRenderManager.h"
#include "EngineModule.h"
#include "Engine/TextureRenderTarget2D.h"

AMZUMGRenderManager::AMZUMGRenderManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// PrimaryComponentTick.TickGroup = ETickingGroup::TG_DuringPhysics;
	

	if (!IsTemplate())
	{
		{
			TObjectPtr<UTextureRenderTarget2D> RenderTarget2D = NewObject<UTextureRenderTarget2D>(this, *FString("target_defo_2d_babbba"));
			RenderTarget2D->ClearColor = FLinearColor::Transparent;
			RenderTarget2D->InitCustomFormat(1920, 1080, PF_FloatRGBA, true);
			RenderTarget2D->UpdateResourceImmediate(true);
			UMGRenderTarget = RenderTarget2D;
			WidgetRenderer = new FWidgetRenderer(true);
		}
	}
}

void AMZUMGRenderManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!IsTemplate())
	{
		UMGRenderTarget->ReleaseResource();
		Widget = nullptr;
		WidgetRenderer = nullptr;
		SlateWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool AMZUMGRenderManager::ShouldTickIfViewportsOnly() const
{
	if (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::Editor)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void AMZUMGRenderManager::Tick(float DeltaTime)
{
	if (FSlateApplication::IsInitialized() && Widget != nullptr)
	{
		if (!SlateWidget)
		{
			SlateWidget = TSharedPtr<SWidget>(Widget->TakeWidget());
		}

		if (!SlateWidget) return;

		if (!WidgetRenderer) return;

		WidgetRenderer->DrawWidget(UMGRenderTarget, SlateWidget.ToSharedRef(), FVector2D(UMGRenderTarget->SizeX, UMGRenderTarget->SizeY), DeltaTime, false);
		
		return;
	}
}

void AMZUMGRenderManager::Destroyed()
{
	if (!IsTemplate())
	{
		UMGRenderTarget->ReleaseResource();
		Widget = nullptr;
		WidgetRenderer = nullptr;
		SlateWidget = nullptr;
	}

	Super::Destroyed();
}
