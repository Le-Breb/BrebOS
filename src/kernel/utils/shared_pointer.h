#pragma once

template <typename T>
class SharedPointer
{
public:
    // The type pointed to.
    using element_type = T;

    /**
    ** \brief Construct a counted reference to a newly allocated object.
    */
    SharedPointer(element_type* p);

    /**
    ** \brief Construct an empty SharedPointer.
    ** Kept separate from the pointer constructor so that it does not require element_type to be complete.
    */
    SharedPointer(decltype(nullptr) = nullptr);

    /**
    ** \brief Destroy the SharedPointer.
    ** The object pointed to is destroyed only if it is not referenced anymore.
    ** Take care to free everything ...
    */
    ~SharedPointer();

    /**
    ** \brief Copy constructor.
    ** \return A new SharedPointer, pointing
    ** to the underlying data of \a other.
    **
    ** Although the implementation is subsumed by the previous, more
    ** generic one, the C++ standard still mandates this specific
    ** signature.  Otherwise, the compiler will provide a default
    ** implementation, which is of course wrong.  Note that the
    ** same applies for the assignment operator.
    */
    SharedPointer(const SharedPointer<element_type>& other);

    /// \name Assignments.
    /// \{

    /**
    ** \brief Replace the underlying data with \a p
    ** A call to \c reset is strictly equivalent to:
    ** \code
    **     r = SharedPointer<element_type>(p);
    ** \endcode
    ** This line implicitly calls the destructor of \a r before assigning
    ** it a new value.
    ** \warning You can't reset your \c SharedPointer with the
    ** same pointer as the current one. In this case, nothing should
    ** be done.
    ** \warning Be mindful of other \c SharedPointers
    ** which could also be pointing to the same data.
    */
    void reset(element_type* p);

    /**
    ** \brief Reference assignment
    ** Its behavior is similar to \c reset() but you
    ** have to use the existing SharedPointer \a other
    ** instead of creating a new instance.
    */
    SharedPointer& operator=(const SharedPointer& other);
    /// \}

    /// \name Access to data.
    /// \{

    // Access via a reference.
    T& operator*() const;
    // Access via a pointer.
    T* operator->() const;

    // Returns the underlying data pointer.
    element_type* get() const;

    // Returns the number of SharedPointer objects referring to the same managed
    // object
    long use_count() const;
    /// \}

    /// \name Equality operators.
    /// \{

    /**
    ** \brief Reference comparison.
    ** Returns true if the two references point to the same object.
    */
    template <typename U>
    bool operator==(const SharedPointer<U>& rhs) const;

    /**
    ** \brief Reference comparison.
    ** Returns false if the two references point to the same object.
    */
    template <typename U>
    bool operator!=(const SharedPointer<U>& rhs) const;

    /**
    ** \brief Reference comparison.
    ** Returns true if this points to \a p.
    */
    bool operator==(const element_type* p) const;

    /**
    ** \brief Reference comparison.
    ** Returns false if this points to \a p.
    */
    bool operator!=(const element_type* p) const;

    /**
     ** \brief Move assignment operator for SharedPointer.
     **
     ** This operator transfers ownership of the managed object from the
     ** \a other to this SharePointer.
     ** After the assignment, the \a other will be empty.
     ** \warning Don't forget to update the reference counter.
     **
     ** Returns a reference to this SharedPointer.
     */
    SharedPointer<element_type>& operator=(SharedPointer&& other) noexcept;

    /**
     ** \brief Move constructor for SharedPointer.
     **
     ** This constructor transfers ownership of the managed object from \a other
     ** SharedPointer to the newly created SharedPointer.
     **
     */
    SharedPointer(SharedPointer&& other);

    /// \}

    /**
    ** \brief Test for validity
    ** Returns true if the reference is valid, i.e. it points to an object.
    ** This conversion operator allows users to write:
    **
    ** \code
    ** SharedPointer<int> p1;
    ** if (p1) // Here false
    **   ...
    ** \endcode
    */
    operator bool() const;

    /**
    ** \brief Test fellowship.
    ** Returns true if the SharedPointer points to an object which
    ** is of the specified type.
    ** Look at the given example file shared_pointer_is_a.cc
    ** to better understand its purpose.
    */
    template <typename U>
    bool is_a() const;

private:
    /**
    ** Shared between all ref pointers to \a data_. \a destroy is set where the pointer is created (where T is
    ** complete), so that releasing the last reference does not require T to be complete.
    */
    struct ControlBlock
    {
        long count;
        void (*destroy)(void*);
    };

    // Only instantiated where its address is taken, i.e. where a pointer is created
    static void delete_data(void* p);

    // Drop this reference, destroying the data and the control block if it was the last one.
    void release();

    // Pointer to the underlying data.
    element_type* data_;
    ControlBlock* cb_;
};

#include "shared_pointer.hxx"
