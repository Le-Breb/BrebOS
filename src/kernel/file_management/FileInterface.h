#ifndef BREBOS_FILEINTERFACE_H
#define BREBOS_FILEINTERFACE_H
#include "dentry.h"
#include "kstddef.h"
#include "../utils/shared_pointer.h"
#include "../core/memory.h"
#include "../utils/circular_buffer.h"

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define TTY_DEV 25 // fstat st_dev for TTYs (set arbitrarily, this is what I get when fstating stdout on my machine)
#define PIPE_DEV 14 // fstat st_dev for pipes (set arbitrarily, this is what I get when fstating a pipe on my machine)

class FileInterface {
public:
    int fd;
    int flags;
    uint offset;
    int rc = 0;

    enum FileType
    {
        File,
        Pipe,
        TTY
    };
    FileType type;

    virtual ~FileInterface() = default;
    explicit FileInterface(int fd, int flags, uint offset, FileType type);
    virtual int read(void* buf, uint count) = 0;
    virtual int lseek(int offset, int whence) = 0;
    virtual int write(void* buf, uint count) = 0;
    virtual int fstat(struct stat* statbuf) = 0;;
};

class File : public FileInterface
{
public:
    SharedPointer<Dentry> dentry;

    File(int fd, int flags, uint offset, const SharedPointer<Dentry>& dentry);
    int read(void* buf, uint count) override;
    int lseek(int offset, int whence) override;
    int write(void* buf, uint count) override;
    int fstat(struct stat* statbuf) override;
};

class TTY : public FileInterface
{
public:
    enum Target
    {
        STDOUT, STDERR
    };
    Target target;

    TTY(int fd, int flags, Target target);
    int read(void* buf, uint count) override;
    int lseek(int offset, int whence) override;
    int write(void* buf, uint count) override;
    int fstat(struct stat* statbuf) override;
};

class Pipe : public FileInterface
{
    static constexpr uint BUF_SIZE_N_PAGES = 16;
    static constexpr uint BUF_SIZE = BUF_SIZE_N_PAGES * PAGE_SIZE;

    enum End {Read, Write};

    SharedPointer<CircularBuffer<char>> buf = nullptr;
    Pipe* other_end = nullptr;
    End end;

    Pipe(int rfd, int wrd, int flags, End end);
    void register_other_end(Pipe* other);
public:
    int read(void* buf, uint count) override;
    int lseek(int offset, int whence) override;
    int write(void* buf, uint count) override;
    int fstat(struct stat* statbuf) override;

    ~Pipe() override;

    static void create_pipe(int rfd, int wrd, int flags, Pipe* pipes[]);
};


#endif //BREBOS_FILEINTERFACE_H
