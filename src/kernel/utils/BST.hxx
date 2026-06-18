#pragma once

#include "BST.h"
#include "../core/fb.h"

template <typename T>
BST<T>::BST(compare_func_t compare_func) : compare_func(compare_func)
{

}

template <typename T>
BST<T>::~BST()
{
    destroy_node(root);
}

template <typename T>
void BST<T>::add(const T& elem)
{
    Node** cur = &root;

    while (*cur)
        cur = compare_func((*cur)->data, elem) >= 0 ? &(*cur)->left : &(*cur)->right;

    *cur = new Node{elem, nullptr, nullptr};
}

template <typename T>
bool BST<T>::remove(const T& elem)
{
    if (!root)
        return false;

    int prev_cmp = compare_func(root->data, elem);
    if (prev_cmp == 0)
    {
        if (root->right)
            root->data = extract_replacement(root);
        else if (root->left)
        {
            Node* tmp = root->left;
            delete root;
            root = tmp;
        }
        else
        {
            delete root;
            root = nullptr;
        }
        return true;
    }

    Node* prev = root;
    Node* cur = prev_cmp > 0 ? root->left : root->right;

    while (cur)
    {
        const int cmp = compare_func(cur->data, elem);
        if (cmp == 0) // Match
        {
            if (cur->left)
            {
                if (cur->right) // Two children
                {
                    cur->data = extract_replacement(cur);
                    return true;
                }
                // Only left child
                (prev_cmp > 0 ? prev->left : prev->right) = cur->left;
                delete cur;
                return true;
            }
            if (cur->right) // Only right child
            {
                (prev_cmp > 0 ? prev->left : prev->right) = cur->right;
                delete cur;
                return true;
            }
            // Leaf
            (prev_cmp > 0 ? prev->left : prev->right) = nullptr;
            delete cur;
            return true;
        }

        prev = cur;
        prev_cmp = cmp;

        cur = cmp > 0 ? cur->left : cur->right;
    }

    return false;
}

template <typename T>
T* BST<T>::find(const T& elem)
{
    Node* cur = root;

    while (cur)
    {
        if (const int cmp = compare_func(cur->data, elem); cmp == 0)
            return &cur->data;
        else
            cur = cmp > 0 ? cur->left : cur->right;
    }

    return nullptr;
}

template <typename T>
void BST<T>::ensure_validity() const
{
    return ensure_validity_aux(root);
}

template <typename T>
T BST<T>::extract_replacement(Node* node)
{
    Node* prev = node;
    Node* cur = node->right;

    while (cur->left)
    {
        prev = cur;
        cur = cur->left;
    }

    T ret = cur->data;

    (cur == node->right ? prev->right : prev->left) = cur->right;
    delete cur;

    return ret;
}

template <typename T>
void BST<T>::destroy_node(Node* node)
{
    if (!node)
        return;
    // Grab right and delete right away so that the last instruction is a recursive call. This allows the compiler to
    // perform tail call optimization
    Node* right = node->right;
    delete node;
    destroy_node(node->left);
    destroy_node(right);
}

template <typename T>
void BST<T>::ensure_validity_aux(const Node* node) const
{
    if (!node)
        return;
    if (node->left)
    {
        if (compare_func(node->data, node->left->data) < 0)
            irrecoverable_error("BST invalid");
        ensure_validity_aux(node->left);
    }
    if (node->right && compare_func(node->data, node->right->data) > 0)
    {
        if (compare_func(node->data, node->right->data) > 0)
            irrecoverable_error("BST invalid");
        ensure_validity_aux(node->right);
    }
}
