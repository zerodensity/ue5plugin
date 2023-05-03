// Copyright MediaZ AS. All Rights Reserved.

#include "MZViewportClient.h"
#include "CanvasTypes.h"
#include "UObject/ObjectPtr.h"

#ifdef VIEWPORT_TEXTURE
FSimpleMulticastDelegate UMZViewportClient::MZViewportDestroyedDelegate;

void UMZViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);
	Super::OnViewportCreated().AddUObject(this, &UMZViewportClient::OnViewportCreated);
}
#endif

void UMZViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	if (GetWorld()->WorldType == EWorldType::Game)
	{
		bDisableWorldRendering = true;
	}
#ifdef VIEWPORT_TEXTURE
	SceneCanvas->DrawTile(0, 0, ViewportTexture->SizeX, ViewportTexture->SizeY, 0, 0, 1, 1, FLinearColor::White, ViewportTexture->GetResource(), ESimpleElementBlendMode::SE_BLEND_Additive);
#endif
	Super::Draw(InViewport, SceneCanvas);
}
UMZViewportClient::~UMZViewportClient()
{
	//ViewportTexture->ReleaseResource();
#ifdef VIEWPORT_TEXTURE
	MZViewportDestroyedDelegate.Broadcast();
#endif
}
#ifdef VIEWPORT_TEXTURE
void UMZViewportClient::OnViewportCreated()
{
	ViewportTexture = NewObject<UTextureRenderTarget2D>(this);
	FVector2D viewportSize;
	Super::GetViewportSize(viewportSize);
	
	ViewportTexture->InitAutoFormat(viewportSize.X, viewportSize.Y);

	FViewport::ViewportResizedEvent.AddUObject(this, &UMZViewportClient::OnViewportResized);
}

void UMZViewportClient::OnViewportResized(FViewport* viewport, uint32 val)
{
	ViewportTexture->ResizeTarget(viewport->GetSizeXY().X, viewport->GetSizeXY().Y);
	UE_LOG(LogTemp, Warning, TEXT("Viewport Resized"));
}
#endif