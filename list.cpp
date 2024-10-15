#include "memory.h"
#include "list.h"

list::list() : size(0), head(NULL), tail(NULL)
{
}

void* list::push_front(void* element)
{
	list_item* i = (list_item*) malloc(sizeof(list_item));
	if (!i)
		return 0;

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


void* list::push_back(void* element)
{
	list_item* i = (list_item*) malloc(sizeof(list_item));
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

size_t list::get_size() const
{
	return size;
}

void* list::get_at(size_t index) const
{
	if (size == 0 || index >= size)
		return NULL;

	list_item* i = head;
	for (size_t c = 0; c < index; c++)
		i = i->next;

	return i->data;
}

void* list::insert_at(void* element, size_t index)
{
	if (index > size)
		return 0;

	if (index == NULL)
		return push_front(element);

	list_item* i = head;
	for (size_t c = 0; c < index - 1; c++)
		i = i->next;

	list_item* n = (list_item*) malloc(sizeof(list_item));
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

int list::find(void* element) const
{
	list_item* i = head;
	for (int c = 0; i; c++, i = i->next)
		if (i->data == element)
			return c;

	return -1;
}

void* list::remove_at(size_t index)
{
	if (index >= size)
		return NULL;

	if (index == 0)
	{
		void* e = head->data;
		list_item* h = head;
		head = h->next;
		if (head)
			head->prev = NULL;
		else
			tail = NULL;
		delete h;
		size--;

		return e;
	}

	list_item* i = head;
	for (size_t c = 0; c < index - 1; c++, i = i->next)
		continue;

	list_item* c = i->next;
	void* e = c->data;
	i->next = c->next;
	if (i->next)
		i->next->prev = i;
	else
		tail = i;

	size--;

	delete c;

	return e;
}

void list::clear()
{
	list_item* i = head;

	while (i)
	{
		list_item* n = i->next;
		delete i;
		i = n;
	}

	size = 0;
	head = tail = NULL;
}

void list::reverse()
{
	list_item* i = head;

	if (!i)
		return;

	while (i)
	{
		list_item* n = i->next;
		i->next = i->prev;
		i->prev = n;

		i = n;
	}

	list_item* tmp = head;
	head = tail;
	tail = tmp;
}

list* list::split_at(size_t index)
{
	if (index > size)
		return NULL;

	list* n = (list*) malloc(sizeof(list));
	if (!n)
		return NULL;

	for (size_t i = index; i < size; i++)
		n->push_back(get_at(i));
	size_t s = size;
	for (size_t i = index; i < s; i++)
		remove_at(index);

	return n;
}

void list::concat(list* list2)
{
	for (size_t i = 0; i < list2->size; i++)
		push_back(list2->get_at(i));
	list2->clear();
}

list::~list()
{
	clear();
}
