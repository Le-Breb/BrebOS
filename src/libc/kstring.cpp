#include <kstdlib.h>
#include <kstring.h>

extern "C" unsigned long strlen(char const* str)
{
	int len = 0;
	while (str[len])
		len++;
	return len;
}

void* memset(void* ptr, int value, unsigned long num)
{
	unsigned char* p = (unsigned char*) ptr;
	for (unsigned long i = 0; i < num; i++)
		p[i] = value;

	return ptr;
}

char* strcpy(char* dest, const char* src)
{
  	char* ret = dest;
	while (*src)
		*dest++ = *src++;
	*dest = '\0';

    return ret;
}

char* strcat(char* dest, const char* src)
{
	char* ret = dest;

	while (*dest)
		dest++;
	while (*src)
		*dest++ = *src++;
	*dest = '\0';

	return ret;
}

char* strncat(char* dest, const char* src, size_t ssize)
{
	char* ret = dest;

	while (*dest)
		dest++;

	size_t copied = 0;
	while (*src && copied < ssize)
	{
		*dest++ = *src++;
		copied++;
	}
	*dest = '\0';

	return ret;
}

extern "C" void* memcpy(void* dest, const void* src, unsigned long num)
{
	unsigned char* d = (unsigned char*) dest;
	const unsigned char* s = (const unsigned char*) src;
	for (unsigned long i = 0; i < num; i++)
		d[i] = s[i];

	return dest;
}

int strcmp(const char* str1, const char* str2)
{
	for (; *str1 && *str2 && *str1 == *str2; str1++, str2++);

	return *str1 - *str2;
}

int in_set(const char* set, const char c)
{
	unsigned int i = 0;
	while (*(set + i))
	{
		if (*(set + i) == c)
		{
			return 1;
		}

		i++;
	}

	return 0;
}

char* strtok_r(char* str, const char* delim, char** saveptr)
{
	if (str != nullptr)
		*saveptr = str;

	while (**saveptr && in_set(delim, **saveptr))
		(*saveptr)++;

	if (!**saveptr)
		return nullptr;

	char* start = *saveptr;

	while (**saveptr && !in_set(delim, **saveptr))
		(*saveptr)++;

	if (**saveptr)
	{
		**saveptr = '\0';
		(*saveptr)++;
	}

	return start;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
	auto s = (const unsigned char*) s1;
    auto t = (const unsigned char*) s2;
    for (size_t i = 0; i < n; i++)
    {
      	if (s[i] == t[i])
          continue;
        if (s[i] < t[i])
          return -1;
        if (s[i] > t[i])
          return 1;
    }

    return 0;
}

void to_lower_in_place(char* str)
{
  	while (*str)
    {
    	if (*str >= 'A' && *str <= 'Z')
          *str += 'a' - 'A';

        str++;
    }
}

size_t strspn(const char* s, const char* accept)
{
	size_t res = 0;
	while (*s)
	{
		const char* a = accept;
		bool contained = false;
		while (*a)
		{
			if (*a == *s)
			{
				contained = true;
				break;
			}
			a++;
		}
		if (contained)
			res++;
		s++;
	}

	return res;
}

size_t strcspn(const char* s, const char* reject)
{
	size_t res = 0;
	while (*s)
	{
		const char* a = reject;
		bool contained = false;
		while (*a)
		{
			if (*a == *s)
			{
				contained = true;
				break;
			}
			a++;
		}
		if (!contained)
			res++;
		s++;
	}

	return res;
}

char* strdup(const char* s)
{
	auto len = strlen(s);
	char* dest = (char*)malloc(len + 1);

	if (!dest)
		return nullptr;

	memcpy(dest, s, len + 1);
	return dest;
}

char* strndup(const char* s, size_t n)
{
	auto slen = strlen(s);
	auto len = slen > n ? n : slen;
	char* dest = (char*)malloc(len + 1);

	if (!dest)
		return nullptr;

	memcpy(dest, s, len + 1);
	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
	char *p = (char*)dest;
	const char *s = (const char*)src;
	size_t ov = ((p >= s) && (s + n >= p)) ? s + n - p : 0;

	if (n == 0)
		return dest;

	if (ov > 0)
	{
		for (size_t i = n - 1; i > 0; i--)
			*(p + i) = *(s + i);
		*p = *s;
	}
	else
		for (size_t i = 0; i < n; i++)
			*(p + i) = *(s + i);

	return dest;
}

char* strchr(const char* s, int c)
{
	while (*s)
	{
		if (*s == c)
			return (char*)s;
		s++;
	}

	return nullptr;
}

char* strncpy(char *dst, const char* src, size_t dsize)
{
	stpncpy(dst, src, dsize);
	return dst;
}

char* stpncpy(char * dst, const char * src, size_t dsize)
{
	size_t  dlen;

	dlen = strnlen(src, dsize);
	return (char*)memset(mempcpy(dst, src, dlen), 0, dsize - dlen);
}

void* mempcpy(void* dest, const void* src, size_t n)
{
	return (char*)memcpy(dest,src,n) + n;
}

size_t strnlen(const char* s, size_t maxlen)
{
	size_t l = 0;
	for (; *s && l < maxlen; s++, l++) {}

	return l;
}

int strstr_match(const char *in, const char *patt)
{
	size_t i = 0;
	while (in[i] && patt[i] && in[i] == patt[i])
		i++;

	return !patt[i];
}

char* strstr(const char *haystack, const char *needle)
{
	size_t nl = strlen(needle);

	if (nl == 0)
		return nullptr;

	size_t hl = strlen(haystack);
	if (nl > hl)
		return nullptr;

	for (size_t i = 0; i < hl - nl + 1; i++)
		if (strstr_match(haystack + i, needle))
			return (char*)haystack + i;

	return nullptr;
}
