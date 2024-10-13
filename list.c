#include "memory.h"
#include "list.h"
#include "clib/stdio.h"

list* list_init(void)
{
	list* l = malloc(sizeof(list));
	if (!l)
		return NULL;

	l->size = 0;
	l->head = l->tail = NULL;

	return l;
}

void* list_push_front(list* list, void* element)
{
	struct list_item* i = malloc(sizeof(struct list_item));
	if (!i)
		return 0;

	i->data = element;
	i->next = list->head;
	if (i->next)
		i->next->prev = i;
	else
		list->tail = i;
	i->prev = NULL;

	list->head = i;
	list->size++;

	return element;
}


void* list_push_back(list* list, void* element)
{
	struct list_item* i = malloc(sizeof(struct list_item));
	if (!i)
		return 0;

	i->data = element;
	i->prev = list->tail;
	if (list->tail)
		list->tail->next = i;
	else
		list->head = i;
	i->next = NULL;

	list->tail = i;
	list->size++;

	return element;
}

size_t list_size(const list* list)
{
	return list->size;
}

void* list_get(const list* list, size_t index)
{
	if (!list || list->size == 0 || index >= list->size)
		return NULL;

	struct list_item* i = list->head;
	for (size_t c = 0; c < index; c++)
		i = i->next;

	return i->data;
}

void* list_insert_at(list* list, void* element, size_t index)
{
	if (index > list->size)
		return 0;

	if (index == NULL)
		return list_push_front(list, element);

	struct list_item* i = list->head;
	for (size_t c = 0; c < index - 1; c++)
		i = i->next;

	struct list_item* n = malloc(sizeof(struct list_item));
	if (!n)
		return NULL;
	n->data = element;
	n->next = i->next;
	n->prev = i;
	if (n->next)
		n->next->prev = n;
	else
		list->tail = n;
	i->next = n;

	list->size++;

	return element;
}

int list_find(const list* list, void* element)
{
	struct list_item* i = list->head;
	for (int c = 0; i; c++, i = i->next)
		if (i->data == element)
			return c;

	return -1;
}

void* list_remove_at(list* list, size_t index)
{
	if (index >= list->size)
		return NULL;

	if (index == 0)
	{
		void* e = list->head->data;
		struct list_item* h = list->head;
		list->head = h->next;
		if (list->head)
			list->head->prev = NULL;
		else
			list->tail = NULL;
		free(h);
		list->size--;

		return e;
	}

	struct list_item* i = list->head;
	for (size_t c = 0; c < index - 1; c++, i = i->next)
		continue;

	struct list_item* c = i->next;
	void* e = c->data;
	i->next = c->next;
	if (i->next)
		i->next->prev = i;
	else
		list->tail = i;

	list->size--;

	free(c);

	return e;
}

void list_clear(list* list)
{
	struct list_item* i = list->head;

	while (i)
	{
		struct list_item* n = i->next;
		free(i);
		i = n;
	}

	list->size = 0;
	list->head = list->tail = NULL;
}

void list_reverse(list* list)
{
	struct list_item* i = list->head;

	if (!i)
		return;

	while (i)
	{
		struct list_item* n = i->next;
		i->next = i->prev;
		i->prev = n;

		i = n;
	}

	struct list_item* tmp = list->head;
	list->head = list->tail;
	list->tail = tmp;
}

list* list_split_at(list* l, size_t index)
{
	if (index > l->size)
		return NULL;

	list* n = list_init();
	if (!n)
		return NULL;

	for (size_t i = index; i < l->size; i++)
		list_push_back(n, list_get(l, i));
	size_t s = l->size;
	for (size_t i = index; i < s; i++)
		list_remove_at(l, index);

	return n;
}

void list_concat(list* list1, list* list2)
{
	for (size_t i = 0; i < list2->size; i++)
		list_push_back(list1, list_get(list2, i));
	list_clear(list2);
}