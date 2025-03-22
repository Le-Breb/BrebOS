#include "stream.h"

#include <kstdio.h>

stream::stream(Policy policy) : size(0), capacity(STREAM_CAPACITY + 1),
                                                     policy(policy)
{
}

stream::~stream()
= default;

void stream::write(char c)
{
    buffer[size++] = c;

    if (size == capacity)
    {
        flush();
        return;
    }

    switch (policy)
    {
    case STREAM_BUFFERED:
        break;
    case STREAM_NEWLINE_FLUSHED:
        if (c == '\n')
            flush();
        break;
    case STREAM_NOBUFFER:
        flush();
        break;
    }
}

void stream::flush()
{
    buffer[size] = '\0';
    puts(buffer);

    size = 0;
}
