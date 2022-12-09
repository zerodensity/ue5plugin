#if WITH_EDITOR
#include "SceneTree.h"


SceneTree::SceneTree()
{
	Root = TSharedPtr<FolderNode>(new FolderNode);
	Root->Name = FString("UE5");
	Root->Parent = nullptr;
}

TSharedPtr<FolderNode> SceneTree::FindOrAddChildFolder(TSharedPtr<TreeNode> node, FString name)
{
	for (auto child : node->Children)
	{
		if (child->Name == name && child->GetAsFolderNode())
		{
			return StaticCastSharedPtr<FolderNode>(child);
		}
	}
	TSharedPtr<FolderNode> newChild(new FolderNode);
	newChild->Parent = node;
	newChild->Name = name;
	newChild->id = FGuid::NewGuid();
	node->Children.push_back(newChild);
	nodeMap.Add(newChild->id, newChild);
	return newChild;
}



void SceneTree::Clear()
{
	ClearRecursive(Root);
	nodeMap.Empty();
	nodeMap.Add(Root->id, Root);
}

void SceneTree::ClearRecursive(TSharedPtr<TreeNode> node)
{
	for (auto child : node->Children)
	{
		ClearRecursive(child);
	}
	if (node == Root)
	{
		Root->Children.clear();
		return;
	}
}

TSharedPtr<ActorNode> SceneTree::AddActor(FString folderPath, AActor* actor)
{
#if WITH_EDITOR
	if (!actor)
	{
		return nullptr;
	}

	folderPath.RemoveFromStart(FString("None"));
	folderPath.RemoveFromStart(FString("/"));
	TArray<FString> folders;
	folderPath.ParseIntoArray(folders, TEXT("/"));
	
	TSharedPtr<TreeNode> ptr = Root;
	for (auto item : folders)
	{
		ptr = FindOrAddChildFolder(ptr, item);
	}

	TSharedPtr<ActorNode> newChild(new ActorNode);
	newChild->Parent = ptr;
	newChild->Name = actor->GetActorLabel();
	newChild->id = actor->GetActorGuid();
	newChild->actor = actor;
	newChild->needsReload = true;
	ptr->Children.push_back(newChild);
	nodeMap.Add(newChild->id, newChild);

	if (actor->GetRootComponent())
	{
		TSharedPtr<SceneComponentNode> loadingChild(new SceneComponentNode);
		loadingChild->Name = "Loading...";
		loadingChild->id = FGuid::NewGuid();
		loadingChild->Parent = newChild;
		newChild->Children.push_back(loadingChild);
	}

	return newChild;
#else
	return nullptr;
#endif // WITH_EDITOR
}

TSharedPtr<ActorNode> SceneTree::AddActor(TSharedPtr<TreeNode> parent, AActor* actor)
{
	if (!actor)
	{
		return nullptr;
	}
	if (!parent)
	{
		parent = Root;
	}

	TSharedPtr<ActorNode> newChild(new ActorNode);
	newChild->Parent = parent;
#if WITH_EDITOR
	newChild->Name = actor->GetActorLabel();
	newChild->id = actor->GetActorGuid();
#else
	newChild->Name = actor->GetFName().ToString();
	newChild->id = FGuid::NewGuid();
#endif
	newChild->actor = actor;
	newChild->needsReload = true;
	parent->Children.push_back(newChild);
	nodeMap.Add(newChild->id, newChild);

	if (actor->GetRootComponent())
	{
		TSharedPtr<SceneComponentNode> loadingChild(new SceneComponentNode);
		loadingChild->Name = "Loading...";
		loadingChild->id = FGuid::NewGuid();
		loadingChild->Parent = newChild;
		newChild->Children.push_back(loadingChild);
	}

	return newChild;
}

TSharedPtr<SceneComponentNode> SceneTree::AddSceneComponent(TSharedPtr<ActorNode> parent, USceneComponent* sceneComponent)
{
	TSharedPtr<SceneComponentNode>newComponentNode(new SceneComponentNode);
	newComponentNode->sceneComponent = sceneComponent;
	newComponentNode->id = FGuid::NewGuid();
	newComponentNode->Name = sceneComponent->GetFName().ToString();
	newComponentNode->Parent = parent;
	newComponentNode->needsReload = true;
	parent->Children.push_back(newComponentNode);
	nodeMap.Add(newComponentNode->id, newComponentNode);

	TSharedPtr<SceneComponentNode> loadingChild(new SceneComponentNode);
	loadingChild->Name = "Loading...";
	loadingChild->id = FGuid::NewGuid();
	loadingChild->Parent = newComponentNode;
	newComponentNode->Children.push_back(loadingChild);

	return newComponentNode;
}

TSharedPtr<SceneComponentNode> SceneTree::AddSceneComponent(TSharedPtr<SceneComponentNode> parent, USceneComponent* sceneComponent)
{
	TSharedPtr<SceneComponentNode> newComponentNode(new SceneComponentNode);
	newComponentNode->sceneComponent = sceneComponent;
	newComponentNode->id = FGuid::NewGuid();
	newComponentNode->Name = sceneComponent->GetFName().ToString();
	newComponentNode->Parent = parent;
	newComponentNode->needsReload = true;
	parent->Children.push_back(newComponentNode);
	nodeMap.Add(newComponentNode->id, newComponentNode);

	TSharedPtr<SceneComponentNode> loadingChild(new SceneComponentNode);
	loadingChild->Name = "Loading...";
	loadingChild->id = FGuid::NewGuid();
	loadingChild->Parent = newComponentNode;
	newComponentNode->Children.push_back(loadingChild);

	return newComponentNode;
}


flatbuffers::Offset<mz::fb::Node> TreeNode::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes = SerializeChildren(fbb);
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*GetClassDisplayName()), false, true, 0, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), "UE5", 0, 0);
}

flatbuffers::Offset<mz::fb::Node> ActorNode::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata;
	for (auto [key, value] : mzMetaData)
	{
		metadata.push_back(mz::fb::CreateMetaDataEntryDirect(fbb, TCHAR_TO_UTF8(*key), TCHAR_TO_UTF8(*value)));
	}

	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes = SerializeChildren(fbb);
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins = SerializePins(fbb);
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*GetClassDisplayName()), false, true, &pins, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), "UE5", 0, 0, 0, 0, &metadata);
}

std::vector<flatbuffers::Offset<mz::fb::Pin>> ActorNode::SerializePins(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins;
	for (auto mzprop : Properties)
	{
		pins.push_back(mzprop->Serialize(fbb));
	}
	return pins;
}

ActorNode::~ActorNode()
{
}

flatbuffers::Offset<mz::fb::Node> SceneComponentNode::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes = SerializeChildren(fbb);
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins = SerializePins(fbb);
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*GetClassDisplayName()), false, true, &pins, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), "UE5", 0, 0);
}

std::vector<flatbuffers::Offset<mz::fb::Pin>> SceneComponentNode::SerializePins(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins;
	for (auto mzprop : Properties)
	{
		pins.push_back(mzprop->Serialize(fbb));
	}
	return pins;
}

std::vector<flatbuffers::Offset<mz::fb::Node>> TreeNode::SerializeChildren(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes;

	if (Children.empty())
	{
		return childNodes;
	}

	for (auto child : Children)
	{
		childNodes.push_back(child->Serialize(fbb));
	}

	return childNodes;
}

TreeNode::~TreeNode()
{
}


SceneComponentNode::~SceneComponentNode()
{
}

FolderNode::~FolderNode()
{
}


#endif

