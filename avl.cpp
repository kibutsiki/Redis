#include "avl.h"

Node** tree_search(Node** from, NodeCompare cmp, void* key) {
    if (from == nullptr || cmp == nullptr) {
        return nullptr;
    }

    while (*from != nullptr) {
        const int32_t comparison = cmp(*from, key);
        if (comparison < 0) {
            from = &(*from)->right;
        } else if (comparison > 0) {
            from = &(*from)->left;
        } else {
            break;
        }
    }

    return from;
}

bool tree_insert(Node** from, Node* node, NodeCompare cmp, void* key) {
    if (from == nullptr || node == nullptr || cmp == nullptr ||
        node->left != nullptr || node->right != nullptr) {
        return false;
    }

    Node** position = tree_search(from, cmp, key);
    if (position == nullptr || *position != nullptr) {
        return false;
    }

    *position = node;
    return true;
}
