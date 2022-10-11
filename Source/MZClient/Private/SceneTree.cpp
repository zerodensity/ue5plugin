#include "SceneTree.h"


SceneTree::SceneTree()
{
	Root = new FolderNode;
	Root->Name = FString("UE5");
	Root->Parent = nullptr;
}

FolderNode* SceneTree::FindOrAddChildFolder(TreeNode* node, FString name)
{
	for (auto child : node->Children)
	{
		if (child->Name == name && child->GetType() == FString("Folder"))
		{
			return (FolderNode*)child;
		}
	}
	FolderNode* newChild = new FolderNode;
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

void SceneTree::ClearRecursive(TreeNode* node)
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
	delete node;
}

ActorNode* SceneTree::AddActor(FString folderPath, AActor* actor)
{
	if (!actor)
	{
		return nullptr;
	}

	folderPath.RemoveFromStart(FString("None"));
	folderPath.RemoveFromStart(FString("/"));
	TArray<FString> folders;
	folderPath.ParseIntoArray(folders, TEXT("/"));
	
	TreeNode* ptr = Root;
	for (auto item : folders)
	{
		ptr = FindOrAddChildFolder(ptr, item);
	}

	ActorNode* newChild = new ActorNode;
	newChild->Parent = ptr;
	newChild->Name = actor->GetActorLabel();
	newChild->id = actor->GetActorGuid();
	newChild->actor = actor;
	ptr->Children.push_back(newChild);
	nodeMap.Add(newChild->id, newChild);

	ActorComponentNode* loadingChild = new ActorComponentNode;
	loadingChild->Name = "Loading...";
	loadingChild->id = FGuid::NewGuid();
	loadingChild->Parent = newChild;
	newChild->Children.push_back(loadingChild);

	return newChild;
}


flatbuffers::Offset<mz::fb::Node> TreeNode::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes = SerializeChildren(fbb);
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*GetType()), false, true, 0, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), "UE5", 0, 0);
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
