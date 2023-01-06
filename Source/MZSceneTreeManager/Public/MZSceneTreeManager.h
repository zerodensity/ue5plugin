#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

#include "MediaZ/AppInterface.h"
#include "MediaZ/MediaZ.h"
#include "AppEvents_generated.h"

#include <mzFlatBuffersCommon.h>

#include "MZSceneTree.h"
#include "MZClient.h"



class FMZSceneTreeManager : public IModuleInterface {

public:
	//Empty constructor
	FMZSceneTreeManager();

	//Called on startup of the module on Unreal Engine start
	virtual void StartupModule() override;

	//Called on shutdown of the module on Unreal Engine exit
	virtual void ShutdownModule() override;

	void OnMZConnected(mz::fb::Node const& appNode);

	void OnMZNodeUpdated(mz::fb::Node const& appNode);

	//every function of this class runs in game thread
	void OnMZNodeSelected(mz::fb::UUID const& nodeId);

	//called when connection is ended with mediaz
	void OnMZConnectionClosed();

	//called when a pin value changed from mediaz
	void OnMZPinValueChanged(mz::fb::UUID const& pinId, uint8_t const* data, size_t size);

	//called when a pins show as changed
	void OnMZPinShowAsChanged(mz::fb::UUID const& pinId, mz::fb::ShowAs newShowAs);

	//called when a function is called from mediaz
	void OnMZFunctionCalled(mz::fb::UUID const& nodeId, mz::fb::Node const& function);

	//called when the app(unreal engine) is executed from mediaz
	void OnMZExecutedApp(mz::fb::Node const& appNode);

	//called when a context menu is requested on some node on mediaz
	void OnMZContextMenuRequested(mz::ContextMenuRequest const& request);

	//called when a action is selected from context menu
	void OnMZContextMenuCommandFired(mz::ContextMenuAction const& action);

	//END OF MediaZ DELEGATES
	 
	
	//Called when the level is initiated
	void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues InitValues);

	//Called when the level destruction began
	void OnPreWorldFinishDestroy(UWorld* World);

	//delegate called when a property is changed from unreal engine editor
	//it updates thecorresponding property in mediaz
	void OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);

	//Called when an actor is spawned into the world
	void OnActorSpawned(AActor* InActor);

	//Called when an actor is destroyed from the world
	void OnActorDestroyed(AActor* InActor);

	//called when unreal engine node is imported from mediaZ
	void OnMZNodeImported(mz::fb::Node const& appNode);

	//Set a properties value
	void SetPropertyValue(FGuid pinId, void* newval, size_t size);

	//Rescans the current viewports world scene to get the current state of the scene outliner
	void RescanScene(bool reset = true);

	//Populates node with child actors/components, functions and properties
	bool PopulateNode(FGuid id);

	//Sends node updates to the MediaZ
	void SendNodeUpdate(FGuid NodeId);

	//Sends pin value changed event to MediaZ
	void SendPinValueChanged(FGuid propertyId, std::vector<uint8> data);

	//Sends pin updates to the root node 
	void SendPinUpdate();

	//Sends pin to add to a node
	void SendPinAdded(FGuid NodeId, TSharedPtr<MZProperty> const& mzprop);

	//Adds the node to scene tree and sends it to mediaZ
	void SendActorAdded(AActor* actor, FString spawnTag = FString());

	//Deletes the node from scene tree and sends it to mediaZ
	void SendActorDeleted(FGuid Id, TSet<UObject*>& RemovedObjects);

	//Called when pie is started
	void HandleBeginPIE(bool bIsSimulating);

	//Called when pie is ending
	void HandleEndPIE(bool bIsSimulating);

	//Remove properties of tree node from registered properties and pins
	void RemoveProperties(TSharedPtr<TreeNode> Node,
		TSet<TSharedPtr<MZProperty>>& PinsToRemove,
		TSet<TSharedPtr<MZProperty>>& PropertiesToRemove);

	void CheckPins(TSet<UObject*>& RemovedObjects,
		TSet<TSharedPtr<MZProperty>>& PinsToRemove,
		TSet<TSharedPtr<MZProperty>>& PropertiesToRemove);

	void Reset();

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

	//handles context menus and their actions
	class ContextMenuActions menuActions;

	//Scene tree holds the information to mimic the outliner in mediaz
	class MZSceneTree SceneTree;
	
	//unreal engine actors spawned by a custom function call from mediaz
	TSet<FGuid> ActorsSpawnedByMediaZ;
	
	//Class communicates with MediaZ
	class FMZClient* MZClient;

	class FMZAssetManager* MZAssetManager;
};