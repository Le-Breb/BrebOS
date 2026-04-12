#include <fcntl.h>

extern void exit(int code);
extern int main ();

// For now this is not used, and manually replaced by start_program.s by setup_newlib.sh
void _start() {
    int ex = main();
    exit(ex);
}
