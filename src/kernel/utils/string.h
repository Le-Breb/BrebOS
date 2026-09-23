#pragma once

#include <stddef.h>

[[noreturn]]
extern int irrecoverable_error(const char* format, ...);
typedef unsigned int uint;

/**
 * String classes that mimics std::string.
 *
 * Has a local field loc_dat to store short strings. If more than STRING_LOC_DAT_LENGTH bytes are needed, then data is
 * store on the heap.
 * The field `data` points to the string content in any case, eg either heap data either loc_dat
 * When heap-allocated, m_capacity grows geometrically (factor 2) to amortize reallocations. m_capacity and loc_dat
 * are never needed at the same time (heap buffer vs. local storage), so they share the same memory via a union.
 */
class string
{
    static constexpr size_t STRING_LOC_DAT_LENGTH = 16;
    size_t m_size; // Size of the string, excluding null terminator
    char* m_data;
    union
    {
        size_t m_capacity; // Usable capacity excluding null terminator; valid only when !uses_loc_dat()
        char loc_dat[STRING_LOC_DAT_LENGTH];
    };

    [[nodiscard]]
    bool uses_loc_dat() const { return m_data == loc_dat; }
    [[nodiscard]]
    static bool should_use_loc_dat(size_t size) { return size < STRING_LOC_DAT_LENGTH; }


    string(const char* str, size_t n);

    void realloc_for(size_t new_size);
    void assign_from(const char* str, size_t n);
    void append(const char* str, size_t n);

    class Iterator
    {
        friend class string;
        string& str;
        size_t index;

    public:
        explicit Iterator(string& str, size_t index) : str(str), index(index) {}
        char& operator*() const
        {
            if (index >= str.m_size)
                irrecoverable_error("%s: out of bounds index", __PRETTY_FUNCTION__);
            return str[index];
        }
        Iterator& operator++() { ++index; return *this; }
    };
public:
    // Constructors
    string();
    string(const char* str);
    string(string&& other) noexcept;
    string(const string& other);
    string(size_t n, char c);
    string(char c);
    string& operator=(const string& other);
    string& operator=(string&& other) noexcept;
    string& operator=(const char* str);
    string& operator=(char c);

    ~string();

    // Equality operators
    bool operator==(const string& other) const;
    bool operator==(const char* str) const;

    // Methods
    void clear();
    void reserve(size_t new_cap);

    // Getters
    bool empty() const;
    size_t size() const;
    size_t capacity() const;
    const char* c_str() const;
    char* data();
    const char* data() const;

    // Indexing operators
    char operator[](size_t index) const;
    char& operator[](size_t index);

    // Concatenation operators
    string& operator+=(char c);
    string& operator+=(const char* str);
    string& operator+=(const string& other);
    string operator+(const string& other) const;

    // Iterators
    Iterator begin();
    Iterator end();
};