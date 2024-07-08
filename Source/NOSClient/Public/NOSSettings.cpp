#include "NOSSettings.h"
#include "Interfaces/IPluginManager.h"
#include "NOSClient.h"

UNOSSettings::UNOSSettings(const FObjectInitializer& ObjectInitializer)
{

}

#if WITH_EDITOR
void UNOSSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	auto NOSClient = &FModuleManager::LoadModuleChecked<FNOSClient>("NOSClient");
	if(!NOSClient->bIsInitialized)
	{
		if (FNodos::Initialize())
		{
			NOSClient->Initialize();
		}
	}
}
#endif // WITH_EDITOR

