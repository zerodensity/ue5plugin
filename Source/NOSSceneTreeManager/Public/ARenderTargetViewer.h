/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ARenderTargetViewer.generated.h"


UCLASS()
class NOSSCENETREEMANAGER_API ARenderTargetViewer : public AActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Render")
	TObjectPtr<UTextureRenderTarget2D> RenderTargetView;

	FString RenderTargetAssetName;

	
	ARenderTargetViewer();

	void UpdateRenderTargetReference();

	virtual void BeginPlay() override;

};