#include "../core/memory.h"
#include "list.h"
#include "../file_management/VFS.h"

template <class T>
list<T>::list() : size(0), head(NULL), tail(NULL)
{
}

template <class T>
T* list<T>::push_front(T* element)
{
	list_item<T>* i = (list_item<T>*)malloc(sizeof(list_item<T>));
	if (!i)
		return nullptr;

	i->data = element;
	i->next = head;
	if (i->next)
		i->next->prev = i;
	else
		tail = i;
	i->prev = NULL;

	head = i;
	size++;

	return element;
}

template <class T>
T* list<T>::push_back(T* element)
{
	list_item<T>* i = (list_item<T>*)malloc(sizeof(list_item<T>));
	if (!i)
		return 0;

	i->data = element;
	i->prev = tail;
	if (tail)
		tail->next = i;
	else
		head = i;
	i->next = NULL;

	tail = i;
	size++;

	return element;
}

template <class T>
size_t list<T>::get_size() const
{
	return size;
}

template <class T>
T* list<T>::get_at(size_t index) const
{
	if (size == 0 || index >= size)
		return NULL;

	list_item<T>* i = head;
	for (size_t c = 0; c < index; c++)
		i = i->next;

	return i->data;
}

template <class T>
T* list<T>::insert_at(T* element, size_t index)
{
	if (index > size)
		return 0;

	if (index == NULL)
		return push_front(element);

	list_item<T>* i = head;
	for (size_t c = 0; c < index - 1; c++)
		i = i->next;

	list_item<T>* n = (list_item<T>*)malloc(sizeof(list_item<T>));
	if (!n)
		return NULL;
	n->data = element;
	n->next = i->next;
	n->prev = i;
	if (n->next)
		n->next->prev = n;
	else
		tail = n;
	i->next = n;

	size++;

	return element;
}

template <class T>
int list<T>::find(T* element) const
{
	list_item<T>* i = head;
	for (int c = 0; i; c++, i = i->next)
		if (i->data == element)
			return c;

	return -1;
}

template <class T>
T* list<T>::remove_at(size_t index)
{
	if (index >= size)
		return NULL;

	if (index == 0)
	{
		T* e = head->data;
		list_item<T>* h = head;
		head = h->next;
		if (head)
			head->prev = NULL;
		else
			tail = NULL;
		delete h;
		size--;

		return e;
	}

	list_item<T>* i = head;
	for (size_t c = 0; c < index - 1; c++, i = i->next)
		continue;

	list_item<T>* c = i->next;
	T* e = c->data;
	i->next = c->next;
	if (i->next)
		i->next->prev = i;
	else
		tail = i;

	size--;

	delete c;

	return e;
}

template <class T>
void list<T>::clear()
{
	list_item<T>* i = head;

	while (i)
	{
		list_item<T>* n = i->next;
		delete i;
		i = n;
	}

	size = 0;
	head = tail = NULL;
}

template <class T>
void list<T>::reverse()
{
	list_item<T>* i = head;

	if (!i)
		return;

	while (i)
	{
		list_item<T>* n = i->next;
		i->next = i->prev;
		i->prev = n;

		i = n;
	}

	list_item<T>* tmp = head;
	head = tail;
	tail = tmp;
}

template <class T>
list<T>* list<T>::split_at(size_t index)
{
	if (index > size)
		return NULL;

	list* n = (list*)malloc(sizeof(list));
	if (!n)
		return NULL;

	for (size_t i = index; i < size; i++)
		n->push_back(get_at(i));
	size_t s = size;
	for (size_t i = index; i < s; i++)
		remove_at(index);

	return n;
}

template <class T>
void list<T>::concat(list* list2)
{
	for (size_t i = 0; i < list2->size; i++)
		push_back(list2->get_at(i));
	list2->clear();
}

template <class T>
list<T>::~list()
{
	clear();
}

// Explicit template instantiation (avoids having to put everything in the header file)
template class list<FS>;
template class list<void>;
