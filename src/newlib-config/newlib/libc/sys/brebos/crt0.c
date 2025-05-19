#include <fcntl.h>

extern void exit(int code);
extern int main ();
extern void _init_signal();

void _start() {
    _init_signal();
    int ex = main();
    exit(ex);
}
