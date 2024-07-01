// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NOSEditorTickableActor.generated.h"

UCLASS()
class NOSCLIENT_API ANOSEditorTickableActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ANOSEditorTickableActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual bool ShouldTickIfViewportsOnly() const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Reality")
	bool UseEditorTick = true;

	UFUNCTION(BlueprintImplementableEvent, CallInEditor, Category = "Reality")
	void BlueprintEditorTick(float DeltaTime);

};
