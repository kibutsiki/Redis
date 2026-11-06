#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

struct Node {
    Node* parent{};
    Node* left{};
    Node* right{};
    uint32_t height{};
};

using AVLNode = Node;
using NodeCompare = int32_t (*)(Node*, void*);

void avl_init(AVLNode* node);
uint32_t avl_height(const AVLNode* node);
void avl_update(AVLNode* node);
AVLNode* avl_rot_left(AVLNode* node);

Node** tree_search(Node** from, NodeCompare cmp, void* key);
bool tree_insert(Node** from, Node* node, NodeCompare cmp, void* key);
Node* node_detach(Node* node);
Node* tree_delete(Node* node, NodeCompare cmp, void* key);

struct SortedSet {
    std::set<std::pair<double, std::string>> by_score;
    std::unordered_map<std::string, double> by_name;
};
