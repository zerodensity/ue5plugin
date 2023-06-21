// Copyright MediaZ AS. All Rights Reserved.

#include "MZSceneTree.h"

MZSceneTree::MZSceneTree()
{
	Root = TSharedPtr<FolderNode>(new FolderNode);
	Root->mzMetaData.Add("PinnedCategories", "Control");
	Root->Name = FMZClient::AppKey;
	Root->Parent = nullptr;
}

TSharedPtr<FolderNode> MZSceneTree::FindOrAddChildFolder(TSharedPtr<TreeNode> node, FString name, TSharedPtr<TreeNode>& mostRecentParent)
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
	if(name == NAME_Reality_FolderName.ToString())
	{
		newChild->mzMetaData.Add("NodeColor", HEXCOLOR_Reality_Node);	
	}
	newChild->Id = FGuid::NewGuid();
	node->Children.push_back(newChild);
	NodeMap.Add(newChild->Id, newChild);
	if (!mostRecentParent)
	{
		mostRecentParent = newChild;
	}
	return newChild;
}



void MZSceneTree::Clear()
{
	ClearRecursive(Root);
	NodeMap.Empty();
	NodeMap.Add(Root->Id, Root);
}

void MZSceneTree::ClearRecursive(TSharedPtr<TreeNode> node)
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


TSharedPtr<SceneComponentNode> MZSceneTree::GetNewLoadingChild(TSharedPtr<TreeNode> parentToAttach)
{
	TSharedPtr<SceneComponentNode> loadingChild(new SceneComponentNode);
	loadingChild->Name = "Loading...";
	loadingChild->Id = FGuid::NewGuid();
	loadingChild->Parent = parentToAttach;
	parentToAttach->Children.push_back(loadingChild);
	return loadingChild;
}

TSharedPtr<ActorNode> MZSceneTree::AddActor(FString folderPath, MZActorReference *ActorReference)
{
	TSharedPtr<TreeNode> mostRecentParent;
	return AddActor(folderPath, ActorReference, mostRecentParent);
}

TSharedPtr<ActorNode> MZSceneTree::AddActor(FString folderPath, MZActorReference *ActorReference, TSharedPtr<TreeNode>& mostRecentParent)
{
	if (!ActorReference)
	{
		return nullptr;
	}

	AActor *actor = ActorReference->Get();
	
	folderPath.RemoveFromStart(FString("None"));
	folderPath.RemoveFromStart(FString("/"));
	TArray<FString> folders;
	folderPath.ParseIntoArray(folders, TEXT("/"));
	
	TSharedPtr<TreeNode> ptr = Root;
	for (auto item : folders)
	{
		ptr = FindOrAddChildFolder(ptr, item, mostRecentParent);
	}

	TSharedPtr<ActorNode> newChild(new ActorNode);
	newChild->Parent = ptr;
	//todo fix display names newChild->Name = actor->GetActorLabel();
	newChild->Name = actor->GetFName().ToString();
	newChild->Id = actor->GetActorGuid();
	newChild->ActorReference = ActorReference;
	newChild->NeedsReload = true;
	ptr->Children.push_back(newChild);
	NodeMap.Add(newChild->Id, newChild);

	if (actor->GetRootComponent())
	{
		newChild->Children.push_back(GetNewLoadingChild(newChild));
	}
	if (!mostRecentParent)
	{
		mostRecentParent = newChild;
	}
	return newChild;
}

TSharedPtr<ActorNode> MZSceneTree::AddActor(TSharedPtr<TreeNode> parent, MZActorReference *ActorReference)
{
	if (!ActorReference)
	{
		return nullptr;
	}
	if (!parent)
	{
		parent = Root;
	}

	AActor* actor = ActorReference->Get();

	TSharedPtr<ActorNode> newChild(new ActorNode);
	newChild->Parent = parent;
	newChild->Name = actor->GetActorLabel();
	newChild->Id = actor->GetActorGuid();
	newChild->ActorReference = ActorReference;
	newChild->NeedsReload = true;
	parent->Children.push_back(newChild);
	NodeMap.Add(newChild->Id, newChild);

	if (actor->GetRootComponent())
	{
		newChild->Children.push_back(GetNewLoadingChild(newChild));
	}

	return newChild;
}

TSharedPtr<SceneComponentNode> MZSceneTree::AddSceneComponent(TSharedPtr<TreeNode> parent, MZComponentReference* sceneComponent)
{
	TSharedPtr<SceneComponentNode> newComponentNode(new SceneComponentNode);
	newComponentNode->mzMetaData.Add("PinnedCategories", "Transform");
	newComponentNode->ComponentReference = sceneComponent;
	newComponentNode->Id = FGuid::NewGuid();
	newComponentNode->Name = sceneComponent->Get()->GetFName().ToString();
	newComponentNode->Parent = parent;
	newComponentNode->NeedsReload = true;
	parent->Children.push_back(newComponentNode);
	NodeMap.Add(newComponentNode->Id, newComponentNode);

	newComponentNode->Children.push_back(GetNewLoadingChild(newComponentNode));

	return newComponentNode;
}

flatbuffers::Offset<mz::fb::Node> TreeNode::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata = SerializeMetaData(fbb);
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes = SerializeChildren(fbb);
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*GetClassDisplayName()), false, true, 0, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), TCHAR_TO_ANSI(*FMZClient::AppKey), 0, 0, 0, 0, &metadata);
}

flatbuffers::Offset<mz::fb::Node> ActorNode::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata = SerializeMetaData(fbb);
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes = SerializeChildren(fbb);
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins = SerializePins(fbb);
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*GetClassDisplayName()), false, true, &pins, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), TCHAR_TO_ANSI(*FMZClient::AppKey), 0, 0, 0, 0, &metadata);
}

std::vector<flatbuffers::Offset<mz::fb::Pin>> ActorNode::SerializePins(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins;
	if(ActorReference)
	{
		for (auto &[PropName, MzProperty] : ActorReference->PropertiesMap)
		{
			pins.push_back(MzProperty->Serialize(fbb));
		}
	}
	return pins;
}

ActorNode::~ActorNode()
{
}

flatbuffers::Offset<mz::fb::Node> SceneComponentNode::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata = SerializeMetaData(fbb);
	std::vector<flatbuffers::Offset<mz::fb::Node>> childNodes = SerializeChildren(fbb);
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins = SerializePins(fbb);
	return mz::fb::CreateNodeDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*GetClassDisplayName()), false, true, &pins, 0, mz::fb::NodeContents::Graph, mz::fb::CreateGraphDirect(fbb, &childNodes).Union(), TCHAR_TO_ANSI(*FMZClient::AppKey), 0, 0, 0, 0, &metadata);
}

std::vector<flatbuffers::Offset<mz::fb::Pin>> SceneComponentNode::SerializePins(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::Pin>> pins;
	if(ComponentReference)
	{
		for (auto &[PropName, MzProperty] : ComponentReference->PropertiesMap)
		{
			pins.push_back(MzProperty->Serialize(fbb));
		}
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

std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> TreeNode::SerializeMetaData(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata;
	for (auto [key, value] : mzMetaData)
	{
		metadata.push_back(mz::fb::CreateMetaDataEntryDirect(fbb, TCHAR_TO_UTF8(*key), TCHAR_TO_UTF8(*value)));
	}
	return metadata;
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


