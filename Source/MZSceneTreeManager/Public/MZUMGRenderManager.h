/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "SceneTypes.h"
#include "Engine/Scene.h"
#include "Slate/WidgetRenderer.h"
#include "Blueprint/UserWidget.h"
#include "MZUMGRenderManager.generated.h"

UCLASS()
class MZSCENETREEMANAGER_API AMZUMGRenderManager : public AActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render")
	TObjectPtr<UTextureRenderTarget2D> UMGRenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render")
	TObjectPtr<UUserWidget> Widget;

	TObjectPtr<FWidgetRenderer> WidgetRenderer;

	TSharedPtr<SWidget> SlateWidget;

	AMZUMGRenderManager();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool ShouldTickIfViewportsOnly() const override;
	void Tick(float DeltaTime) override;

	void Destroyed() override;

};