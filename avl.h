#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

struct Node {
    Node* left{};
    Node* right{};
};

using NodeCompare = int32_t (*)(Node*, void*);

Node** tree_search(Node** from, NodeCompare cmp, void* key);
bool tree_insert(Node** from, Node* node, NodeCompare cmp, void* key);
Node* node_detach(Node* node);
Node* tree_delete(Node* node, NodeCompare cmp, void* key);

struct SortedSet {
    std::set<std::pair<double, std::string>> by_score;
    std::unordered_map<std::string, double> by_name;
};
