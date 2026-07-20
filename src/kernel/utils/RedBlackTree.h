#pragma once

// Taken from https://www.geeksforgeeks.org/dsa/deletion-in-red-black-tree/ and then modified by me

[[noreturn]]
extern __attribute__ ((format (printf, 1, 2))) int irrecoverable_error(const char* format, ...);

extern "C" void* malloc(uint);
extern "C" void free(void*);

enum COLOR { RED, BLACK };

template <typename T>
class RBTree
{
protected:
    class Node
    {
    public:
        T data;
        COLOR color;
        Node *left, *right;
        Node* parent;

        Node(const T& val);

        // returns pointer to uncle
        Node* uncle();

        // check if node is left child of parent
        bool isOnLeft();

        // returns pointer to sibling
        Node* sibling();

        // moves node down and moves given node in its place
        void moveDown(Node* nParent);

        bool hasRedChild();

        Node(const Node& other)
            : data(other.data),
              color(other.color),
              left(other.left),
              right(other.right),
              parent(other.parent)
                {}

        Node& operator=(const Node& other)
        {
            if (this == &other)
                return *this;

            data = other.data;
            color = other.color;
            left = other.left;
            right = other.right;
            parent = other.parent;

            return *this;
        }
    };

    Node* root;

    // left rotates the given node
    void leftRotate(Node* x);

    void rightRotate(Node* x);

    static void swapColors(Node* x1, Node* x2);

    static void swapValues(Node* u, Node* v);

    // fix red red at given node
    void fixRedRed(Node* x);

    // find node that do not have a left child
    // in the subtree of the given node
    Node* successor(Node* x);

    // find node that replaces a deleted node in BST
    Node* BSTreplace(Node* x);

    // deletes the given node
    void deleteNode(Node* v);

    void fixDoubleBlack(Node* x);

    Node* insert_aux(const T& data);

    // searches for given value
    // if found returns the node (used for delete)
    // else returns the last node while traversing (used in insert)
    Node* search_or_get_last(const T& data);

    static Node* default_node_allocator();
    static void default_node_deallocator(Node* node);

    Node* new_node(const T& data);
    void delete_node(Node* node);

    typedef Node* (*allocator_t)();
    typedef void (*deallocator_t)(Node*);
    typedef int (*compare_func_t)(const T& a, const T& b);

    allocator_t allocator;
    deallocator_t deallocator;
    compare_func_t compare_func;

    int check_invariants_aux(Node* node, Node* expected_parent) const;

    void check_invariants() const;
public:
    // constructor
    // initialize root
    explicit RBTree(compare_func_t compare_func,
        allocator_t allocator = [](){return static_cast<Node*>(::malloc(sizeof(Node)));},
        deallocator_t deallocator = [](Node* ptr){::free(ptr);});

    Node* getRoot();

    // searches for given value
    // returns null if node is not found
    Node* search(const T& data);

    // inserts the given value to tree
    void insert(const T& data);

    // utility function that deletes the node with given value
    void deleteByVal(const T& data);
};

#include "RedBlackTree.hxx"
