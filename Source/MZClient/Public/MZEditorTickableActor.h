// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MZEditorTickableActor.generated.h"

UCLASS()
class MZCLIENT_API AMZEditorTickableActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMZEditorTickableActor();

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
