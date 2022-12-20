#pragma once

#include "Components/SceneComponent.h"
#include "SceneTypes.h"
#include "Engine/Scene.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MZUMGRenderer.generated.h"

class UTextureRenderTarget2D;

UCLASS()
class MZCLIENT_API UMZUMGRenderer : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/*Renders a widget to a Render Texture 2D with the given draw size.
		*
		*@param widget      The widget to converted to a Render Texture 2D.
		* @param drawSize    The size to render the Widget to.Also will be the texture size.
		* @return            The texture render target 2D containing the rendered widget.
	*/
	UFUNCTION(BlueprintCallable, Category = RenderTexture)
	static class UTextureRenderTarget2D* WidgetToTexture(class UTextureRenderTarget2D* TextureRenderTarget, class UUserWidget* const widget, const FVector2D & drawSize);

	
};

//
//UCLASS()
//class MZCLIENT_API AMZUMGRenderer :: public AActor
//{
//	GENERATE_BODY()
//public: 
//	TMap<FString, >
//
//};