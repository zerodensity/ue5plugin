#pragma once

#include "Engine/EngineCustomTimeStep.h"

#include "MZCustomTimeStep.generated.h"

UCLASS()
class UMZCustomTimeStep : public UEngineCustomTimeStep
{
	GENERATED_BODY()
public:
	/** This CustomTimeStep became the Engine's CustomTimeStep. */
	virtual bool Initialize(class UEngine* InEngine) override
	{
		return true;
	}

	/** This CustomTimeStep stop being the Engine's CustomTimeStep. */
	virtual void Shutdown(class UEngine* InEngine) override
	{

	}

	/**
	 * Update FApp::CurrentTime/FApp::DeltaTime and optionally wait until the end of the frame.
	 * @return	true if the Engine's TimeStep should also be performed; false otherwise.
	 */
	virtual bool UpdateTimeStep(class UEngine* InEngine) override
	{
		UpdateApplicationLastTime();
		FApp::SetDeltaTime(FApp::GetDeltaTime()/128.0);
		return true;
	}

	/** The state of the CustomTimeStep. */
	virtual ECustomTimeStepSynchronizationState GetSynchronizationState() const override
	{
		return ECustomTimeStepSynchronizationState::Synchronized;
	}

};
