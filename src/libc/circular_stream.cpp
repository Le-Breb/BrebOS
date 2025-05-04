#include "circular_stream.h"

#include <kstdio.h>

circular_stream::circular_stream(Policy policy) : size(0), capacity(STREAM_CAPACITY + 1),
                                                     policy(policy)
{
}

circular_stream::~circular_stream()
= default;

void circular_stream::write(char c)
{
    buffer[size++] = c;

    if (size == capacity)
    {
        flush();
        return;
    }

    switch (policy)
    {
    case STREAM_MANUAL:
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

void circular_stream::flush()
{
    buffer[size] = '\0';
    puts(buffer);

    size = 0;
}
