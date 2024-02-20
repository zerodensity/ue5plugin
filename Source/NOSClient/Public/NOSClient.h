/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Engine/EngineTypes.h"

#include "CoreMinimal.h"
#include <numeric>
#include "Logging/LogMacros.h"

#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

#include "Nodos/AppAPI.h"
#include <uuid.h>
#include "nosFlatBuffersCommon.h"
#include "AppEvents_generated.h"
#include <nosFlatBuffersCommon.h>
#include <functional> 

struct PinDataQueue : public TQueue<TPair<nos::Buffer, uint32_t>>
{
	bool LiveNow = true;

	void DiscardExcessThenDequeue(TPair<nos::Buffer, uint32_t>& result, uint32_t requestedFrameNumber, bool wait)
	{
		u32 tryCount = 0;
		bool dequeued = false;
		bool oldLiveNow = LiveNow;
		FPlatformProcess::ConditionalSleep([&]()
			{
				while (Dequeue(result))
				{
					LiveNow = true;
					dequeued = true;
					if (result.Value >= requestedFrameNumber)
						return true;
				}

				return !LiveNow || !wait || tryCount++ > 20;
			}, 0.001f);
		
		LiveNow = dequeued;
		if (oldLiveNow != LiveNow)
			UE_LOG(LogCore, Warning, TEXT("LiveNow Changed"));

		if (LiveNow && result.Value != requestedFrameNumber)
			UE_LOG(LogCore, Warning, TEXT("Mismatch between popped frame number and requested frame number: %i, %i"), result.Value, requestedFrameNumber);
	}
};

class PinDataQueues : public nos::app::IEventDelegates
{
public:
	virtual ~PinDataQueues() {}

	PinDataQueue* GetAddQueue(nos::fb::UUID const& pinId)
	{
		uuids::uuid id(pinId.bytes()->begin(), pinId.bytes()->end());

		std::scoped_lock<std::mutex> lock(Guard);
		return &Queues[id];
	}
	virtual void OnPinValueChanged(nos::fb::UUID const& pinId, uint8_t const* data, size_t size, bool reset, uint64_t frameNumber) override
	{
		if (reset)
		{
			std::scoped_lock<std::mutex> lock(Guard);
			for (auto& [_, queue] : Queues)
				queue.Empty();

			return;
		}

		auto queue = GetAddQueue(pinId);
		queue->Enqueue({ nos::Buffer(data, size), frameNumber });
	}

	nos::Buffer Pop(nos::fb::UUID const& pinId, bool wait, uint32_t frameNumber)
	{
		auto queue = GetAddQueue(pinId);

		TPair<nos::Buffer, uint32_t> result;
		queue->DiscardExcessThenDequeue(result, frameNumber, wait);
		return result.Key;
	}

	std::mutex Guard;
	std::unordered_map<uuids::uuid, PinDataQueue> Queues;
};


class UNOSCustomTimeStep;
typedef std::function<void()> Task;

DECLARE_LOG_CATEGORY_EXTERN(LogNOSClient, Log, All);

//events coming from Nodos
DECLARE_EVENT_OneParam(FNOSClient, FNOSNodeConnected, nos::fb::Node const*);
DECLARE_EVENT_OneParam(FNOSClient, FNOSNodeUpdated, nos::fb::Node const&);
DECLARE_EVENT_OneParam(FNOSClient, FNOSContextMenuRequested, nos::ContextMenuRequest const&);
DECLARE_EVENT_OneParam(FNOSClient, FNOSContextMenuCommandFired, nos::ContextMenuAction const&);
DECLARE_EVENT(FNOSClient, FNOSNodeRemoved);
DECLARE_EVENT_FourParams(FNOSClient, FNOSPinValueChanged, nos::fb::UUID const&, uint8_t const*, size_t, bool);
DECLARE_EVENT_TwoParams(FNOSClient, FNOSPinShowAsChanged, nos::fb::UUID const&, nos::fb::ShowAs);
DECLARE_EVENT_TwoParams(FNOSClient, FNOSFunctionCalled, nos::fb::UUID const&, nos::fb::Node const&);
DECLARE_EVENT_OneParam(FNOSClient, FNOSNodeSelected, nos::fb::UUID const&);
DECLARE_EVENT_OneParam(FNOSClient, FNOSNodeImported, nos::fb::Node const&);
DECLARE_EVENT(FNOSClient, FNOSConnectionClosed);

// DECLARE_EVENT_OneParam(FNOSClient, FNOSConsoleCommandExecuted, FString);

/**
 * Implements communication with the Nodos Engine
 */
class FNOSClient;

class NOSCLIENT_API NOSEventDelegates : public PinDataQueues
{
public:
	~NOSEventDelegates() {}

	virtual void OnAppConnected(nos::fb::Node const* appNode) override;
	virtual void OnNodeUpdated(nos::fb::Node const& appNode) override;
	virtual void OnContextMenuRequested(nos::ContextMenuRequest const& request) override;
	virtual void OnContextMenuCommandFired(nos::ContextMenuAction const& action) override;
	virtual void OnNodeRemoved() override;
	virtual void OnPinValueChanged(nos::fb::UUID const& pinId, uint8_t const* data, size_t size, bool reset, uint64_t frameNumber) override;
	virtual void OnPinShowAsChanged(nos::fb::UUID const& pinId, nos::fb::ShowAs newShowAs) override;
	virtual void OnExecuteAppInfo(nos::app::AppExecuteInfo const* appExecuteInfo) override; 
	virtual void OnFunctionCall(nos::fb::UUID const& nodeId, nos::fb::Node const& function) override;
	virtual void OnNodeSelected(nos::fb::UUID const& nodeId) override;
	virtual void OnNodeImported(nos::fb::Node const& appNode) override;
	virtual void OnConnectionClosed() override;
	virtual void OnStateChanged(nos::app::ExecutionState newState) override;
	virtual void OnConsoleCommand(nos::app::ConsoleCommand const* consoleCommand) override;
	virtual void OnConsoleAutoCompleteSuggestionRequest(nos::app::ConsoleAutoCompleteSuggestionRequest const* consoleAutoCompleteSuggestionRequest) override;
	virtual void OnLoadNodesOnPaths(nos::LoadNodesOnPaths const* loadNodesOnPathsRequest) override;
	virtual void OnCloseApp() override;

	FNOSClient* PluginClient;
};


class UENodeStatusHandler
{
public:
	void SetClient(FNOSClient* PluginClient);
	void Add(std::string const& Id, nos::fb::TNodeStatusMessage const& Status);
	void Remove(std::string const& Id);
	void Update();
private:
	void SendStatus();
	FNOSClient* PluginClient = nullptr;
	std::unordered_map<std::string, nos::fb::TNodeStatusMessage> StatusMessages;
	bool Dirty = false;
};

class FPSCounter
{
public:
	bool Update(float dt);
	nos::fb::TNodeStatusMessage GetNodeStatusMessage() const;
private:
	float DeltaTimeAccum = 0;
	uint64_t FrameCount = 0;
	float FramesPerSecond = 0;
};

class NOSCLIENT_API FNodos
{
public:
	static bool Initialize();
	static void Shutdown();
	static nos::app::FN_MakeAppServiceClient* MakeAppServiceClient;
	static nos::app::FN_ShutdownClient* ShutdownClient;
private:
	// Nodos SDK DLL handle
	static void* LibHandle;
};



class NOSCLIENT_API FNOSClient : public IModuleInterface {

public:
	 
	//Empty constructor
	FNOSClient();

	//Called on startup of the module on Unreal Engine start
	virtual void StartupModule() override;

	//Called on shutdown of the module on Unreal Engine exit
	virtual void ShutdownModule() override;

	//This function is called when the connection with the Nodos Engine is started
	virtual void Connected();

	//This function is called when the connection with the Nodos Engine is finished
	virtual void Disconnected();
	 
	/// @return Connection status with Nodos Engine
	virtual bool IsConnected();

	//Tries to initialize connection with the Nodos engine
	void TryConnect();

	//Tick is called every frame once and handles the tasks queued from grpc threads
	bool Tick(float dt);

	void OnBeginFrame();

	//Called when the level is initiated
	void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues InitValues);
	 
	//Called when the level destruction began
	void OnPreWorldFinishDestroy(UWorld* World);

	//Called when the node is executed from Nodos
	void OnUpdatedNodeExecuted(nos::fb::vec2u deltaSeconds);

	bool ExecuteConsoleCommand(const TCHAR* Input);

	bool ExecInternal(const TCHAR* Input);
	
	//Grpc client to communicate
	TSharedPtr<NOSEventDelegates> EventDelegates = 0;

	//To send events to Nodos and communication
	nos::app::IAppServiceClient* AppServiceClient = nullptr;

	//Task queue
	TQueue<Task, EQueueMode::Mpsc> TaskQueue;

	//Custom time step implementation for Nodos controlling the unreal editor in play mode
	UPROPERTY()
	TWeakObjectPtr<UNOSCustomTimeStep> NOSTimeStep = nullptr;
	bool CustomTimeStepBound = false;

	// Nodos root node id
	static FGuid NodeId;
	// The app key we are using for Nodos
	static FString AppKey;

	TMap<FGuid, FName> PathUpdates;

	FNOSNodeConnected OnNOSConnected;
	FNOSNodeUpdated OnNOSNodeUpdated;
	FNOSContextMenuRequested OnNOSContextMenuRequested;
	FNOSContextMenuCommandFired OnNOSContextMenuCommandFired;
	FNOSNodeRemoved OnNOSNodeRemoved;
	FNOSPinValueChanged OnNOSPinValueChanged;
	FNOSPinShowAsChanged OnNOSPinShowAsChanged;
	FNOSFunctionCalled OnNOSFunctionCalled;
	FNOSNodeSelected OnNOSNodeSelected;
	FNOSNodeImported OnNOSNodeImported;
	FNOSConnectionClosed OnNOSConnectionClosed;
	TMulticastDelegate<void(nos::app::ExecutionState), FDefaultTSDelegateUserPolicy> OnNOSStateChanged_GRPCThread;
	TMulticastDelegate<void(const TArray<FString>&), FDefaultTSDelegateUserPolicy> OnNOSLoadNodesOnPaths;
	// FNOSConsoleCommandExecuted OnNOSConsoleCommandExecuted;
	
	UENodeStatusHandler UENodeStatusHandler;

	bool bDisplayingShaderCompilationWarning = false;

	int ReloadingLevel = 0;
	
protected:
	void Reset();

	FPSCounter FPSCounter;
	bool IsWorldInitialized = false;

};

class NOSConsoleOutput : public FOutputDevice
{
public:
	FNOSClient* NOSClient;
	NOSConsoleOutput(FNOSClient* NOSClient)
		: FOutputDevice(), NOSClient(NOSClient)
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if (!NOSClient || FString(V).IsEmpty())
		{
			return;
		}
		
		flatbuffers::FlatBufferBuilder mb;
		auto offset = nos::CreateAppEventOffset(mb ,nos::app::CreateConsoleOutputDirect(mb, TCHAR_TO_UTF8(V)));
		mb.Finish(offset);
		auto buf = mb.Release();
		auto root = flatbuffers::GetRoot<nos::app::AppEvent>(buf.data());
		NOSClient->AppServiceClient->Send(*root);
	}
};


