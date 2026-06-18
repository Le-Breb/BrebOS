#pragma once

template <typename T>
class BST
{
public:
    typedef int (*compare_func_t)(const T& a, const T& b);
    typedef bool (*matcher_t)(const T& a, const T& b);
private:
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
    T* find(const T& elem);
    void ensure_validity() const;
private:
    T extract_replacement(Node* node);
    void destroy_node(Node* node);
    void ensure_validity_aux(const Node* node) const;
};

#include "BST.hxx"