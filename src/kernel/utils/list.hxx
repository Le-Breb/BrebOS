#pragma once

#include "list.h"
#include <utility>
#include <kstddef.h>

template<class T>
list<T>::Node::Node() {
    next = nullptr;
}

template<class T>
list<T>::Node::Node(const T& val) : value(val), next(nullptr) {

}

template <class T>
list<T>::Node::Node(T&& val) : value(std::move(val)), next(nullptr)
{

}

template <class E>
void list<E>::add(Node* newNode)
{
    if (head == nullptr)
    {
        head = newNode;
        s++;
        return;
    }

    Node* current = head;
    Node* tmp;

    do
    {
        tmp = current;
        current = current->next;
    }
    while (current != nullptr);


    tmp->next = newNode;

    s++;
}

template <class E>
list<E>::list()
{
    head = nullptr;
    s = 0;
}

template <class E>
void list<E>::copy_elements(const list& other)
{
    Node* tail = nullptr;
    for (Node* current = other.head; current != nullptr; current = current->next)
    {
        auto* newNode = new Node(current->value);
        if (tail == nullptr)
            head = newNode;
        else
            tail->next = newNode;
        tail = newNode;
        s++;
    }
}

template <class E>
list<E>::list(const list& other) : list()
{
    copy_elements(other);
}

template <class E>
list<E>::list(list&& other) noexcept : head(other.head), s(other.s)
{
    other.head = nullptr;
    other.s = 0;
}

template <class E>
list<E>& list<E>::operator=(const list& other)
{
    if (this == &other)
        return *this;

    clear();
    copy_elements(other);
    return *this;
}

template <class E>
list<E>::~list()
{
    clear();
}

template <class E>
void list<E>::addFirst(E e)
{
    auto* newNode = new Node(std::move(e));
    if (head == nullptr)
    {
        head = newNode;
    }
    else
    {
        newNode->next = head;
        head = newNode;
    }

    s++;
}

template <class E>
void list<E>::add(const E& e)
{
    auto* newNode = new Node(e);
    add(newNode);
}

template <class E>
void list<E>::add(E&& e)
{
    auto* newNode = new Node(std::move(e));
    add(newNode);
}

template <class E>
bool list<E>::remove(E e)
{
    Node* current = head;
    Node* prev = nullptr;
    bool found = false;

    if (current == nullptr)
    {
        return false;
    }

    do
    {
        if (current->value == e)
        {
            found = true;
            break;
        }

        prev = current;
        current = current->next;
    }
    while (current != nullptr);

    if (!found)
    {
        return false;
    }

    s--;

    // if the first element
    if (current == head)
    {
        prev = head;
        head = current->next;
        delete prev;
        return true;
    }

    // if the last element
    if (current->next == nullptr)
    {
        prev->next = nullptr;
        delete current;
        return true;
    }

    prev->next = current->next;
    delete current;

    return true;
}

template <class E>
template <typename Pred>
int list<E>::remove_if(Pred pred)
{
    int removed = 0;
    Node** link = &head; // Pointer to the pointer to the current node, so that unlinking needs no special case
    while (*link != nullptr)
    {
        Node* current = *link;
        if (pred(current->value))
        {
            *link = current->next;
            delete current;
            s--;
            removed++;
        }
        else
            link = &current->next;
    }

    return removed;
}

template <class E>
bool list<E>::removeFirst()
{
    Node* tmp = head;

    if (tmp == nullptr)
    {
        return false;
    }

    head = tmp->next;
    delete tmp;

    s--;

    return true;
}

template <class E>
bool list<E>::removeLast()
{
    Node* current = head;
    Node* prev = nullptr;

    if (current == nullptr)
    {
        return false;
    }

    do
    {
        prev = current;
        current = current->next;
    }
    while (current->next != nullptr);


    prev->next = nullptr;
    delete current;
    s--;
    return true;
}

template <class E>
void list<E>::reverse()
{
    Node* current = head;
    Node* newNext = nullptr;
    Node* tmp;

    if (current == nullptr)
        return;

    do
    {
        tmp = current->next;
        current->next = newNext;
        newNext = current;
        current = tmp;
    }
    while (current != nullptr);

    head = newNext;
}

/*template<class E>
void list<E>::sort(int order) {

    Node *tmpPtr = head;
    Node *tmpNxt = nullptr;

    if (tmpPtr == nullptr)
        return;

    tmpNxt = head->next;

    E tmp;

    while (tmpNxt != nullptr) {
        while (tmpNxt != tmpPtr) {
            if (order == SORT_ASC) {
                if (tmpNxt->value < tmpPtr->value) {
                    tmp = tmpPtr->value;
                    tmpPtr->value = tmpNxt->value;
                    tmpNxt->value = tmp;
                }
            } else if (order == SORT_DESC) {
                if (tmpNxt->value > tmpPtr->value) {
                    tmp = tmpPtr->value;
                    tmpPtr->value = tmpNxt->value;
                    tmpNxt->value = tmp;
                }
            } else {
                cerr << "Err: invalid sort order '" << order << "'" << endl;
                return;
            }
            tmpPtr = tmpPtr->next;
        }
        tmpPtr = head;
        tmpNxt = tmpNxt->next;
    }
}*/

template <class E>
void list<E>::clear()
{
    Node* current = head;

    while (current != nullptr)
    {
        Node* tmp = current;
        current = current->next;
        delete tmp;
    };

    head = nullptr;

    s = 0;
}

template <class E>
E* list<E>::get(int index) const
{
    if (index < 0 || index >= s)
        return nullptr;

    Node* current = head;

    while (index--)
    {
        current = current->next;
    }
    return &current->value;
}

template <class E>
int list<E>::size() const
{
    return s;
}

template <class E>
void list<E>::addLast(E e)
{
    add(std::move(e));
}

template <class E>
bool list<E>::add(int index, E e)
{
    if (index < 0 || index > s)
    {
        return false;
    }

    if (index == 0)
    {
        addFirst(std::move(e));
        return true;
    }

    if (index == s)
    {
        addLast(std::move(e));
        return true;
    }

    Node* current = head;
    auto* newNode = new Node(std::move(e));

    int i = 0;
    do
    {
        if (i++ == index)
        {
            break;
        }
        current = current->next;
    }
    while (current != nullptr);

    newNode->next = current->next;
    current->next = newNode;

    s++;

    return true;
}

template <class E>
bool list<E>::contains(E e)
{
    Node* current = head;

    while (current != nullptr)
    {
        if (current->value == e)
        {
            return true;
        }
        current = current->next;
    }
    return false;
}

template <class E>
typename list<E>::Iterator list<E>::begin() const
{
    return {head};
}

template <class E>
typename list<E>::Iterator list<E>::end() const
{
    return {nullptr};
}
