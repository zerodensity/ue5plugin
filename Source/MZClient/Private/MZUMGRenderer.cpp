#include "MZUMGRenderer.h"
#include "Slate/WidgetRenderer.h"
#include "Blueprint/UserWidget.h"


UTextureRenderTarget2D* UMZUMGRenderer::WidgetToTexture(class UTextureRenderTarget2D* TextureRenderTarget, UUserWidget* const widget, const FVector2D& drawSize)
{
	// As long as the slate application is initialized and the widget passed in is not null continue...
	if (FSlateApplication::IsInitialized() && widget != nullptr)
	{
		// Get the slate widget as a smart pointer. Return if null.
		TSharedPtr<SWidget> SlateWidget(widget->TakeWidget());
		if (!SlateWidget) return nullptr;
		// Create a new widget renderer to render the widget to a texture render target 2D.
		FWidgetRenderer* WidgetRenderer = new FWidgetRenderer(true);
		if (!WidgetRenderer) return nullptr;
		// Update/Create the render target 2D.
		WidgetRenderer->DrawWidget(TextureRenderTarget, SlateWidget.ToSharedRef(), drawSize, 0, false);
		// Return the updated render target 2D.
		return TextureRenderTarget;
	}
	else return nullptr;
}