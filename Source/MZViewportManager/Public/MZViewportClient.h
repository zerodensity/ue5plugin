#pragma once
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MZViewportClient.generated.h"

UCLASS()
class MZVIEWPORTMANAGER_API UMZViewportClient : public UGameViewportClient
{
	GENERATED_BODY()
public:
	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;
	virtual ~UMZViewportClient();
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> ViewportTexture;
	static FSimpleMulticastDelegate MZViewportDestroyedDelegate;
private:
	void OnViewportCreated();
	void OnViewportResized(FViewport* viewport, uint32 val);
};
