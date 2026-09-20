#ifndef INCLUDE_SYSCALL_NUMBERS_H
#define INCLUDE_SYSCALL_NUMBERS_H

namespace SyscallNumber
{
    constexpr int TERMINATE_PROCESS      = 1;
    constexpr int PRINTF                 = 2;
    constexpr int GDB_NOTIFY_ELF         = 3;
    constexpr int GET_KEY                = 4;
    constexpr int GET_PID                = 5;
    constexpr int SHUTDOWN               = 6;
    constexpr int SLEEP                  = 7;
    constexpr int MALLOC                 = 8;
    constexpr int FREE                   = 9;
    constexpr int MKDIR                  = 10;
    constexpr int TOUCH                  = 11;

    constexpr int CLEAR_SCREEN           = 13;
    constexpr int WAIT_PID               = 14;
    constexpr int WGET                   = 15;
    constexpr int STAT                   = 16;
    constexpr int CALLOC                 = 17;
    constexpr int REALLOC                = 18;
    constexpr int TCBSET                 = 19;
    constexpr int EXECVE                 = 20;
    constexpr int FEH                    = 21;
    constexpr int LOCK_FLUSHING          = 22;
    constexpr int UNLOCK_FLUSHING        = 23;
    constexpr int GET_SCREEN_DIMENSIONS  = 24;
    constexpr int LOAD_FILE              = 25;
    constexpr int WRITE                  = 26;
    constexpr int OPENDIR                = 27;
    constexpr int FORK                   = 28;
    constexpr int GET_FILE_SIZE          = 29;
    constexpr int OPEN                   = 30;
    constexpr int READ                   = 31;
    constexpr int CLOSE                  = 32;
    constexpr int LSEEK                  = 33;
    constexpr int FSTAT                  = 34;
    constexpr int KILL                   = 35;
    constexpr int SIGNAL                 = 36;
    constexpr int SIGNAL_RETURN          = 37;
    constexpr int FCNTL                  = 38;
    constexpr int DUP2                   = 39;
    constexpr int DUP                    = 40;
    constexpr int PIPE                   = 41;

    constexpr int GETCWD                 = 43;
    constexpr int CHDIR                  = 44;
    constexpr int SIGPROCMASK            = 45;
    constexpr int ISATTY                 = 46;
    constexpr int SIGACTION              = 47;
    constexpr int MMAP                   = 48;
    constexpr int MPROTECT               = 49;
    constexpr int GDB_UNLOAD_ELF         = 50;
    constexpr int GETDENTS               = 51;

    constexpr int DEBUG_PRINT            = 400;
}

#endif //INCLUDE_SYSCALL_NUMBERS_H
