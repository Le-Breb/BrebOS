#include "list.h"

template <class E>
list<E>::list()
{
    head = nullptr;
    s = 0;
}

template <class E>
void list<E>::addFirst(E e)
{
    auto* newNode = new Node<E>(e);
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
void list<E>::add(E e)
{
    auto* newNode = new Node<E>(e);
    if (head == nullptr)
    {
        head = newNode;
        s++;
        return;
    }

    Node<E>* current = head;
    Node<E>* tmp;

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
bool list<E>::remove(E e)
{
    Node<E>* current = head;
    Node<E>* prev = nullptr;
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
bool list<E>::removeFirst()
{
    Node<E>* tmp = head;

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
    Node<E>* current = head;
    Node<E>* prev = nullptr;

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
    Node<E>* current = head;
    Node<E>* newNext = nullptr;
    Node<E>* tmp;

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

    Node<E> *tmpPtr = head;
    Node<E> *tmpNxt = nullptr;

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
    Node<E>* current = head;

    do
    {
        Node<E>* tmp = current;
        current = current->next;
        delete tmp;
    }
    while (current != nullptr);

    head = nullptr;

    s = 0;
}

template <class E>
E* list<E>::get(int index) const
{
    if (index < 0 || index >= s)
        return nullptr;

    Node<E>* current = head;

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
    add(e);
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
        addFirst(e);
        return true;
    }

    if (index == s)
    {
        addLast(e);
        return true;
    }

    Node<E>* current = head;
    auto* newNode = new Node<E>(e);

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
    Node<E>* current = head;

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

// Compile the types of lists we will need here to other files can use them
#include "../file_management/FS.h"
#include "../processes/ELF.h"
class Socket;
class HTTP;
template class list<FS*>;
template class list<ELF*>;
template class list<uint>;
template class list<Socket*>;
template class list<HTTP*>;
