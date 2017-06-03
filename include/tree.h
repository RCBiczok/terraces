#pragma once

#include <memory>

class Tree {
public:
	Tree(std::shared_ptr<Tree> p_left, std::shared_ptr<Tree> p_right,
			std::shared_ptr<Tree> p_parent, std::string p_label) :
			label(p_label), left(p_left), right(p_right), parent(p_parent) {
	}

	Tree(std::shared_ptr<Tree> p_left, std::shared_ptr<Tree> p_right,
			std::string p_label) :
			label(p_label), left(p_left), right(p_right), parent(nullptr) {
	}

	Tree(std::shared_ptr<Tree> p_left, std::shared_ptr<Tree> p_right) :
			left(p_left), right(p_right), parent(nullptr) {
	}

	Tree(std::shared_ptr<Tree> p_left, std::shared_ptr<Tree> p_right,
			std::shared_ptr<Tree> p_parent) :
			left(p_left), right(p_right), parent(p_parent) {
	}

	Tree(std::string p_label, std::shared_ptr<Tree> p_parent) :
			label(p_label), left(nullptr), right(nullptr), parent(p_parent) {
	}

	Tree(std::string p_label) :
			label(p_label), left(nullptr), right(nullptr), parent(nullptr) {
	}

	Tree() :
			left(nullptr), right(nullptr), parent(nullptr) {
	}

	//TODO getter and setter? //performance vs. code quality?
	std::string label;
	//double length;  //TODO do we need the length?
	std::shared_ptr<Tree> left;
	std::shared_ptr<Tree> right;
	std::shared_ptr<Tree> parent;

	// unused from newick structure:
	//unsigned int leaves;
	//char * color;
	//int mark;
	//void * data;

	inline bool is_leaf() {
		return (left == nullptr && right == nullptr);
	}

	std::string to_newick_string();
};

class UnrootedTree {
public:
	UnrootedTree(std::string p_label, std::shared_ptr<Tree> p_elem1,
			std::shared_ptr<Tree> p_elem2, std::shared_ptr<Tree> p_elem3) :
			label(p_label), elem1(p_elem1), elem2(p_elem2), elem3(p_elem3) {
	}
	UnrootedTree(std::shared_ptr<Tree> rooted_tree) {
		if (rooted_tree == nullptr) {
			return;
		}
		//Only one leaf
		if (rooted_tree->is_leaf()) {
			this->elem1 = rooted_tree;
			return;
		}
		//Only binary tree with two leafs
		if (rooted_tree->left->is_leaf() && rooted_tree->right->is_leaf()) {
			this->elem1 = rooted_tree->left;
			this->elem2 = rooted_tree->right;
			return;
		}
		if (rooted_tree->left->is_leaf()) {
			this->elem1 = rooted_tree->left;
			this->elem2 = rooted_tree->right->left;
			this->elem3 = rooted_tree->right->right;
		} else {
			this->elem1 = rooted_tree->left->left;
			this->elem2 = rooted_tree->left->right;
			this->elem3 = rooted_tree->right;
		}
	}

	std::string label;
	std::shared_ptr<Tree> elem1;
	std::shared_ptr<Tree> elem2;
	std::shared_ptr<Tree> elem3;

	std::string to_newick_string();
};

std::shared_ptr<Tree> deep_copy(std::shared_ptr<Tree> tree);

