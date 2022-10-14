#pragma once

#include "CoreMinimal.h"
#include <vector>
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)
#include <mzFlatBuffersCommon.h>
#include "mediaz.h"
#include "AppClient.h"

using MessageBuilder = flatbuffers::grpc::MessageBuilder;

struct ActorNode;
struct FolderNode;
struct SceneComponentNode;

struct TreeNode {

	virtual ActorNode* GetAsActorNode() { return nullptr; };
	virtual FolderNode* GetAsFolderNode() { return nullptr; };
	virtual SceneComponentNode* GetAsSceneComponentNode() { return nullptr; };
	virtual FString GetClassDisplayName() = 0;
	
	FString Name;
	TreeNode* Parent;
	FGuid id;
	bool needsReload = true;
	std::vector<TreeNode*> Children;

	flatbuffers::Offset<mz::fb::Node> Serialize(flatbuffers::FlatBufferBuilder& fbb);
	std::vector<flatbuffers::Offset<mz::fb::Node>> SerializeChildren(flatbuffers::FlatBufferBuilder& fbb);

};

struct ActorNode : TreeNode
{
	AActor* actor = nullptr;
	virtual FString GetClassDisplayName() override { return actor ? actor->GetClass()->GetFName().ToString() : "Actor"; };
	virtual ActorNode* GetAsActorNode() override { return this; };

};

struct SceneComponentNode : TreeNode
{
	USceneComponent* sceneComponent = nullptr;
	virtual FString GetClassDisplayName() override { return sceneComponent ? sceneComponent->GetClass()->GetFName().ToString() : FString("ActorComponent"); };
	virtual SceneComponentNode* GetAsSceneComponentNode() override { return this; };

};

struct FolderNode : TreeNode
{
	virtual FString GetClassDisplayName() override { return FString("Folder"); };
	virtual FolderNode* GetAsFolderNode() override { return this; };
};

class SceneTree {

public:
	SceneTree();

	TreeNode* Root;
	bool IsSorted = false;
	TMap<FGuid, TreeNode*> nodeMap;
	TMap<FGuid, TSet<AActor*>> childMap;

	FolderNode* FindOrAddChildFolder(TreeNode* node, FString name);
	ActorNode* AddActor(FString folderPath, AActor* actor);
	ActorNode* AddActor(TreeNode* parent, AActor* actor);
	SceneComponentNode* AddSceneComponent(ActorNode* parent, USceneComponent* sceneComponent);
	SceneComponentNode* AddSceneComponent(SceneComponentNode* parent, USceneComponent* sceneComponent);
	//FolderNode* AddFolder(FString fullFolderPath);

	void Clear();

private:
	void ClearRecursive(TreeNode* node);
};


