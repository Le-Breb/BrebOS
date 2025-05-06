#ifndef MIN_HEAP_H
#define MIN_HEAP_H

#include "kstddef.h"

template<typename T>
class MinHeap
{
	uint count = 0;
	uint capacity;
	T* elements;

	// Given the index i of element, return the index of that element's parent in the heap
	[[nodiscard]]
	static uint parent(uint i);

	// Given the index i of element, return the index of that element's left child in the heap
	[[nodiscard]]
	uint left(uint i) const;

	// Given the index i of element, return the index of that element's right child in the heap
	[[nodiscard]]
	uint right(uint i) const;

	void min_heapify(uint i);

public:
	explicit MinHeap(uint capacity);

	~MinHeap();

	//Insert a given element elem into the heap
	void insert(T);

	// Return the minimum of the heap
	T min() const;

	// Delete the minimum from the heap and return the minimum
	T delete_min();

	[[nodiscard]]
	bool empty() const;
};

#include "min_heap.hxx"

#endif //MIN_HEAP_H