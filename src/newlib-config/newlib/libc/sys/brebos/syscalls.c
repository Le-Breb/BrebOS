/* note these headers are all provided by newlib - you don't need to provide them */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>

#undef errno
extern int errno;

void _exit(int status)
{
    // Todo: call fini functions and __cxa_finalize here and not in start_program.s
    // For now, if this function is called directly from within a program, fini functions and __cxa_finalize aren't run
    __asm__ volatile("int $0x80" : : "a"(1), "D"(status));
    __builtin_unreachable();
}
int close(int file) {
  return -1;
}
char *__env[1] = { 0 };
char **environ = __env;
int execve(char *name, char **argv, char **env) {
  int argc = 0;
  for (char** argv2 = argv; *argv2; argv2++, argc++){};

  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(20), "D"(name), "S"(argc), "d"(argv));

  // If this point  is reached, then execve failed
  errno = ENOMEM;
  return -1;
}
int fork(void) {
  pid_t pid;
  __asm__ volatile("int $0x80" : "=a"(pid) : "a"(28));

  if (pid == -1)
      errno = EAGAIN;

  return pid;
}
int fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}
int getpid(void) {
    int pid;
    __asm__ volatile("int $0x80" : "=a"(pid) : "a"(5));
    return pid;
}
int isatty(int file)
{
    return 1;
}
int kill(int pid, int sig) {
  errno = EINVAL;
  return -1;
}
int link(char *old, char *new) {
  errno = EMLINK;
  return -1;
}
int lseek(int file, int ptr, int dir) {
  return 0;
}
int open(const char *name, int flags, ...) {
  return -1;
}
int read(int file, char *ptr, int len) {
  return 0;
}
void* sbrk(ptrdiff_t incr) {
    void* br;
    __asm__ volatile("int $0x80" : "=a"(br) : "a"(27), "D"(incr));

    if (br == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    return br;
}
int stat(const char *file, struct stat *st) {
  st->st_mode = S_IFCHR;
  return 0;
}
long unsigned int times(struct tms *buf) {
  return -1;
}
int unlink(char *name) {
  errno = ENOENT;
  return -1;
}
int wait(int *status) {
    errno = ECHILD;
    return -1;
}
int write(int file, char *ptr, int len)
{
    int count;
    __asm__ volatile("int $0x80" : "=a"(count) : "a"(26), "D"(file), "S"(ptr), "d"(len));
    return count;
}
int gettimeofday(struct timeval *p, void *z);
