/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Components/SceneComponent.h"
#include "SceneTypes.h"
#include "Engine/Scene.h"
#include "Slate/WidgetRenderer.h"
#include "Blueprint/UserWidget.h"
#include "MZUMGRendererComponent.generated.h"

class UTextureRenderTarget2D;

UCLASS(ClassGroup = "Media", BlueprintType, HideCategories = ("Rendering"), Meta = (BlueprintSpawnableComponent))
class MZSCENETREEMANAGER_API UMZUMGRendererComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render")
	TObjectPtr<UTextureRenderTarget2D> UMGRenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render")
	TObjectPtr<UUserWidget> Widget;

	TObjectPtr<FWidgetRenderer> WidgetRenderer;

	TSharedPtr<SWidget> SlateWidget;

	UMZUMGRendererComponent(); 

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	void FinishDestroy() override;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

};