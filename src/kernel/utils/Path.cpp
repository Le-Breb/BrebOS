#include "Path.h"

#include "kstring.h"
#include "string.h"
#include "../core/memory/c_memory.h"

Path::~Path()
{
    for (const auto el : elem)
        delete[] el;
}

Result<Path> Path::build_path(const char* pathname)
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
