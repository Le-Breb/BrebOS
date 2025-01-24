#ifndef INCLUDE_LIST_H
#define INCLUDE_LIST_H

#include "kstddef.h"
#include "../core/memory.h"

template <class T>
struct list_item_
{
	T* data;
	struct list_item_* next;
	struct list_item_* prev;
};

template <class T>
using list_item = struct list_item_<T>;

template <class T>
class list
{
	size_t size;
	list_item<T>* head;
	list_item<T>* tail;

public:
	~list();

	list();

	T* push_front(T* element);

	T* push_back(T* element);

	size_t get_size() const;

	T* get_at(size_t index) const;

	T* insert_at(T* element, size_t index);

	int find(T* element) const;

	T* remove_at(size_t index);

	void clear();

	void reverse();

	list* split_at(size_t index);

	void concat(list* list2);
};

#endif /* !INCLUDE_LIST_H */
