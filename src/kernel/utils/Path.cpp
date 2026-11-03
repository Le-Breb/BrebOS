#include "Path.h"

#include "kstring.h"
#include "string.h"
#include "../core/memory/c_memory.h"

Path::~Path()
{
    for (const auto el : elem)
        delete[] el;
}

Result<Path> Path::build(const char* pathname)
{
    if (!pathname || !*pathname)
        return MAKE_ERR("Incorrect path (null or empty)");

    char* svptr; // strtok_r internal ptr
    char* cpy = strdup(pathname);

    vector<const char*> elems{};
    char* token = strtok_r(cpy, "/", &svptr);

    while (token)
    {
        elems.push_back(strdup(token));
        token = strtok_r(nullptr, "/", &svptr);
    }

    free(cpy);

    const bool is_absolute = pathname[0] == '/';
    return make_ok<Path>(elems, is_absolute);
}

std::optional<Path> Path::get_parent() const
{
    if (!elem.size())
        return std::nullopt;

    vector<const char*> parent_elem{};
    for (size_t i = 0; i < elem.size() - 1; i++)
        parent_elem.push_back(strdup(elem[i]));

    return std::optional{Path(parent_elem, is_absolute)};
}

string Path::operator*() const
{
    size_t tot_len = is_absolute ? 1 : 0; // starting slash
    for (const auto el : elem)
        tot_len += strlen(el) + 1; // token + /

    string str(tot_len, '\0');
    char* p = str.data();
    if (is_absolute)
        *p++ = '/';
    for (const auto el : elem)
    {
        const size_t l = strlen(el);
        memcpy(p, el, l);
        p += l;
        *p++ = '/';
    }

    return str;
}

Path::Iterator Path::begin() const
{
    return Iterator(*this, 0);
}

Path::Iterator Path::end() const
{
    return Iterator(*this, elem.size());
}

PathView::PathView(const char* str) : PathView(str, str ? strlen(str) : 0)
{
}

PathView::PathView(const char* str, size_t len) : str(str), len(len), trailing_slash(false)
{
    // Strip trailing slashes, but keep a lone root "/"
    while (this->len > 1 && str[this->len - 1] == '/')
    {
        this->len--;
        trailing_slash = true;
    }
}

Result<PathView> PathView::build(const char* str)
{
    if (!str)
        return MAKE_ERR("Incorrect path (null)");
    if (!*str)
        return MAKE_ERR("Incorrect path (empty)");

    // Single scan, bounded by PATH_MAX_LEN so an unterminated string can't make us run forever
    size_t len = 0;
    size_t comp_len = 0;
    for (; str[len]; len++)
    {
        if (len + 1 >= PATH_MAX_LEN)
            return MAKE_ERR("Path too long (max %u chars)", static_cast<unsigned>(PATH_MAX_LEN - 1));

        if (str[len] == '/')
            comp_len = 0;
        else if (++comp_len > NAME_MAX_LEN)
            return MAKE_ERR("Path component too long (max %u chars)", static_cast<unsigned>(NAME_MAX_LEN));
    }

    return make_ok<PathView>(str, len);
}

bool PathView::Component::operator==(const char* other) const
{
    if (!other)
        return false;
    // strncmp stops at other's NUL, then other[len] check rejects other being longer than us
    return strncmp(ptr, other, len) == 0 && other[len] == '\0';
}

bool PathView::Component::operator==(const string& other) const
{
    return other.size() == len && memcmp(ptr, other.data(), len) == 0;
}

bool PathView::is_absolute() const
{
    return len > 0 && str[0] == '/';
}

std::optional<PathView> PathView::parent() const
{
    // No components ("/" or ""): no parent, same as Path::get_parent
    if (last().len == 0)
        return std::nullopt;

    const char* e = str + len;

    // Drop last component
    while (e > str && e[-1] != '/')
        --e;
    // Drop the slashes separating it from its parent
    while (e > str && e[-1] == '/')
        --e;

    if (e == str && is_absolute())
        return PathView(str, 1); // root

    return PathView(str, static_cast<size_t>(e - str));
}

PathView::Component PathView::last() const
{
    const char* e = str + len;
    const char* p = e;
    while (p > str && p[-1] != '/')
        --p;

    return {p, static_cast<size_t>(e - p)};
}

PathView::Iterator PathView::begin() const
{
    return {str, str + len};
}

PathView::Iterator PathView::end() const
{
    return {str + len, str + len};
}

PathView::Iterator::Iterator(const char* start, const char* end) : cur(start), cur_len(0), end(end)
{
    advance_from(start);
}

void PathView::Iterator::advance_from(const char* p)
{
    while (p < end && *p == '/') // collapse "//"
        ++p;

    cur = p;
    while (p < end && *p != '/')
        ++p;

    cur_len = p - cur;
}

PathView::Iterator& PathView::Iterator::operator++()
{
    advance_from(cur + cur_len);
    return *this;
}
