#include "Path.h"

#include "kstring.h"
#include "TmpString.h"

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
    const auto len = strlen(pathname);
    TmpString cpy(len + 1);
    strcat(*cpy, pathname);

    vector<const char*> elems{};
    char* token = strtok_r(*cpy, "/", &svptr);

    while (token)
    {
        elems.push_back(strdup(token));
        token = strtok_r(nullptr, "/", &svptr);
    }

    return make_ok<Path>(elems);
}

std::optional<Path> Path::get_parent() const
{
    if (!elem.size())
        return std::nullopt;

    vector<const char*> parent_elem{};
    for (size_t i = 0; i < elem.size() - 1; i++)
        parent_elem.push_back(strdup(elem[i]));

    return std::optional{Path(parent_elem)};
}

TmpString Path::operator*() const
{
    auto tot_len = 2; // starting slash and trailing 0
    for (const auto el : elem)
        tot_len += strlen(el) + 1; // token + /
    TmpString str(tot_len);
    memset(*str, 0, tot_len);
    (*str)[0] = '/';
    for (const auto el : elem)
        strcat(strcat(*str, el), "/");
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
