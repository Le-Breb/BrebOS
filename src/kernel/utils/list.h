#ifndef INCLUDE_LIST_H
#define INCLUDE_LIST_H

template<class E>
class list {

private:
	class Node {
	public:
		E value;
		Node *next;

		Node();

		explicit Node(E val);
	};

    Node *head;
    int s;

public:

	class Iterator
	{
		Node* n;
	public:
		// ReSharper disable once CppNonExplicitConvertingConstructor
		Iterator(Node *n) : n(n){} // NOLINT(*-explicit-constructor)

		E& operator*() const { return n->value; }

		Iterator& operator++() { n = n->next; return *this; }

		bool operator!=(const Iterator& other) const {
			return n != other.n;
		}
	};

    const static int SORT_ASC = 0;
    const static int SORT_DESC = 1;

    list();

    /**
     * Appends the specified element to the end of this list.
     *
     * @param e - the element to add
     */
    void add(E e);

    /**
     * Inserts the specified element at the specified position in this list.
     * Shifts the element currently at that position (if any) and any subsequent
     * elements to the right (adds one to their indices).
     *
     * @param index index at which the specified element is to be inserted
     * @param e  element to be inserted
     */
    bool add(int index, E e);

    /**
     * Inserts the specified element at the beginning of this list.
     *
     * @param e  the element to add
     */
    void addFirst(E e);

    /**
     * Appends the specified element to the end of this list.
     *
     * @param e the element to add
     */
    void addLast(E e);

    /**
     * Removes the first occurrence of the specified element from this list, if it is present.
     * If this list does not contain the element, it is unchanged.
     *
     * @param e
     */
    bool remove(E e);

    /**
     * Removes and returns the first element from this list.
     */
    bool removeFirst();

    /**
     * Removes and returns the last element from this list.
     */
    bool removeLast();

    /**
     * Returns the number of elements in this list.
     *
     * @return the number of elements in this list
     */
    [[nodiscard]] int size() const;

    /**
     * Returns true if this list contains the specified element.
     *
     * @param e element whose presence in this list is to be tested
     * @return <b>true</b> if this list contains the specified element
     */
    bool contains(E e);

    /**
     * Reverse the whole list
     */
    void reverse();

    /**
     * Sort the list by the given order
     *
     * @param order the order to sort the list
     */
    void sort(int order);

    /**
     * Removes all of the elements from this list and free the allocated memory.
     */
    void clear();

	[[nodiscard]] E* get(int index) const;

	[[nodiscard]]
	Iterator begin() const;

	[[nodiscard]]
	Iterator end() const;
};

#include "list.hxx"

#endif /* !INCLUDE_LIST_H */