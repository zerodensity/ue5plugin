#pragma once

// std
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

// UE
#include "Engine/EngineCustomTimeStep.h"

// MediaZ Plugin
#include "Editor.h"
#include "MZClient.h"
#include "MZCustomTimeStep.generated.h"


UCLASS()
class MZCLIENT_API UMZCustomTimeStep : public UEngineCustomTimeStep
{
	GENERATED_BODY()
public:

	//std::atomic<bool> wait = false;
	/** This CustomTimeStep became the Engine's CustomTimeStep. */
	bool Initialize(class UEngine* InEngine) override
	{
		return true;
	}

	/** This CustomTimeStep stop being the Engine's CustomTimeStep. */
	void Shutdown(class UEngine* InEngine) override
	{

	}

	void Step()
	{
		std::unique_lock lock(Mutex);
		IsReadyForNextStep = true;
		lock.unlock();
		CV.notify_one();
	}

	/**
	 * Update FApp::CurrentTime/FApp::DeltaTime and optionally wait until the end of the frame.
	 * @return	true if the Engine's TimeStep should also be performed; false otherwise.
	 */
	bool UpdateTimeStep(class UEngine* InEngine) override
	{
		UpdateApplicationLastTime();
		if (PluginClient && PluginClient->IsConnected() /*&& IsGameRunning()*/)
		{
			std::unique_lock lock(Mutex);
			CV.wait(lock, [this] { return IsReadyForNextStep; });
			IsReadyForNextStep = false;
		}
		return true;
	}

	/** The state of the CustomTimeStep. */
	ECustomTimeStepSynchronizationState GetSynchronizationState() const override
	{
		if (PluginClient && PluginClient->IsConnected())
		{
			return ECustomTimeStepSynchronizationState::Synchronized;
		}
		else
		{
			return ECustomTimeStepSynchronizationState::Closed;
		}
	}

	class FMZClient* PluginClient = nullptr;


private:
	bool IsGameRunning()
	{

			return (GEditor && GEditor->IsPlaySessionInProgress());
	}

	std::mutex Mutex;
	std::condition_variable CV;
	bool IsReadyForNextStep = false;
};

