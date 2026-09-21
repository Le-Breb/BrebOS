#include "string.h"

#include "kstring.h"
#include "../core/memory/c_memory.h"

string::string() : m_size(0), m_data(loc_dat), loc_dat{}
{
}

string::string(const char* str)
{
    m_size = strlen(str);

    if (should_use_loc_dat(m_size))
    {
        m_data = loc_dat;
        strcpy(m_data, str);
    }
    else
        m_data = strdup(str);
}

string::string(string&& other) noexcept : m_size(other.m_size)
{
    if (other.uses_loc_dat())
    {
        m_data = loc_dat;
        strcpy(m_data, other.m_data);
    }
    else
        m_data = other.m_data;

    other.m_size = 0;
    other.m_data = other.loc_dat;
    other.m_data[0] = '\0';
}

string::string(const string& other) : m_size(other.m_size)
{
    if (should_use_loc_dat(m_size))
    {
        m_data = loc_dat;
        memcpy(m_data, other.m_data, m_size + 1);
    }
    else
        m_data = strdup(other.m_data);
}

string::string(size_t n, char c)
{
    if (should_use_loc_dat(n))
    {
        m_data = loc_dat;
        memset(loc_dat, c, n);
    }
    else
    {
        m_data = (char*)malloc(n + 1);
        memset(m_data, c, n);
    }

    m_data[n] = '\0';
    m_size = n;
}

string::string(char c) : m_size(1), m_data(loc_dat)
{
    loc_dat[0] = c;
    loc_dat[1] = '\0';
}

string& string::operator=(const string& other)
{
    if (this == &other)
        return *this;

    if (other.uses_loc_dat())
    {
        if (!uses_loc_dat())
            free(m_data);
        m_data = loc_dat;
    }
    else
    {
        if (uses_loc_dat())
            m_data = (char*)malloc(other.m_size + 1);
        else
            m_data = (char*)realloc(m_data, other.m_size + 1);
    }

    strcpy(m_data, other.m_data);
    m_size = other.m_size;

    return *this;
}

string& string::operator=(string&& other) noexcept
{
    if (this == &other)
        return *this;

    if (other.uses_loc_dat())
    {
        if (!uses_loc_dat())
        {
            free(m_data);
            m_data = loc_dat;
        }
        strcpy(m_data, other.m_data);
    }
    else
    {
        if (uses_loc_dat())
        {
            m_data = (char*)malloc(other.m_size + 1);
            strcpy(m_data, other.m_data);
        }
        else
        {
            free(m_data);
            m_data = other.m_data;
        }
    }

    m_size = other.m_size;

    other.m_size = 0;
    other.m_data = other.loc_dat;
    other.m_data[0] = '\0';

    return *this;
}

string& string::operator=(const char* str)
{
    const size_t new_size = strlen(str);
    if (should_use_loc_dat(new_size))
    {
        if (!uses_loc_dat())
        {
            free(m_data);
            m_data = loc_dat;
        }
    }
    else
    {
        if (uses_loc_dat())
            m_data = (char*)malloc(new_size + 1);
        else
            m_data = (char*)realloc(m_data, new_size + 1);
    }

    strcpy(m_data, str);
    m_size = new_size;

    return *this;
}

string& string::operator=(char c)
{
    if (!uses_loc_dat())
    {
        free(m_data);
        m_data = loc_dat;
    }

    loc_dat[0] = c;
    loc_dat[1] = '\0';

    m_size = 1;

    return *this;
}

string::~string()
{
    if (!uses_loc_dat())
        free(m_data);
    m_size = 0;
}

bool string::operator==(const string& other) const
{
    return !strcmp(m_data, other.m_data);
}

bool string::operator==(const char* str) const
{
    return !strcmp(m_data, str);
}

void string::clear()
{
    if (!uses_loc_dat())
    {
        free(m_data);
        m_data = loc_dat;
    }

    m_size = 0;
    loc_dat[0] = '\0';
}

bool string::empty() const
{
    return m_size == 0;
}

size_t string::size() const
{
    return m_size;
}

const char* string::c_str() const
{
    return m_data;
}

char* string::data()
{
    return m_data;
}

const char* string::data() const
{
    return m_data;
}

char string::operator[](size_t index) const
{
    if (index >= m_size)
        irrecoverable_error("%s: index (%zu) > size (%zu)", __PRETTY_FUNCTION__ ,index, m_size);

    return m_data[index];
}

char& string::operator[](size_t index)
{
    if (index >= m_size)
        irrecoverable_error("%s: index (%zu) > size (%zu)", __PRETTY_FUNCTION__, index, m_size);

    return m_data[index];
}

string& string::operator+=(char c)
{
    const size_t new_size = m_size + 1;

    if (should_use_loc_dat(new_size))
    {
        if (!uses_loc_dat())
        {
            free(m_data);
            m_data = loc_dat;
        }
    }
    else
    {
        if (uses_loc_dat())
        {
            m_data = (char*)malloc(new_size + 1);
            strcpy(m_data, loc_dat);
        }
        else
            m_data = (char*)realloc(m_data, new_size + 1);
    }

    m_data[m_size] = c;
    m_data[m_size + 1] = '\0';
    m_size = new_size;

    return *this;
}

string& string::operator+=(const char* str)
{
    const size_t other_len = strlen(str);
    const size_t new_size = m_size + other_len;

    if (should_use_loc_dat(new_size))
    {
        if (!uses_loc_dat())
        {
            free(m_data);
            m_data = loc_dat;
        }
    }
    else
    {
        if (uses_loc_dat())
        {
            m_data = (char*)malloc(new_size + 1);
            strcpy(m_data, loc_dat);
        }
        else
            m_data = (char*)realloc(m_data, new_size + 1);
    }

    strcpy(m_data + m_size, str);
    m_size = new_size;

    return *this;
}

string& string::operator+=(const string& other)
{
    const auto other_len = other.m_size;
    const size_t new_size = m_size + other_len;

    if (should_use_loc_dat(new_size))
    {
        if (!uses_loc_dat())
        {
            free(m_data);
            m_data = loc_dat;
        }
    }
    else
    {
        if (uses_loc_dat())
        {
            m_data = (char*)malloc(new_size + 1);
            strcpy(m_data, loc_dat);
        }
        else
            m_data = (char*)realloc(m_data, new_size + 1);
    }

    strcpy(m_data + m_size, other.m_data);
    m_size = new_size;

    return *this;
}

string string::operator+(const string& other) const
{
    string result(*this);
    result += other;
    return result;
}

string::Iterator string::begin()
{
    return Iterator(*this, 0);
}

string::Iterator string::end()
{
    return Iterator(*this, m_size);
}
