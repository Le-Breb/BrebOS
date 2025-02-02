#ifndef STREAM_H
#define STREAM_H
#include <kstddef.h>

#define STREAM_CAPACITY 4096

class stream {
public:
    enum Policy
    {
        STREAM_BUFFERED,
        STREAM_NEWLINE_FLUSHED,
        STREAM_NOBUFFER
    };
    explicit stream(Policy policy = STREAM_NEWLINE_FLUSHED);
    ~stream();
    void write(char c);
    void flush();
private:
    char buffer[STREAM_CAPACITY + 1]{};
    uint size, capacity;
    Policy policy;
};



#endif //STREAM_H
