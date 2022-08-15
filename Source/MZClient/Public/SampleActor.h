#pragma once

#include "SampleActor.generated.h"

UCLASS(BlueprintType, Blueprintable, config = Engine, meta = (ShortTooltip = "olur boyle seyler"))
class MZCLIENT_API ASampleActor : public AActor
{
	GENERATED_BODY()
public:
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Default")
		void Olur();
};
