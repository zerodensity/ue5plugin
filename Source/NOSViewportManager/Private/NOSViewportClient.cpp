// Copyright MediaZ AS. All Rights Reserved.

#include "NOSViewportClient.h"
#include "CanvasTypes.h"
#include "UObject/ObjectPtr.h"

static TAutoConsoleVariable<bool> CVarNodosDisableViewport = TAutoConsoleVariable<bool>(
	TEXT("Nodos.viewport.disableViewport"),
	false,
	TEXT("Disables viewport rendering completely.\n")
	TEXT("Also disables debug and console rendering."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

#ifdef VIEWPORT_TEXTURE
FSimpleMulticastDelegate UNOSViewportClient::NOSViewportDestroyedDelegate;

void UNOSViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);
	Super::OnViewportCreated().AddUObject(this, &UNOSViewportClient::OnViewportCreated);
}
#endif

void UNOSViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	auto state = GetViewportRenderingState();
	if(state == NOSViewportRenderingState::RENDERING_DISABLED_COMPLETELY)
		return;

	bDisableWorldRendering = state != NOSViewportRenderingState::RENDER_DEFAULT_VIEWPORT;

#ifdef VIEWPORT_TEXTURE
	SceneCanvas->DrawTile(0, 0, ViewportTexture->SizeX, ViewportTexture->SizeY, 0, 0, 1, 1, FLinearColor::White, ViewportTexture->GetResource(), ESimpleElementBlendMode::SE_BLEND_Additive);
#endif
	Super::Draw(InViewport, SceneCanvas);
}
UNOSViewportClient::~UNOSViewportClient()
{
	//ViewportTexture->ReleaseResource();
#ifdef VIEWPORT_TEXTURE
	NOSViewportDestroyedDelegate.Broadcast();
#endif
}
UNOSViewportClient::NOSViewportRenderingState UNOSViewportClient::GetViewportRenderingState() const
{
	return CVarNodosDisableViewport.GetValueOnGameThread() ? NOSViewportRenderingState::RENDERING_DISABLED_COMPLETELY : NOSViewportRenderingState::WORLD_RENDERING_DISABLED;
}
#ifdef VIEWPORT_TEXTURE
void UNOSViewportClient::OnViewportCreated()
{
	ViewportTexture = NewObject<UTextureRenderTarget2D>(this);
	FVector2D viewportSize;
	Super::GetViewportSize(viewportSize);
	
	ViewportTexture->InitAutoFormat(viewportSize.X, viewportSize.Y);

	FViewport::ViewportResizedEvent.AddUObject(this, &UNOSViewportClient::OnViewportResized);
}

void UNOSViewportClient::OnViewportResized(FViewport* viewport, uint32 val)
{
	ViewportTexture->ResizeTarget(viewport->GetSizeXY().X, viewport->GetSizeXY().Y);
	UE_LOG(LogTemp, Warning, TEXT("Viewport Resized"));
}
#endif