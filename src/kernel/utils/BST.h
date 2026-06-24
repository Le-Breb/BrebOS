#pragma once

template <typename T>
class BST
{
public:
    typedef int (*compare_func_t)(const T& a, const T& b);
protected:
    struct Node
    {
        T data;
        Node* left = nullptr;
        Node* right = nullptr;
    };

    Node* root = nullptr;
    compare_func_t compare_func;
public:
    explicit BST(compare_func_t compare_func);
    ~BST();
    void add(const T& elem);
    bool remove(const T& elem);
    T* find(const T& elem) const;
    void ensure_validity() const;
protected:
    Node* find_node(const T& elem, compare_func_t cmp_func, Node**& node_ptr) const;
    T extract_replacement(Node* node);
    void destroy_node(Node* node);
    void ensure_validity_aux(const Node* node) const;
    void add_node(Node* node);
};

#include "BST.hxx"