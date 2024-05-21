#include "ARenderTargetViewer.h"
#include <NOSSceneTreeManager.h>


void ARenderTargetViewer::UpdateRenderTargetReference()
{
	
	auto NOSAssetManager = &FModuleManager::LoadModuleChecked<FNOSAssetManager>("NOSAssetManager");
	auto object = NOSAssetManager->FindRenderTarget(RenderTargetAssetName);
	if (auto texture = Cast<UTextureRenderTarget2D>(object))
	{
		RenderTargetView = texture;
	}
	else
	{
		RenderTargetView = nullptr;
	}
}

ARenderTargetViewer::ARenderTargetViewer()
{
	UpdateRenderTargetReference();
}

void ARenderTargetViewer::BeginPlay()
{
	UpdateRenderTargetReference();
}
