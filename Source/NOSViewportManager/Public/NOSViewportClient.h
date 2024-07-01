/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "NOSViewportClient.generated.h"

//#define VIEWPORT_TEXTURE

UCLASS()
class NOSVIEWPORTMANAGER_API UNOSViewportClient : public UGameViewportClient
{
	GENERATED_BODY()
public:
	~UNOSViewportClient() override;
	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;

#ifdef VIEWPORT_TEXTURE
	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
#endif

	virtual EMouseCaptureMode GetMouseCaptureMode() const override { return EMouseCaptureMode::NoCapture; }

#ifdef VIEWPORT_TEXTURE
private:
	void OnViewportCreated();
	void OnViewportResized(FViewport* viewport, uint32 val);
#endif

protected:
	enum NOSViewportRenderingState
	{
		RENDERING_DISABLED_COMPLETELY,
		WORLD_RENDERING_DISABLED,
		RENDER_DEFAULT_VIEWPORT
	};
	virtual NOSViewportRenderingState GetViewportRenderingState() const;

public:
	// it's not inside #ifdef VIEWPORT_TEXTURE, because of "UPROPERTY must not be inside preprocessor blocks" error
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> ViewportTexture;

#ifdef VIEWPORT_TEXTURE
	static FSimpleMulticastDelegate NOSViewportDestroyedDelegate;
#endif
};
