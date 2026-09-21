#pragma once

#include <optional>

#include "kstring.h"
#include "vector.h"
#include "Result.h"
#include "string.h"

class Path
{
    vector<const char*> elem;
    const bool is_absolute;
public:
    class Iterator
    {
        friend class Path;
        const Path& path;
        size_t index;
    public:
        Iterator(const Path& path, size_t index) : path(path), index(index) {};
        const char* operator*() const {return path.elem[index];}
        const char** operator->() const {return &path.elem[index];} // that's weird innit ?
        bool operator!=(const Iterator& other) const { return index != other.index; }
        Iterator& operator++() { ++index; return *this; };
        Iterator& operator--() { --index; return *this; }
        size_t operator-(const Iterator& other) const { return index - other.index; }
    };

    Path(const vector<const char*>& elem, bool is_absolute) : elem(elem), is_absolute(is_absolute)
    {

    };

    // ReSharper disable once CppFunctionIsNotImplemented
    // This is prevent just to shut up CLion who reports false positive compile errors when using TRY_OR
    Path(const Path& other);

    explicit Path(Path&& other) noexcept : elem(std::move(other.elem)), is_absolute(other.is_absolute)
    {

    }

    ~Path();

    [[nodiscard]]
    static Result<Path> build_path(const char* pathname);

    std::optional<Path> get_parent() const;

    string operator*() const;

    [[nodiscard]]
    Iterator begin() const;
    [[nodiscard]]
    Iterator end() const;
};
