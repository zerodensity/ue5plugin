/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MZViewportClient.generated.h"

//#define VIEWPORT_TEXTURE

UCLASS()
class MZVIEWPORTMANAGER_API UMZViewportClient : public UGameViewportClient
{
	GENERATED_BODY()
public:
	~UMZViewportClient() override;
	void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> ViewportTexture;
#ifdef VIEWPORT_TEXTURE
	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	static FSimpleMulticastDelegate MZViewportDestroyedDelegate;
private:
	void OnViewportCreated();
	void OnViewportResized(FViewport* viewport, uint32 val);
#endif
	virtual EMouseCaptureMode GetMouseCaptureMode() const override { return EMouseCaptureMode::NoCapture;  }
};
