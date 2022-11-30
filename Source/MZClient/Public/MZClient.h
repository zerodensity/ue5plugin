#pragma once

#include "Engine/EngineTypes.h"

#if WITH_EDITOR
#include "Engine/EngineCustomTimeStep.h"
#include "MZCustomTimeStep.h"

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include <queue>
#include <map>
#include <numeric>
#include "Containers/Queue.h"
#include "Logging/LogMacros.h"
#include "SceneTree.h"
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

#include "SceneTree.h"

#include "AppClient.h"
#include <mzFlatBuffersCommon.h>



#include <functional> 
typedef std::function<void()> Task;



DECLARE_LOG_CATEGORY_EXTERN(LogMediaZ, Log, All);
/**
 * Implements communication with the MediaZ Engine
 */

class FMZClient;

class MZCLIENT_API ClientImpl : public mz::app::AppClient
{
public:
	using mz::app::AppClient::AppClient;

	virtual void OnAppConnected(mz::app::AppConnectedEvent const& event) override;


	virtual void OnNodeUpdate(mz::NodeUpdated const& archive) override;


	void OnTextureCreated(mz::app::TextureCreated const& texture);


	virtual void Done(grpc::Status const& Status) override;


	virtual void OnNodeRemoved(mz::app::NodeRemovedEvent const& action) override;


	virtual void OnPinValueChanged(mz::PinValueChanged const& action) override;


	virtual void OnPinShowAsChanged(mz::PinShowAsChanged const& action) override;


	virtual void OnFunctionCall(mz::app::FunctionCall const& action) override;


	virtual void OnExecute(mz::app::AppExecute const& aE) override;


	virtual void OnNodeSelected(mz::NodeSelected const& action) override;


	virtual void OnMenuFired(mz::ContextMenuRequest const& request) override;


	virtual void OnCommandFired(mz::ContextMenuAction const& action) override;

	virtual void OnNodeImported(mz::app::NodeImported const& action) override;

	FMZClient* PluginClient;
	FGuid nodeId;
	std::atomic_bool shutdown = true;
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
	void SetClient(class ClientImpl* GrpcClient);
	void Add(std::string const& Id, mz::fb::TNodeStatusMessage const& Status);
	void Remove(std::string const& Id);
private:
	void SendStatus() const;
	class ClientImpl* Client = nullptr;
	std::unordered_map<std::string, mz::fb::TNodeStatusMessage> StatusMessages;
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
	 void InitConnection();

	 //Sends node updates to the MediaZ
	 void SendNodeUpdate(FGuid nodeId);

	 //Sends the spawnable actor list to MediaZ
	 void SendAssetList();

	 //Sends pin value changed event to MediaZ (now only used for function return values)
	 void SendPinValueChanged(FGuid propertyId, std::vector<uint8> data);

	 //Sends pin updates to the root node 
	 void SendPinUpdate();

	 //Adds the node to scene tree and sends it to mediaZ
	 void SendActorAdded(AActor* actor, FString spawnTag = FString());
	 
	 //Deletes the node from scene tree and sends it to mediaZ
	 void SendActorDeleted(FGuid id, std::set<UObject*> removedObjects);

	 //Called when pie is started
	 void HandleBeginPIE(bool bIsSimulating);

	 //Called when pie is ending
	 void HandleEndPIE(bool bIsSimulating);

	 //Fills the root graph with first level information (Only the names of the actors without parents) 
	 void PopulateSceneTree();

	 //Fills the specified node information to the root graph
	 bool PopulateNode(FGuid nodeId);

	 //Tick is called every frame once and handles the tasks queued from grpc threads
	 bool Tick(float dt);

	 //Test action to test wheter debug menu works
	 void TestAction();

	 //Set a properties value
	 void SetPropertyValue(FGuid pinId, void* newval, size_t size);
	 
	 //Populate root graph using sceneTree 
	 //void PopulateRootGraphWithSceneTree();

	 //Called when the level is initiated
	 void OnPostWorldInit(UWorld* world, const UWorld::InitializationValues initValues);

	 //Called when an actor is spawned into the world
	 void OnActorSpawned(AActor* InActor);

	 //Called when an actor is destroyed from the world
	 void OnActorDestroyed(AActor* InActor);

	 //Remove properties of tree node from registered properties and pins
	 void RemoveProperties(TreeNode* node, std::set<MZProperty*>& pinsToRemove, std::set<MZProperty*>& propertiesToRemove);

	 void CheckPins(std::set<UObject*>& removedObjects, std::set<MZProperty*>& pinsToRemove, std::set<MZProperty*>& propertiesToRemove);

	 //Called when the actor is selected on the mediaZ hierarchy pane
	 void OnNodeSelected(FGuid nodeId);

	 //Called when a pin show as change action is fired from mediaZ 
	 //We make that property a pin in the root node with the same GUID
	 void OnPinShowAsChanged(FGuid nodeId, mz::fb::ShowAs newShowAs);

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
	 void SendPinAdded(FGuid nodeId, MZProperty* mzprop);

	 // Sends current node status to mediaz engine
	 void SendNodeStatusUpdate();

	 //Grpc client to communicate
	 class ClientImpl* Client = 0;

	 //Task queue
	 TQueue<Task, EQueueMode::Mpsc> TaskQueue;

	 //Scene tree holds the information to mimic the outliner in mediaz
	 SceneTree sceneTree;

	 //all the properties registered 
	 TMap<FGuid, MZProperty*> RegisteredProperties;

	 //all the properties registered mapped with property pointers
	 TMap<FProperty*, MZProperty*> PropertiesMap;
	 
	 //all the functions registered
	 TMap<FGuid, MZFunction*> RegisteredFunctions;
	 
	 //in/out pins of the mediaz node
	 TMap<FGuid, MZProperty*> Pins;

	 //custom functions like spawn actor
	 TMap<FGuid, MZCustomFunction*> CustomFunctions;

	 //Spawnable class list to spawn them from mediaZ
	 UPROPERTY()
	 TMap<FString, UObject*> SpawnableClasses;
	 TMap<FString, FAssetPlacementInfo> ActorPlacementParamMap;

	 ContextMenuActions menuActions;

	 //Custom time step implementation for mediaZ controlling the unreal editor in play mode
	 class UMZCustomTimeStep* CustomTimeStepImpl = nullptr;
	 bool ctsBound = false;

protected:
	FPSCounter FPSCounter;
	UENodeStatusHandler UENodeStatusHandler;
};

#endif



