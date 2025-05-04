#ifndef STREAM_H
#define STREAM_H
#include <kstddef.h>

#define STREAM_CAPACITY 4096

class circular_stream {
public:
    enum Policy
    {
        STREAM_MANUAL,
        STREAM_NEWLINE_FLUSHED,
        STREAM_NOBUFFER
    };
    explicit circular_stream(Policy policy = STREAM_NEWLINE_FLUSHED);
    ~circular_stream();
    void write(char c);
    void flush();
private:
    char buffer[STREAM_CAPACITY + 1]{};
    uint size, capacity;
    Policy policy;
};



#endif //STREAM_H
