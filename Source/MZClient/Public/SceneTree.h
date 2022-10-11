#pragma once

#include "CoreMinimal.h"
#include <vector>
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)
#include <mzFlatBuffersCommon.h>
#include "mediaz.h"
#include "AppClient.h"

using MessageBuilder = flatbuffers::grpc::MessageBuilder;

struct TreeNode {

	FString Name;
	TreeNode* Parent;
	FGuid id;
	virtual FString GetType() = 0;
	std::vector<TreeNode*> Children;

	flatbuffers::Offset<mz::fb::Node> Serialize(flatbuffers::FlatBufferBuilder& fbb);
	std::vector<flatbuffers::Offset<mz::fb::Node>> SerializeChildren(flatbuffers::FlatBufferBuilder& fbb);
};

struct ActorNode : TreeNode
{
	AActor* actor = nullptr;
	virtual FString GetType() override { return FString("Actor"); }
};

struct ActorComponentNode : TreeNode
{
	UActorComponent* actorComponent = nullptr;
	virtual FString GetType() override { return FString("ActorComponent"); }
};

struct FolderNode : TreeNode
{
	virtual FString GetType() override { return FString("Folder"); }
};

class SceneTree {

public:
	SceneTree();

	TreeNode* Root;
	bool IsSorted = false;
	TMap<FGuid, TreeNode*> nodeMap;

	FolderNode* FindOrAddChildFolder(TreeNode* node, FString name);
	ActorNode* AddActor(FString folderPath, AActor* actor);
	//FolderNode* AddFolder(FString fullFolderPath);

	void Clear();

private:
	void ClearRecursive(TreeNode* node);
};


