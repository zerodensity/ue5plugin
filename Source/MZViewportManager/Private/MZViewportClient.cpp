// Copyright MediaZ AS. All Rights Reserved.

#include "MZViewportClient.h"
#include "CanvasTypes.h"
#include "UObject/ObjectPtr.h"

static TAutoConsoleVariable<bool> CVarMediazDisableViewport = TAutoConsoleVariable<bool>(
	TEXT("mediaz.viewport.disableViewport"),
	false,
	TEXT("Disables viewport rendering completely.\n")
	TEXT("Also disables debug and console rendering."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

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
	auto state = GetViewportRenderingState();
	if(state == MZViewportRenderingState::RENDERING_DISABLED_COMPLETELY)
		return;
	if (GetWorld()->WorldType == EWorldType::Game)
	{
		bDisableWorldRendering = state != MZViewportRenderingState::RENDER_DEFAULT_VIEWPORT;
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
UMZViewportClient::MZViewportRenderingState UMZViewportClient::GetViewportRenderingState() const
{
	return CVarMediazDisableViewport.GetValueOnGameThread() ? MZViewportRenderingState::RENDERING_DISABLED_COMPLETELY : MZViewportRenderingState::WORLD_RENDERING_DISABLED;
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