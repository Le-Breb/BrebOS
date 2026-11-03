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
    static Result<Path> build(const char* pathname);

    std::optional<Path> get_parent() const;

    string operator*() const;

    [[nodiscard]]
    Iterator begin() const;
    [[nodiscard]]
    Iterator end() const;
};

class PathView
{
    const char* str; // underlying string, NOT OWNED
    size_t len;      // trailing slashes excluded (except for root "/")
    bool trailing_slash;

public:
    struct Component
    {
        const char* ptr; // NOT null terminated, use len
        size_t len;

        [[nodiscard]] bool operator==(const char* other) const;
        [[nodiscard]] bool operator==(const string& other) const;
    };

    class Iterator
    {
        const char* cur; // start of current component
        size_t cur_len;
        const char* end; // one past the last char of the view

        void advance_from(const char* p);

    public:
        Iterator(const char* start, const char* end);
        Component operator*() const { return {cur, cur_len}; }
        Iterator& operator++();
        bool operator!=(const Iterator& other) const { return cur != other.cur; }
        bool operator==(const Iterator& other) const { return cur == other.cur; }
    };

    static constexpr size_t PATH_MAX_LEN = 4096; // includes terminating NUL, like POSIX PATH_MAX
    static constexpr size_t NAME_MAX_LEN = 255;  // max component length, like POSIX NAME_MAX

    // Rejects null, empty, too long paths and too long components
    [[nodiscard]]
    static Result<PathView> build(const char* str);

    // Unchecked: use for trusted kernel strings. Untrusted input must go through build()
    explicit PathView(const char* str);
    PathView(const char* str, size_t len);

    [[nodiscard]] bool is_absolute() const;
    [[nodiscard]] bool is_empty() const { return len == 0; }
    // Path ended with a '/', so POSIX requires the target to be a directory
    [[nodiscard]] bool must_be_dir() const { return trailing_slash; }
    [[nodiscard]] const char* data() const { return str; }
    [[nodiscard]] size_t size() const { return len; }

    // "/a/b" -> "/a", "/a" -> "/", "a" -> "" (relative, i.e. cwd), "/" and "" -> nullopt (no components)
    [[nodiscard]] std::optional<PathView> parent() const;
    [[nodiscard]] Component last() const;

    [[nodiscard]] Iterator begin() const;
    [[nodiscard]] Iterator end() const;
};
