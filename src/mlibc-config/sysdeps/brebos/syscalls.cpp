#include <mlibc/sysdeps.hpp>
#include <bits/ensure.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <mlibc/debug.hpp>

#define STUB()                                                         \
    ({                                                                 \
        __ensure(!"STUB sysdep called");                               \
        __builtin_unreachable();                                       \
    })

namespace mlibc {

void SysdepImpl<Exit>::operator()(int status) {
    __asm__ volatile("int $0x80" : : "a"(1), "D"(status));
    __builtin_unreachable();
}

int SysdepImpl<FutexWait>::operator()(int *pointer, int expected, const struct timespec *time) {
    (void)pointer; (void)expected; (void)time;
    STUB();
}

int SysdepImpl<FutexWake>::operator()(int *pointer, bool all) {
    (void)pointer; (void)all;
    STUB();
}

int SysdepImpl<Open>::operator()(const char *pathname, int flags, mode_t mode, int *fd) {
    if (!(flags & O_CREAT || flags & O_TMPFILE))
        mode = 0;

    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(30), "D"(pathname), "S"(flags), "d"(mode));

  if (ret >= 0)
  {
      *fd = ret;
      return 0;
  }

  return -ret;
}

int SysdepImpl<Read>::operator()(int fd, void *buf, size_t count, ssize_t *bytes_read) {
  int r;
  __asm__ volatile("int $0x80" : "=a"(r) : "a"(31), "D"(fd), "S"(buf), "d"(count));

  if (r >= 0)
  {
      *bytes_read = (ssize_t)r;
      return 0;
  }

  return -r;
}

int SysdepImpl<Write>::operator()(int fd, const void *buf, size_t count, ssize_t *bytes_written) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(26), "D"(fd), "S"((int)buf), "d"(count));

    if (ret >= 0)
    {
        *bytes_written = (ssize_t)ret;
        return 0;
    }

    return -ret;
}

int SysdepImpl<Seek>::operator()(int fd, off_t offset, int whence, off_t *new_offset) {
    int noffset;
    __asm__ volatile("int $0x80" : "=a"(noffset) : "a"(33), "D"(fd), "S"((int)offset), "d"(whence));

  if (noffset >= 0)
  {
     *new_offset = noffset;
     return 0;
  }

  return -noffset;
}

int SysdepImpl<Close>::operator()(int fd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(32), "D"(fd));

    if (ret < 0)
        return -ret;
    return 0;
}

int SysdepImpl<ClockGet>::operator()(int clock, time_t *secs, long *nanos) {
    (void)clock; (void)secs; (void)nanos;
    STUB();
}

void SysdepImpl<LibcLog>::operator()(const char *message) {
    size_t len = 0;
    for (; message[len]; len++){}
    [[maybe_unused]] ssize_t written_bytes;
    constexpr int guards_size = 10;
    SysdepImpl<Write>{}(STDERR_FILENO, "==mlibc==\n", guards_size, &written_bytes);
    SysdepImpl<Write>{}(STDERR_FILENO, message, len, &written_bytes);
    SysdepImpl<Write>{}(STDERR_FILENO, "\n=========\n", guards_size + 1, &written_bytes);
}

void SysdepImpl<LibcPanic>::operator()() {
    SysdepImpl<Exit>{}(0xDEAD); // DEAD is an arbitraty value
}

int SysdepImpl<AnonAllocate>::operator()(size_t size, void **pointer) {
    int mem;
    __asm__ volatile("int $0x80" : "=a"(mem) : "a"(17), "D"(1), "S"(size));

    *pointer = (void*)mem;
    
    return mem ? 0 : ENOMEM;
}

int SysdepImpl<AnonFree>::operator()(void *pointer, [[maybe_unused]] size_t size) {
    __asm__ volatile("int $0x80" :  : "a"(9), "D"(pointer));

    return 0;
}

int SysdepImpl<VmMap>::operator()(void *hint, size_t size, int prot, int flags,
                                  int fd, off_t offset, void **window) {
    (void)hint; (void)size; (void)prot; (void)flags;
    (void)fd; (void)offset; (void)window;
    STUB();
}

int SysdepImpl<VmUnmap>::operator()(void *pointer, size_t size) {
    (void)pointer; (void)size;
    STUB();
}

int SysdepImpl<TcbSet>::operator()(void *pointer) {
    int gdt_entry_num;
    __asm__ volatile("int $0x80" : "=a"(gdt_entry_num) : "a"(19), "D"(pointer));

    // Commented as this is done in the kernel
    // asm volatile ("movw %w0, %%gs" : : "q"(gdt_entry_num * 8 + 3) :);

    return 0;
}

int SysdepImpl<Clone>::operator()(void *entry, int *tid_out, void *user_arg) {
    (void)entry; (void)tid_out; (void)user_arg;
    STUB();
}

int SysdepImpl<Execve>::operator()(const char *path,
        char *const *argv, char *const *envp) {
  if (*envp != nullptr)
      mlibc::infoLogger() << "Execve called with ignored non null envp\n" << frg::endlog;

  int argc = 0;
  for (char* const* argv2 = argv; *argv2; argv2++, argc++){};

  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(20), "D"(path), "S"(argc), "d"(argv));

  return ENOMEM;
}

int SysdepImpl<FdToPath>::operator()(int fd, char **path) {
    (void)fd; (void)path;
    STUB();
}

int SysdepImpl<Fork>::operator()(int *child) {
    pid_t pid;
    __asm__ volatile("int $0x80" : "=a"(pid) : "a"(28));

    if (pid == -1)
        return EAGAIN;

    *child = pid;
    return 0;
}

pid_t SysdepImpl<FutexTid>::operator()() {
    return SysdepImpl<GetPid>{}();
}

pid_t SysdepImpl<GetPid>::operator()() {
    int pid;
    __asm__ volatile("int $0x80" : "=a"(pid) : "a"(5));
    return pid;
}

int SysdepImpl<Isatty>::operator()(int fd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(46), "D"(fd));

    __ensure(ret <= 0);

    if (ret < 0)
        return -ret;
    return ret;
}

int SysdepImpl<Kill>::operator()(int pid, int sig) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(35), "D"(pid), "S"(sig));

    if (ret == 0)
        return 0;

    return -ret;
}

int SysdepImpl<PrepareStack>::operator()(void **stack, void *entry,
        void *user_arg, void *tcb, unsigned long *stack_size,
        unsigned long *guard_size, void **stack_base) {
    (void)stack;
    (void)entry;
    (void)user_arg;
    (void)tcb;
    (void)stack_size;
    (void)guard_size;
    (void)stack_base;
    STUB();
}

int SysdepImpl<Rename>::operator()(const char *old_path,
        const char *new_path) {
    (void)old_path; (void)new_path;
    STUB();
}

int SysdepImpl<Rmdir>::operator()(const char *path) {
    (void)path;
    STUB();
}

extern "C" void __sig_return(void);
int SysdepImpl<Sigaction>::operator()(int sig,
        const struct sigaction *act, struct sigaction *oldact) {
    if (!act)
        return EFAULT;

    if (act->sa_flags & SA_SIGINFO)
        mlibc::panicLogger() << "sigaction called with act->sa_sflags with SA_SIGINFO set, which is not supported yet\n" << frg::endlog;

    // Set restorer address (cf. linux x86 implem)
    struct sigaction kact = *act;
    kact.sa_flags |= SA_RESTORER;
    kact.sa_restorer = __sig_return;

    // mlibc::infoLogger() << "act flags: " << frg::hex_fmt{act->sa_flags} << ", kact flags: " << frg::hex_fmt{kact.sa_flags} << "\n" << frg::endlog;

    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(47), "D"(sig), "S"(&kact), "d"(oldact) : "memory");

    if (ret == 0)
        return 0;

    return -ret;
}

int SysdepImpl<Sigprocmask>::operator()(int how,
        const sigset_t *set, sigset_t *retrieve) {
    int ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(45), "D"(how), "S"(set), "d"(retrieve));

    if (ret < 0)
        return -ret;

    return 0;
}

int SysdepImpl<Stat>::operator()(mlibc::fsfd_target fsfdt,
        int fd, const char *path, int flags, struct stat *statbuf) {
    (void)fsfdt;
    (void)fd;
    (void)path;
    (void)flags;
    (void)statbuf;
    STUB();
}

void SysdepImpl<ThreadExit>::operator()() {
    STUB();
}

int SysdepImpl<Unlinkat>::operator()(int fd,
        const char *path, int flags) {
    (void)fd; (void)path; (void)flags;
    STUB();
}

int SysdepImpl<VmProtect>::operator()(void *pointer,
        unsigned long size, int prot) {
    (void)pointer; (void)size; (void)prot;
    STUB();
}

int SysdepImpl<Waitpid>::operator()(int pid, int *status,
        int flags, struct rusage *ru, int *ret_pid) {

    if (ru)
        mlibc::panicLogger() << "waitpid called with non-null ru, this is not supported yet\n" << frg::endlog;
    if (flags)
        mlibc::panicLogger() << "waitpid called with non-null flags (" << frg::hex_fmt{flags} << ", this is not supported yet\n" << frg::endlog;

    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(14), "D"(pid), "S"(status));

    if (ret < 0)
        return -ret;

    *ret_pid = ret;
    return 0;
}

int SysdepImpl<Access>::operator()(const char *path, int mode) {
    (void)path; (void)mode;
    STUB();
}

int SysdepImpl<Chdir>::operator()(const char *path) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret): "a"(44), "D"(path));

    if (ret < 0)
        return -ret;

    return 0;
}

int SysdepImpl<Chmod>::operator()(const char *pathname, mode_t mode) {
    (void)pathname; (void)mode;
    STUB();
}

int SysdepImpl<ClockSet>::operator()(int clock, time_t secs, long nanos) {
    (void)clock; (void)secs; (void)nanos;
    STUB();
}

int SysdepImpl<Dup>::operator()(int fd, int flags, int *newfd) {
    if (flags)
        mlibc::panicLogger() << "dup called with non-zero flags, this is not supported yet\n" << frg::endlog;

    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(40), "D"(fd));

    if (ret >= 0)
    {
        *newfd = ret;
        return 0;
    }

    return ret;
}

int SysdepImpl<Dup2>::operator()(int fd, int flags, int newfd) {
    if (flags)
        mlibc::panicLogger() << "Dup2 called with non-zero flags (probably due to a call to dup3. This is not supported yet\n" << frg::endlog;


    int ret;

    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(39), "D"(fd), "S"(newfd));

    if (ret < 0)
        return -ret;

    return 0;
}

int SysdepImpl<Faccessat>::operator()(int dirfd, const char *pathname, int mode, int flags) {
    (void)dirfd; (void)pathname; (void)mode; (void)flags;
    STUB();
}

int SysdepImpl<Fadvise>::operator()(int fd, off_t offset, off_t length, int advice) {
    (void)fd; (void)offset; (void)length; (void)advice;
    STUB();
}

int SysdepImpl<Fallocate>::operator()(int fd, off_t offset, size_t size) {
    (void)fd; (void)offset; (void)size;
    STUB();
}

int SysdepImpl<Fchdir>::operator()(int fd) {
    (void)fd;
    STUB();
}

int SysdepImpl<Fchmod>::operator()(int fd, mode_t mode) {
    (void)fd; (void)mode;
    STUB();
}

int SysdepImpl<Fchmodat>::operator()(int fd, const char *pathname, mode_t mode, int flags) {
    (void)fd; (void)pathname; (void)mode; (void)flags;
    STUB();
}

int SysdepImpl<Fchownat>::operator()(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) {
    (void)dirfd; (void)pathname; (void)owner; (void)group; (void)flags;
    STUB();
}

int SysdepImpl<Fcntl>::operator()(int fd, int request, va_list args, int *result) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(38), "D"(fd), "S"(request), "d"(args));

    if (ret >= 0)
    {
        *result = ret;
        return 0;
    }


    return -ret;
}

int SysdepImpl<Fdatasync>::operator()(int fd) {
    (void)fd;
    STUB();
}

int SysdepImpl<Fexecve>::operator()(int fd, char *const argv[], char *const envp[]) {
    (void)fd; (void)argv; (void)envp;
    STUB();
}

int SysdepImpl<Fsync>::operator()(int fd) {
    (void)fd;
    STUB();
}

int SysdepImpl<Ftruncate>::operator()(int fd, size_t size) {
    (void)fd; (void)size;
    STUB();
}

int SysdepImpl<GetCwd>::operator()(char *buffer, size_t size) {
    char* ret;
    int err;
    __asm__ volatile("int $0x80" : "=a"(ret), "=D"(err) : "a"(43), "D"(buffer), "S"(size));

    if (ret == nullptr)
        return err;
    return 0;
}

gid_t SysdepImpl<GetEgid>::operator()() {
    STUB();
}

int SysdepImpl<GetEntropy>::operator()(void *buffer, size_t length) {
    (void)buffer; (void)length;
    STUB();
}

uid_t SysdepImpl<GetEuid>::operator()() {
    STUB();
}

gid_t SysdepImpl<GetGid>::operator()() {
    STUB();
}

int SysdepImpl<GetGroups>::operator()(size_t size, gid_t *list, int *ret) {
    (void)size; (void)list; (void)ret;
    STUB();
}

int SysdepImpl<GetHostname>::operator()(char *buffer, size_t bufsize) {
    (void)buffer; (void)bufsize;
    STUB();
}

int SysdepImpl<GetLoginR>::operator()(char *name, size_t name_len) {
    (void)name; (void)name_len;
    STUB();
}

int SysdepImpl<GetPgid>::operator()(pid_t pid, pid_t *pgid) {
    (void)pid; (void)pgid;
    STUB();
}

pid_t SysdepImpl<GetPpid>::operator()() {
    STUB();
}

int SysdepImpl<GetResgid>::operator()(gid_t *rgid, gid_t *egid, gid_t *sgid) {
    (void)rgid; (void)egid; (void)sgid;
    STUB();
}

int SysdepImpl<GetResuid>::operator()(uid_t *ruid, uid_t *euid, uid_t *suid) {
    (void)ruid; (void)euid; (void)suid;
    STUB();
}

int SysdepImpl<GetSid>::operator()(pid_t pid, pid_t *sid) {
    (void)pid; (void)sid;
    STUB();
}

pid_t SysdepImpl<GetTid>::operator()() {
    STUB();
}

uid_t SysdepImpl<GetUid>::operator()() {
    STUB();
}

int SysdepImpl<Ioctl>::operator()(int fd, unsigned long request, void *arg, int *result) {
    (void)fd; (void)request; (void)arg; (void)result;
    STUB();
}

int SysdepImpl<Link>::operator()(const char *old_path, const char *new_path) {
    (void)old_path; (void)new_path;
    STUB();
}

int SysdepImpl<Linkat>::operator()(int olddirfd, const char *old_path, int newdirfd, const char *new_path, int flags) {
    (void)olddirfd; (void)old_path; (void)newdirfd; (void)new_path; (void)flags;
    STUB();
}

int SysdepImpl<Mkdir>::operator()(const char *path, mode_t mode) {
    if (mode != 0777)
        mlibc::panicLogger() << "mkdir called with mode != O777, this is not supported yet\n" << frg::endlog;


    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(10), "D"(path));

    if (ret)
        return 0;

    return EIO;
}

int SysdepImpl<Mkdirat>::operator()(int dirfd, const char *path, mode_t mode) {
    (void)dirfd; (void)path; (void)mode;
    STUB();
}

int SysdepImpl<Mkfifoat>::operator()(int dirfd, const char *path, mode_t mode) {
    (void)dirfd; (void)path; (void)mode;
    STUB();
}

int SysdepImpl<Mknodat>::operator()(int dirfd, const char *path, int mode, int dev) {
    (void)dirfd; (void)path; (void)mode; (void)dev;
    STUB();
}

int SysdepImpl<Nice>::operator()(int nice, int *new_nice) {
    (void)nice; (void)new_nice;
    STUB();
}

int SysdepImpl<Openat>::operator()(int dirfd, const char *path, int flags, mode_t mode, int *fd) {
    (void)dirfd; (void)path; (void)flags; (void)mode; (void)fd;
    STUB();
}

int SysdepImpl<Pause>::operator()() {
    STUB();
}

int SysdepImpl<Pipe>::operator()(int *fds, int flags) {
    if (flags)
        mlibc::panicLogger() << "pipe2 not supported yet\n" << frg::endlog;

    int ret;

    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(41), "D"(fds));

    if (ret >= 0)
        return ret;

    return -ret;
}

int SysdepImpl<Renameat>::operator()(int olddirfd, const char *old_path, int newdirfd, const char *new_path) {
    (void)olddirfd; (void)old_path; (void)newdirfd; (void)new_path;
    STUB();
}

int SysdepImpl<SetEgid>::operator()(gid_t egid) {
    (void)egid;
    STUB();
}

int SysdepImpl<SetEuid>::operator()(uid_t euid) {
    (void)euid;
    STUB();
}

int SysdepImpl<SetGid>::operator()(gid_t gid) {
    (void)gid;
    STUB();
}

int SysdepImpl<SetHostname>::operator()(const char *buffer, size_t bufsize) {
    (void)buffer; (void)bufsize;
    STUB();
}

int SysdepImpl<SetItimer>::operator()(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    (void)which; (void)new_value; (void)old_value;
    STUB();
}

int SysdepImpl<SetPgid>::operator()(pid_t pid, pid_t pgid) {
    (void)pid; (void)pgid;
    STUB();
}

int SysdepImpl<SetRegid>::operator()(gid_t rgid, gid_t egid) {
    (void)rgid; (void)egid;
    STUB();
}

int SysdepImpl<SetResgid>::operator()(gid_t rgid, gid_t egid, gid_t sgid) {
    (void)rgid; (void)egid; (void)sgid;
    STUB();
}

int SysdepImpl<SetResuid>::operator()(uid_t ruid, uid_t euid, uid_t suid) {
    (void)ruid; (void)euid; (void)suid;
    STUB();
}

int SysdepImpl<SetReuid>::operator()(uid_t ruid, uid_t euid) {
    (void)ruid; (void)euid;
    STUB();
}

int SysdepImpl<SetSid>::operator()(pid_t *sid) {
    (void)sid;
    STUB();
}

int SysdepImpl<SetUid>::operator()(uid_t uid) {
    (void)uid;
    STUB();
}

int SysdepImpl<Sleep>::operator()(time_t *secs, long *nanos) {
    (void)secs; (void)nanos;
    STUB();
}

int SysdepImpl<Symlink>::operator()(const char *target_path, const char *link_path) {
    (void)target_path; (void)link_path;
    STUB();
}

int SysdepImpl<Symlinkat>::operator()(const char *target_path, int dirfd, const char *link_path) {
    (void)target_path; (void)dirfd; (void)link_path;
    STUB();
}

void SysdepImpl<Sync>::operator()() {
    STUB();
}

int SysdepImpl<Sysconf>::operator()(int num, long *ret) {
    (void)num; (void)ret;
    STUB();
}

int SysdepImpl<Tcdrain>::operator()(int fd) {
    (void)fd;
    STUB();
}

int SysdepImpl<Tcflow>::operator()(int fd, int action) {
    (void)fd; (void)action;
    STUB();
}

int SysdepImpl<Tcflush>::operator()(int fd, int queue) {
    (void)fd; (void)queue;
    STUB();
}

int SysdepImpl<Tcgetattr>::operator()(int fd, struct termios *attr) {
    (void)fd; (void)attr;
    STUB();
}

int SysdepImpl<Tcgetwinsize>::operator()(int fd, struct winsize *winsz) {
    (void)fd; (void)winsz;
    STUB();
}

int SysdepImpl<Tcsendbreak>::operator()(int fd, int dur) {
    (void)fd; (void)dur;
    STUB();
}

int SysdepImpl<Tcsetattr>::operator()(int fd, int optional_actions, const struct termios *attr) {
    (void)fd; (void)optional_actions; (void)attr;
    STUB();
}

int SysdepImpl<Tcsetwinsize>::operator()(int fd, const struct winsize *winsz) {
    (void)fd; (void)winsz;
    STUB();
}

int SysdepImpl<TimerCreate>::operator()(clockid_t clk, struct sigevent *__restrict evp, timer_t *__restrict res) {
    (void)clk; (void)evp; (void)res;
    STUB();
}

int SysdepImpl<TimerDelete>::operator()(timer_t t) {
    (void)t;
    STUB();
}

int SysdepImpl<TimerGettime>::operator()(timer_t t, struct itimerspec *val) {
    (void)t; (void)val;
    STUB();
}

int SysdepImpl<TimerSettime>::operator()(timer_t t, int flags, const struct itimerspec *__restrict val, struct itimerspec *__restrict old) {
    (void)t; (void)flags; (void)val; (void)old;
    STUB();
}

int SysdepImpl<Truncate>::operator()(const char *path, off_t length) {
    (void)path; (void)length;
    STUB();
}

int SysdepImpl<Ttyname>::operator()(int fd, char *buf, size_t size) {
    (void)fd; (void)buf; (void)size;
    STUB();
}

int SysdepImpl<Umask>::operator()(mode_t mode, mode_t *old) {
    (void)mode; (void)old;
    STUB();
}

int SysdepImpl<Utimensat>::operator()(int dirfd, const char *pathname, const struct timespec times[2], int flags) {
    (void)dirfd; (void)pathname; (void)times; (void)flags;
    STUB();
}

int Sysdeps<Pread>::operator()(int fd, void *buf, size_t n, off_t off, ssize_t *bytes_read) {
    if (off)
        mlibc::panicLogger() << "pread called with non-null offset, this is not supported yet\n" << frg::endlog;

    return Sysdeps<Read>{}(fd, buf, n, bytes_read);
}

int Sysdeps<Pwrite>::operator()(int fd, const void *buf, size_t n, off_t off, ssize_t *bytes_written) {
    if (off)
        mlibc::panicLogger() << "pwrite called with non-null offset, this is not supported yet\n" << frg::endlog;

    return Sysdeps<Write>{}(fd, buf, n, bytes_written);
}

int SysdepImpl<Readlinkat>::operator()(int, char const*, void*, unsigned long, int*)
{
    STUB();
}

int SysdepImpl<Readlink>::operator()(char const*, void*, unsigned long, int*)
{
    STUB();
}

int SysdepImpl<Waitid>::operator()(idtype_t, unsigned int, siginfo_t*, int)
{
    STUB();
}

int SysdepImpl<ClockGetres>::operator()(int, long*, long*)
{
    STUB();
}
} // namespace mlibc
