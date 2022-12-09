#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "MZActorProperties.h"
#include "MZActorFunctions.h"
#include <vector>
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)
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
	TSharedPtr<TreeNode> Parent;
	FGuid id;
	bool needsReload = true;
	std::vector<TSharedPtr<TreeNode>> Children;

	virtual flatbuffers::Offset<mz::fb::Node> Serialize(flatbuffers::FlatBufferBuilder& fbb);
	std::vector<flatbuffers::Offset<mz::fb::Node>> SerializeChildren(flatbuffers::FlatBufferBuilder& fbb);

	virtual ~TreeNode();
};

struct ActorNode : TreeNode
{
	AActor* actor = nullptr;
	std::vector<TSharedPtr<MZProperty>> Properties;
	std::vector<TSharedPtr<MZFunction>> Functions;
	TMap<FString, FString> mzMetaData;
	virtual FString GetClassDisplayName() override { return actor ? actor->GetClass()->GetFName().ToString() : "Actor"; };
	virtual ActorNode* GetAsActorNode() override { return this; };
	virtual flatbuffers::Offset<mz::fb::Node> Serialize(flatbuffers::FlatBufferBuilder& fbb) override;
	std::vector<flatbuffers::Offset<mz::fb::Pin>> SerializePins(flatbuffers::FlatBufferBuilder& fbb);
	
	virtual ~ActorNode();
};

struct SceneComponentNode : TreeNode
{
	USceneComponent* sceneComponent = nullptr;
	std::vector<TSharedPtr<MZProperty>> Properties;
	virtual FString GetClassDisplayName() override { return sceneComponent ? sceneComponent->GetClass()->GetFName().ToString() : FString("ActorComponent"); };
	virtual SceneComponentNode* GetAsSceneComponentNode() override { return this; };
	virtual flatbuffers::Offset<mz::fb::Node> Serialize(flatbuffers::FlatBufferBuilder& fbb) override;
	std::vector<flatbuffers::Offset<mz::fb::Pin>> SerializePins(flatbuffers::FlatBufferBuilder& fbb);

	virtual ~SceneComponentNode();
};

struct FolderNode : TreeNode
{
	virtual FString GetClassDisplayName() override { return FString("Folder"); };
	virtual FolderNode* GetAsFolderNode() override { return this; };

	virtual ~FolderNode();
};

class SceneTree {

public:
	SceneTree();

	TSharedPtr<TreeNode> Root;
	bool IsSorted = false;
	TMap<FGuid, TSharedPtr<TreeNode>> nodeMap;
	TMap<FGuid, TSet<AActor*>> childMap;

	TSharedPtr<FolderNode> FindOrAddChildFolder(TSharedPtr<TreeNode> node, FString name);
	TSharedPtr<ActorNode> AddActor(FString folderPath, AActor* actor);
	TSharedPtr<ActorNode> AddActor(TSharedPtr<TreeNode> parent, AActor* actor);
	TSharedPtr<SceneComponentNode> AddSceneComponent(TSharedPtr<ActorNode> parent, USceneComponent* sceneComponent);
	TSharedPtr<SceneComponentNode> AddSceneComponent(TSharedPtr<SceneComponentNode> parent, USceneComponent* sceneComponent);
	//FolderNode* AddFolder(FString fullFolderPath);

	void Clear();

private:
	void ClearRecursive(TSharedPtr<TreeNode> node);
};






#endif

