#pragma once

#include "RedBlackTree.h"
#include <utility>

// Taken from https://www.geeksforgeeks.org/dsa/deletion-in-red-black-tree/ and then modified by me

[[noreturn]]
extern __attribute__ ((format (printf, 1, 2))) int irrecoverable_error(const char* format, ...);

template <typename T>
RBTree<T>::Node::Node(const T& val) : data(val)
{
    parent = left = right = nullptr;

    // Node is created during insertion
    // Node is red at insertion
    color = RED;
}

// returns pointer to uncle
template <typename T>
typename RBTree<T>::Node* RBTree<T>::Node::uncle()
{
    // If no parent or grandparent, then no uncle
    if (parent == nullptr or parent->parent == nullptr)
        return nullptr;

    if (parent->isOnLeft())
        // uncle on right
        return parent->parent->right;
    else
        // uncle on left
        return parent->parent->left;
}

// check if node is left child of parent
template <typename T>
bool RBTree<T>::Node::isOnLeft() { return this == parent->left; }

// returns pointer to sibling
template <typename T>
typename RBTree<T>::Node* RBTree<T>::Node::sibling()
{
    // sibling null if no parent
    if (parent == nullptr)
        return nullptr;

    if (isOnLeft())
        return parent->right;

    return parent->left;
}

// moves node down and moves given node in its place
template <typename T>
void RBTree<T>::Node::moveDown(Node* nParent)
{
    if (parent != nullptr)
    {
        if (isOnLeft())
        {
            parent->left = nParent;
        }
        else
        {
            parent->right = nParent;
        }
    }
    nParent->parent = parent;
    parent = nParent;
}

template <typename T>
bool RBTree<T>::Node::hasRedChild()
{
    return (left != nullptr and left->color == RED) or
        (right != nullptr and right->color == RED);
}

// left rotates the given node
template <typename T>
void RBTree<T>::leftRotate(Node* x)
{
    // new parent will be node's right child
    Node* nParent = x->right;

    // update root if current node is root
    if (x == root)
        root = nParent;

    x->moveDown(nParent);

    // connect x with new parent's left element
    x->right = nParent->left;
    // connect new parent's left element with node
    // if it is not null
    if (nParent->left != nullptr)
        nParent->left->parent = x;

    // connect new parent with x
    nParent->left = x;
}

template <typename T>
void RBTree<T>::rightRotate(Node* x)
{
    // new parent will be node's left child
    Node* nParent = x->left;

    // update root if current node is root
    if (x == root)
        root = nParent;

    x->moveDown(nParent);

    // connect x with new parent's right element
    x->left = nParent->right;
    // connect new parent's right element with node
    // if it is not null
    if (nParent->right != nullptr)
        nParent->right->parent = x;

    // connect new parent with x
    nParent->right = x;
}

template <typename T>
void RBTree<T>::swapColors(Node* x1, Node* x2)
{
    COLOR temp = x1->color;
    x1->color = x2->color;
    x2->color = temp;
}

template <typename T>
void RBTree<T>::swapValues(Node* u, Node* v)
{
    std::swap(u->data, v->data);
}

// fix red red at given node
template <typename T>
void RBTree<T>::fixRedRed(Node* x)
{
    // if x is root color it black and return
    if (x == root)
    {
        x->color = BLACK;
        return;
    }

    // initialize parent, grandparent, uncle
    Node* parent = x->parent, *grandparent = parent->parent,
        *uncle = x->uncle();

    if (parent->color != BLACK)
    {
        if (uncle != nullptr && uncle->color == RED)
        {
            // uncle red, perform recoloring and recurse
            parent->color = BLACK;
            uncle->color = BLACK;
            grandparent->color = RED;
            fixRedRed(grandparent);
        }
        else
        {
            // Else perform LR, LL, RL, RR
            if (parent->isOnLeft())
            {
                if (x->isOnLeft())
                {
                    // for left right
                    swapColors(parent, grandparent);
                }
                else
                {
                    leftRotate(parent);
                    swapColors(x, grandparent);
                }
                // for left left and left right
                rightRotate(grandparent);
            }
            else
            {
                if (x->isOnLeft())
                {
                    // for right left
                    rightRotate(parent);
                    swapColors(x, grandparent);
                }
                else
                {
                    swapColors(parent, grandparent);
                }

                // for right right and right left
                leftRotate(grandparent);
            }
        }
    }
}

// find node that do not have a left child
// in the subtree of the given node
template <typename T>
typename RBTree<T>::Node* RBTree<T>::successor(Node* x)
{
    Node* temp = x;

    while (temp->left != nullptr)
        temp = temp->left;

    return temp;
}

// find node that replaces a deleted node in BST
template <typename T>
typename RBTree<T>::Node* RBTree<T>::BSTreplace(Node* x)
{
    // when node have 2 children
    if (x->left != nullptr and x->right != nullptr)
        return successor(x->right);

    // when leaf
    if (x->left == nullptr and x->right == nullptr)
        return nullptr;

    // when single child
    if (x->left != nullptr)
        return x->left;
    else
        return x->right;
}

// deletes the given node
template <typename T>
void RBTree<T>::deleteNode(Node* v)
{
    Node* u = BSTreplace(v);

    // True when u and v are both black
    const bool uvBlack = ((u == nullptr or u->color == BLACK) and (v->color == BLACK));
    Node* parent = v->parent;

    if (u == nullptr)
    {
        // u is nullptr therefore v is a leaf
        if (v == root)
        {
            // v is root, making root null
            root = nullptr;
        }
        else
        {
            if (uvBlack)
            {
                // u and v both black
                // v is leaf, fix double black at v
                fixDoubleBlack(v);
            }
            else
            {
                // u or v is red
                if (v->sibling() != nullptr)
                    // sibling is not null, make it red
                    v->sibling()->color = RED;
            }

            // delete v from the tree
            if (v->isOnLeft())
            {
                parent->left = nullptr;
            }
            else
            {
                parent->right = nullptr;
            }
        }
        delete_node(v);
        return;
    }

    if (v->left == nullptr or v->right == nullptr)
    {
        // v has 1 child
        if (v == root)
        {
            // v is root, assign the value of u to v, and delete u
            v->data = u->data;
            v->left = v->right = nullptr;
            delete_node(u);
        }
        else
        {
            // Detach v from tree and move u up
            if (v->isOnLeft())
            {
                parent->left = u;
            }
            else
            {
                parent->right = u;
            }
            delete_node(v);
            u->parent = parent;
            if (uvBlack)
            {
                // u and v both black, fix double black at u
                fixDoubleBlack(u);
            }
            else
            {
                // u or v red, color u black
                u->color = BLACK;
            }
        }
        return;
    }

    // v has 2 children, swap values with successor and recurse
    swapValues(u, v);
    deleteNode(u);
}

template <typename T>
void RBTree<T>::fixDoubleBlack(Node* x)
{
    if (x == root)
        // Reached root
        return;

    Node* sibling = x->sibling(), *parent = x->parent;
    if (sibling == nullptr)
    {
        // No sibling, double black pushed up
        fixDoubleBlack(parent);
    }
    else
    {
        if (sibling->color == RED)
        {
            // Sibling red
            parent->color = RED;
            sibling->color = BLACK;
            if (sibling->isOnLeft())
            {
                // left case
                rightRotate(parent);
            }
            else
            {
                // right case
                leftRotate(parent);
            }
            fixDoubleBlack(x);
        }
        else
        {
            // Sibling black
            if (sibling->hasRedChild())
            {
                // at least 1 red children
                if (sibling->left != nullptr and sibling->left->color == RED)
                {
                    if (sibling->isOnLeft())
                    {
                        // left left
                        sibling->left->color = sibling->color;
                        sibling->color = parent->color;
                        rightRotate(parent);
                    }
                    else
                    {
                        // right left
                        sibling->left->color = parent->color;
                        rightRotate(sibling);
                        leftRotate(parent);
                    }
                }
                else
                {
                    if (sibling->isOnLeft())
                    {
                        // left right
                        sibling->right->color = parent->color;
                        leftRotate(sibling);
                        rightRotate(parent);
                    }
                    else
                    {
                        // right right
                        sibling->right->color = sibling->color;
                        sibling->color = parent->color;
                        leftRotate(parent);
                    }
                }
                parent->color = BLACK;
            }
            else
            {
                // 2 black children
                sibling->color = RED;
                if (parent->color == BLACK)
                    fixDoubleBlack(parent);
                else
                    parent->color = BLACK;
            }
        }
    }
}

template <typename T>
typename RBTree<T>::Node* RBTree<T>::insert_aux(const T& data)
{
    Node* newNode = new_node(data);
    if (root == nullptr)
    {
        // when root is null
        // simply insert value at root
        newNode->color = BLACK;
        root = newNode;
    }
    else
    {
        Node* temp = search_or_get_last(data);

        if (compare_func(temp->data, data) == 0)
            irrecoverable_error("%s: value already exists", __PRETTY_FUNCTION__);

        // if value is not found, search returns the node
        // where the value is to be inserted

        // connect new node to correct node
        newNode->parent = temp;

        if (const int cmp = compare_func(data, temp->data); cmp == -1)
            temp->left = newNode;
        else
            temp->right = newNode;

        // fix red red violation if exists
        fixRedRed(newNode);
    }

    return newNode;
}

// searches for given value
// if found returns the node (used for delete)
// else returns the last node while traversing (used in insert)
template <typename T>
typename RBTree<T>::Node* RBTree<T>::search_or_get_last(const T& data)
{
    Node* temp = root;
    while (temp != nullptr)
    {
        if (const int cmp = compare_func(data, temp->data); cmp == -1)
        {
            if (temp->left == nullptr)
                break;
            else
                temp = temp->left;
        }
        else if (cmp == 0)
        {
            break;
        }
        else
        {
            if (temp->right == nullptr)
                break;
            else
                temp = temp->right;
        }
    }

    return temp;
}

template <typename T>
typename RBTree<T>::Node* RBTree<T>::default_node_allocator()
{
    return static_cast<Node*>(malloc(sizeof(Node)));
}

template <typename T>
void RBTree<T>::default_node_deallocator(Node* node)
{
    free(node);
}

template <typename T>
typename RBTree<T>::Node* RBTree<T>::new_node(const T& data)
{
    return new (allocator()) Node(data);
}

template <typename T>
void RBTree<T>::delete_node(Node* node)
{
    node->~Node();
    deallocator(node);
}

// constructor
// initialize root
template <typename T>
RBTree<T>::RBTree(compare_func_t compare_func, allocator_t allocator, deallocator_t deallocator)  :
    root(nullptr), allocator(allocator), deallocator(deallocator), compare_func(compare_func) {  }

template <typename T>
typename RBTree<T>::Node* RBTree<T>::getRoot() { return root; }

// searches for given value
// returns null if not is not found
template <typename T>
typename RBTree<T>::Node* RBTree<T>::search(const T& data)
{
    Node* temp = root;
    while (temp != nullptr)
    {
        if (const int cmp = compare_func(data, temp->data); cmp == -1)
        {
            if (temp->left == nullptr)
                break;
            else
                temp = temp->left;
        }
        else if (cmp == 0)
        {
            return temp;
        }
        else
        {
            if (temp->right == nullptr)
                break;
            else
                temp = temp->right;
        }
    }

    return nullptr;
}

// inserts the given value to tree
template <typename T>
void RBTree<T>::insert(const T& data)
{
    insert_aux(data);
}

// utility function that deletes the node with given value
template <typename T>
void RBTree<T>::deleteByVal(const T& data)
{
    if (root == nullptr)
        // Tree is empty
        return;

    Node* v = search_or_get_last(data);

    if (compare_func(v->data, data))
    {
        irrecoverable_error("%s: node not found", __PRETTY_FUNCTION__);
        return;
    }

    deleteNode(v);
}

template <typename T>
int RBTree<T>::check_invariants_aux(Node* node, Node* expected_parent) const
{
    if (!node)
        return 0; // black-height contribution of a null leaf

    if (node->parent != expected_parent)
        irrecoverable_error("RBTree: parent link mismatch (node=%p, node->parent=%p, expected=%p)",
                             node, node->parent, expected_parent);

    if (node->left && node->left->parent != node)
        irrecoverable_error("RBTree: node->left->parent broken (node=%p)", node);
    if (node->right && node->right->parent != node)
        irrecoverable_error("RBTree: node->right->parent broken (node=%p)", node);

    if (node->color == RED)
    {
        if ((node->left && node->left->color == RED) ||
            (node->right && node->right->color == RED))
            irrecoverable_error("RBTree: red-red violation at node=%p", node);
    }

    int bh_left = check_invariants_aux(node->left, node);
    int bh_right = check_invariants_aux(node->right, node);

    if (bh_left != bh_right)
        irrecoverable_error("RBTree: black-height mismatch at node=%p (%d vs %d)",
                             node, bh_left, bh_right);

    return bh_left + (node->color == BLACK ? 1 : 0);
}

template <typename T>
void RBTree<T>::check_invariants() const
{
    if (!root)
        return;
    if (root->parent != nullptr)
        irrecoverable_error("RBTree: root->parent is not null (root=%p, parent=%p)", root, root->parent);
    if (root->color != BLACK)
        irrecoverable_error("RBTree: root is red");
    check_invariants_aux(root, nullptr);
}