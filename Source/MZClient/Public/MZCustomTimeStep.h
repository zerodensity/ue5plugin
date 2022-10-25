#pragma once
#include "Engine/EngineCustomTimeStep.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#include <atomic>
#include <mutex>
#include <condition_variable>
#include "MZCustomTimeStep.generated.h"

UCLASS()
class UMZCustomTimeStep : public UEngineCustomTimeStep
{
	GENERATED_BODY()
public:

#if WITH_EDITOR
	std::mutex Mutex;
	std::condition_variable CV;
#endif
	//std::atomic<bool> wait = false;
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
		//if (IMZClient::Get()->IsConnected() && IsGameRunning())
		//{
		//	std::unique_lock lock(Mutex);
		//	CV.wait(lock);
		//}
		return true;
	}

	/** The state of the CustomTimeStep. */
	virtual ECustomTimeStepSynchronizationState GetSynchronizationState() const override
	{
		return ECustomTimeStepSynchronizationState::Synchronized;
	}

private:
	bool IsGameRunning()
	{
		#if WITH_EDITOR
			return (GEditor && GEditor->IsPlaySessionInProgress());
		#endif
			return true;
	}
};

