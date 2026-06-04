#include <stdint.h>
#include <stdlib.h>
#include <mlibc/elf/startup.h>
#include <sys/auxv.h>

extern "C" void __dlapi_enter(uintptr_t *);

extern char **environ;
extern "C" void _init(void) __attribute__((weak));
extern "C" void _fini(void) __attribute__((weak));

size_t __hwcap;

extern "C" void __mlibc_entry(uintptr_t *entry_stack, int (*main_fn)(int argc, char *argv[], char *env[])) {
    __dlapi_enter(entry_stack);

    // mlibc (surpisingly) does not call _init and _fini when built statically,
    // so I do it manually here
    // However those symbols do not exist when building mlibc dynamically, so
    // they're defined weak and only called when they exist
    if (_init)
        _init();
    __hwcap = getauxval(AT_HWCAP);
    auto result = main_fn(mlibc::entry_stack.argc, mlibc::entry_stack.argv, environ);
    if (_fini)
        _fini();

    exit(result);
}
