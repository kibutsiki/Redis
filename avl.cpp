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

Node* node_detach(Node* node) {
    if (node == nullptr || node->right == nullptr) {
        return node == nullptr ? nullptr : node->left;
    }

    Node** to_victim = &node->right;
    while ((*to_victim)->left != nullptr) {
        to_victim = &(*to_victim)->left;
    }

    Node* victim = *to_victim;
    *to_victim = victim->right;
    *victim = *node;
    return victim;
}

Node* tree_delete(Node* node, NodeCompare cmp, void* key) {
    if (node == nullptr || cmp == nullptr) {
        return node;
    }

    const int32_t comparison = cmp(node, key);
    if (comparison < 0) {
        node->right = tree_delete(node->right, cmp, key);
    } else if (comparison > 0) {
        node->left = tree_delete(node->left, cmp, key);
    } else {
        return node_detach(node);
    }

    return node;
}
