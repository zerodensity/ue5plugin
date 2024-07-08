#pragma once

#include "Engine/DeveloperSettings.h"
#include "NOSSettings.generated.h"

#define GET_NOS_CONFIG_VALUE(a) (GetDefault<UNOSSettings>()->a)

/**
 * Implements the settings for the Nodos Link plugin.
 */
UCLASS(config=EditorSettings, DisplayName="Nodos Link Settings")
class NOSCLIENT_API UNOSSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNOSSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	UPROPERTY(EditAnywhere, config, Category = NodosLink)
	FString NosmanPath;
};
