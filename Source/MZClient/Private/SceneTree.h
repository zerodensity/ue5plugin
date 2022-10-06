#pragma once

#include "CoreMinimal.h"
#include <vector>

struct TreeNode {

	FString Name;
	TreeNode* Parent;
	std::vector<TreeNode*> Children;
};


class SceneTree {

public:
	SceneTree();

	TreeNode* Root;
	bool IsSorted = false;
	std::vector<TreeNode*> LinearNodes;

	void AddItem(FString fullPath);

	TreeNode* FindOrAddChild(TreeNode* node, FString name);
};


