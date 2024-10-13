#ifndef INCLUDE_LIST_H
#define INCLUDE_LIST_H

#include "clib/stddef.h"
#include "memory.h"

struct list_item
{
	void* data;
	struct list_item* next;
	struct list_item* prev;
};

typedef struct
{
	size_t size;
	struct list_item* head;
	struct list_item* tail;
} list;

list* list_init(void);

void* list_push_front(list* list, void* element);

void* list_push_back(list* list, void* element);

size_t list_size(const list* list);

void* list_get(const list* list, size_t index);

void* list_insert_at(list* list, void* element, size_t index);

int list_find(const list* list, void* element);

void* list_remove_at(list* list, size_t index);

void list_clear(list* list);

void list_reverse(list* list);

list* list_split_at(list* l, size_t index);

void list_concat(list* list1, list* list2);

#endif /* !INCLUDE_LIST_H */
