#pragma once

#include "Engine/EngineTypes.h"
#include "Engine/EngineCustomTimeStep.h"
#include "MZCustomTimeStep.h"

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include <queue>
#include <map>
#include <numeric>
#include "Containers/Queue.h"
#include "Logging/LogMacros.h"
#include "Subsystems/PlacementSubsystem.h"
//#include "mediaz.h"
//#include "Engine/TextureRenderTarget2D.h"
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)


//void MemoryBarrier();
//#include "Windows/AllowWindowsPlatformTypes.h"
//#pragma intrinsic(_InterlockedCompareExchange64)
//#define InterlockedCompareExchange64 _InterlockedCompareExchange64
//#include <d3d12.h>
//#include "Windows/HideWindowsPlatformTypes.h"

//#include "D3D12RHIPrivate.h"
//#include "D3D12RHI.h"
//#include "D3D12Resources.h"

#include "MZSceneTree.h"

#include "MediaZ/AppInterface.h"
#include "MediaZ/MediaZ.h"
#include "AppEvents_generated.h"

#include <mzFlatBuffersCommon.h>

#include <functional> 

typedef std::function<void()> Task;

DECLARE_LOG_CATEGORY_EXTERN(LogMediaZ, Log, All);
/**
 * Implements communication with the MediaZ Engine
 */

class FMZClient;

class MZCLIENT_API MZEventDelegates : public mz::app::IEventDelegates
{
public:
	virtual void OnAppConnected(mz::fb::Node const& appNode) override;
	virtual void OnNodeUpdated(mz::fb::Node const& appNode) override;
	virtual void OnContextMenuRequested(mz::ContextMenuRequest const& request) override;
	virtual void OnContextMenuCommandFired(mz::ContextMenuAction const& action) override;
	virtual void OnNodeRemoved() override;
	virtual void OnPinValueChanged(mz::fb::UUID const& pinId, uint8_t const* data, size_t size) override;
	virtual void OnPinShowAsChanged(mz::fb::UUID const& pinId, mz::fb::ShowAs newShowAs) override;
	virtual void OnExecuteApp(mz::fb::Node const& appNode) override; // Why do we need the whole node?
	virtual void OnFunctionCall(mz::fb::UUID const& nodeId, mz::fb::Node const& function) override;
	virtual void OnNodeSelected(mz::fb::UUID const& nodeId) override;
	virtual void OnNodeImported(mz::fb::Node const& appNode) override;
	virtual void OnConnectionClosed() override;


	FMZClient* PluginClient;
	// (Samil:) Will every app client have one node attached to it?
	// If so we can move this node ID to mediaZ SDK.
	//std::atomic_bool IsChannelReady = false;
};

class ContextMenuActions
{
public:
	TArray< TPair<FString, std::function<void(AActor*)>> >  ActorMenu;
	TArray< TPair<FString, Task> >  FunctionMenu;
	TArray< TPair<FString, Task> >  PropertyMenu;

	std::vector<flatbuffers::Offset<mz::ContextMenuItem>> SerializeActorMenuItems(flatbuffers::FlatBufferBuilder& fbb)
	{
		std::vector<flatbuffers::Offset<mz::ContextMenuItem>> result;
		int command = 0;
		for (auto item : ActorMenu)
		{
			result.push_back(mz::CreateContextMenuItemDirect(fbb, TCHAR_TO_UTF8(*item.Key), command++, 0));
		}
		return result;
	}

	ContextMenuActions()
	{
		TPair<FString, std::function<void(AActor*)> > deleteAction(FString("Delete"), [](AActor* actor)
			{
				//actor->Destroy();
				actor->GetWorld()->EditorDestroyActor(actor, false);
			});
		ActorMenu.Add(deleteAction);
	}

	void ExecuteActorAction(uint32 command, AActor* actor)
	{
		if (ActorMenu.IsValidIndex(command))
		{
			ActorMenu[command].Value(actor);
		}
	}
};

class UENodeStatusHandler
{
public:
	void SetClient(FMZClient* PluginClient);
	void Add(std::string const& Id, mz::fb::TNodeStatusMessage const& Status);
	void Remove(std::string const& Id);
	void Update();
private:
	void SendStatus();
	FMZClient* PluginClient = nullptr;
	std::unordered_map<std::string, mz::fb::TNodeStatusMessage> StatusMessages;
	bool Dirty = false;
};

class FPSCounter
{
public:
	bool Update(float dt);
	mz::fb::TNodeStatusMessage GetNodeStatusMessage() const;
private:
	float DeltaTimeAccum = 0;
	uint64_t FrameCount = 0;
	float FramesPerSecond = 0;
};

using PFN_MakeAppServiceClient = decltype(&mz::app::MakeAppServiceClient);
using PFN_mzGetD3D12Resources = decltype(&mzGetD3D12Resources);

class FMediaZ
{
public:
	static bool Initialize();
	static void Shutdown();
	static PFN_MakeAppServiceClient MakeAppServiceClient;
	static PFN_mzGetD3D12Resources GetD3D12Resources;
private:
	// MediaZ SDK DLL handle
	static void* LibHandle;
};

class MZCLIENT_API FMZClient : public IModuleInterface {

public:
	 
	//Empty constructor
	FMZClient();

	//Called on startup of the module on Unreal Engine start
	virtual void StartupModule() override;

	//Called on shutdown of the module on Unreal Engine exit
	virtual void ShutdownModule() override;

	//This function is called when the connection with the MediaZ Engine is started
	virtual void Connected();

	//This function is called when the connection with the MediaZ Engine is finished
	virtual void Disconnected();
	 
	/// @return Connection status with MediaZ Engine
	virtual bool IsConnected();

	//Tries to initialize connection with the MediaZ engine
	void TryConnect();

	//Sends node updates to the MediaZ
	void SendNodeUpdate(FGuid NodeId);

	//update asset lists when a new asset is created
	void OnAssetCreated(const FAssetData& createdAsset);

	//update asset lists when an asset deleted
	void OnAssetDeleted(const FAssetData& removedAsset);

	//Sends the spawnable actor list to MediaZ
	void SendAssetList();

	//Sends the user widget list to MediaZ
	void SendUMGList();

	//Sends pin value changed event to MediaZ (now only used for function return values)
	void SendPinValueChanged(FGuid propertyId, std::vector<uint8> data);

	//Sends pin updates to the root node 
	void SendPinUpdate();

	//Adds the node to scene tree and sends it to mediaZ
	void SendActorAdded(AActor* actor, FString spawnTag = FString());
	 
	//Deletes the node from scene tree and sends it to mediaZ
	void SendActorDeleted(FGuid Id, TSet<UObject*>& RemovedObjects);

	//Called when pie is started
	void HandleBeginPIE(bool bIsSimulating);

	//Called when pie is ending
	void HandleEndPIE(bool bIsSimulating);

	//Fills the root graph with first level information (Only the names of the actors without parents) 
	void PopulateSceneTree(bool reset = true);

	//Fills the specified node information to the root graph
	bool PopulateNode(FGuid NodeId);

	//Tick is called every frame once and handles the tasks queued from grpc threads
	bool Tick(float dt);

	//Test action to test wheter debug menu works
	void TestAction();

	//Set a properties value
	void SetPropertyValue(FGuid pinId, void* newval, size_t size);
	 
	//Populate root graph using SceneTree 
	//void PopulateRootGraphWithSceneTree();

	//Called when the level is initiated
	void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues InitValues);
	 
	//Called when the level destruction began
	void OnPreWorldFinishDestroy(UWorld* World);

	//Called when an actor is spawned into the world
	void OnActorSpawned(AActor* InActor);

	//Called when an actor is destroyed from the world
	void OnActorDestroyed(AActor* InActor);

	//Remove properties of tree node from registered properties and pins
	void RemoveProperties(TSharedPtr<TreeNode> Node,
		TSet<TSharedPtr<MZProperty>>& PinsToRemove, 
		TSet<TSharedPtr<MZProperty>> &PropertiesToRemove);

	void CheckPins(TSet<UObject*>& RemovedObjects,
		TSet<TSharedPtr<MZProperty>>& PinsToRemove,
		TSet<TSharedPtr<MZProperty>>& PropertiesToRemove);

	//Called when the actor is selected on the mediaZ hierarchy pane
	void OnNodeSelected(FGuid NodeId);

	//Called when a pin show as change action is fired from mediaZ 
	//We make that property a pin in the root node with the same GUID
	void OnPinShowAsChanged(FGuid NodeId, mz::fb::ShowAs newShowAs);

	//Called when a function is called from mediaZ
	void OnFunctionCall(FGuid funcId, TMap<FGuid, std::vector<uint8>> properties);

	//Called when the node is executed from mediaZ
	void OnUpdatedNodeExecuted(TMap<FGuid, std::vector<uint8>> updates);

	//Called when a context menu is fired
	void OnContexMenuFired(FGuid itemId, FVector2D pos, uint32 instigator);

	//Called when a action from a context menu is fired
	void OnContexMenuActionFired(FGuid itemId, uint32 actionId);

	//Called when a node is imported through (save/load graph) mediaZ
	void OnNodeImported(const mz::fb::Node* node);

	//Sends pin to add to a node
	void SendPinAdded(FGuid NodeId, TSharedPtr<MZProperty> const& mzprop);

	//delegate called when a property is changed from unreal engine editor
	//it updates thecorresponding property in mediaz
	void OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);
	 
	//Grpc client to communicate
	TSharedPtr<MZEventDelegates> EventDelegates = 0;

	//To send events to mediaz and communication
	TSharedPtr<mz::app::IAppServiceClient> AppServiceClient = nullptr;

	//Task queue
	TQueue<Task, EQueueMode::Mpsc> TaskQueue;

	//Scene tree holds the information to mimic the outliner in mediaz
	MZSceneTree SceneTree;

	//all the properties registered 
	TMap<FGuid, TSharedPtr<MZProperty>> RegisteredProperties;

	//all the properties registered mapped with property pointers
	TMap<FProperty*, TSharedPtr<MZProperty>> PropertiesMap;
	 
	//all the functions registered
	TMap<FGuid, TSharedPtr<MZFunction>> RegisteredFunctions;
	 
	//in/out pins of the mediaz node
	TMap<FGuid, TSharedPtr<MZProperty>> Pins;

	//custom functions like spawn actor
	TMap<FGuid, MZCustomFunction*> CustomFunctions;

	//Spawnable class list to spawn them from mediaZ
	TMap<FString, UObject*> SpawnableClasses;
	TMap<FString, FAssetPlacementInfo> ActorPlacementParamMap;

	//UMG asset list map
	TMap<FString, FTopLevelAssetPath> UMGs;

	ContextMenuActions menuActions;

	FDelegateHandle OnPropertyChangedHandle;

	//Custom time step implementation for mediaZ controlling the unreal editor in play mode
	class UMZCustomTimeStep* MZTimeStep = nullptr;
	bool CustomTimeStepBound = false;

	//MediaZ root node id
	static FGuid NodeId;

protected:
	void Reset();

	FPSCounter FPSCounter;
	UENodeStatusHandler UENodeStatusHandler;
	bool IsWorldInitialized = false;
};



