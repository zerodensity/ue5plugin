#include "SceneTree.h"


SceneTree::SceneTree()
{
	Root = new TreeNode;
	Root->Name = FString("World");
	Root->Parent = nullptr;
}

TreeNode* SceneTree::FindOrAddChild(TreeNode* node, FString name)
{
	for (auto child : node->Children)
	{
		if (child->Name == name)
		{
			return child;
		}
	}
	TreeNode* newChild = new TreeNode;
	newChild->Parent = node;
	newChild->Name = name;
	node->Children.push_back(newChild);
	
	return newChild;
}

void SceneTree::AddItem(FString fullPath)
{
	fullPath.RemoveFromStart(FString("None/"));
	TArray<FString> folders;
	fullPath.ParseIntoArray(folders, TEXT("/"));
	
	TreeNode* ptr = Root;
	for (auto item : folders)
	{
		ptr = FindOrAddChild(ptr, item);
	}
}
