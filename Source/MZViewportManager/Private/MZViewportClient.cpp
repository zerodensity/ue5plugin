#include "MZViewportClient.h"
#include "CanvasTypes.h"
#include "UObject/ObjectPtr.h"

FSimpleMulticastDelegate UMZViewportClient::MZViewportDestroyedDelegate;

void UMZViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);
	Super::OnViewportCreated().AddUObject(this, &UMZViewportClient::OnViewportCreated);
}

void UMZViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	float FrameTime = FPlatformTime::ToSeconds(FMath::Max3<uint32>(GRenderThreadTime, GGameThreadTime, GGPUFrameTime));
	const float FrameRate = FrameTime > 0 ? 1 / FrameTime : 0;
	bDisableWorldRendering = true;
	SceneCanvas->DrawTile(0, 0, ViewportTexture->SizeX, ViewportTexture->SizeY, 0, 0, 1, 1, FLinearColor::White, ViewportTexture->GetResource(), ESimpleElementBlendMode::SE_BLEND_Additive);

	Super::Draw(InViewport, SceneCanvas);
}

UMZViewportClient::~UMZViewportClient()
{
	//ViewportTexture->ReleaseResource();
	MZViewportDestroyedDelegate.Broadcast();
}

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
