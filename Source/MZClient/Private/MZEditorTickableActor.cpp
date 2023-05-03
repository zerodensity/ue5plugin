// Copyright MediaZ AS. All Rights Reserved.


#include "MZEditorTickableActor.h"

// Sets default values
AMZEditorTickableActor::AMZEditorTickableActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMZEditorTickableActor::BeginPlay()
{
	Super::BeginPlay();
	
}

void AMZEditorTickableActor::Tick(float DeltaTime)
{
#if WITH_EDITOR
	if (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::Editor)
	{
		BlueprintEditorTick(DeltaTime);
	}
	else
#endif
	{
		Super::Tick(DeltaTime);
	}
}


bool AMZEditorTickableActor::ShouldTickIfViewportsOnly() const
{
	if (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::Editor && UseEditorTick)
	{
		return true;
	}
	else
	{
		return false;
	}
}
