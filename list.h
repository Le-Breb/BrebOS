#ifndef INCLUDE_LIST_H
#define INCLUDE_LIST_H

#include "libc/stddef.h"
#include "memory.h"

struct list_item_
{
	void* data;
	struct list_item_* next;
	struct list_item_* prev;
};

typedef struct list_item_ list_item;

class list
{
	size_t size;
	list_item* head;
	list_item* tail;

public:
	~list();

	list();

	void* push_front(void* element);

	void* push_back(void* element);

	size_t get_size() const;

	void* get_at(size_t index) const;

	void* insert_at(void* element, size_t index);

	int find(void* element) const;

	void* remove_at(size_t index);

	void clear();

	void reverse();

	list* split_at(size_t index);

	void concat(list* list2);
};

#endif /* !INCLUDE_LIST_H */
