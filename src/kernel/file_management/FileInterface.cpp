#include "FileInterface.h"

#include "kstring.h"
#include "superblock.h"
#include "../utils/comparison.h"
#include "sys/errno.h"
#include "../core/fb.h"
#include "sys/_default_fcntl.h"
#include "../core/memory.h"

FileInterface::FileInterface(int fd, int flags, uint offset, FileType type) : fd(fd), flags(flags), offset(offset), type(type)
{
}

File::File(int fd, int flags, uint offset, const SharedPointer<Dentry>& dentry) : FileInterface(fd, flags, offset, FileType::File), dentry(dentry)
{

}

int File::read(void* buf, uint count)
{
    auto l = min(count, dentry->inode->size - offset);
    uint loaded_bytes;

    if (dentry->inode->type != Inode::File)
        return -EINVAL; // Not a regular file

    if (!dentry->inode->superblock->get_fs()->load_file_to_buf(buf, dentry->name, dentry->parent, offset, l,
                                                                        loaded_bytes))
        return -EIO; // IO error

    offset += loaded_bytes;

    return (int)loaded_bytes;
}

int File::lseek(int offset, int whence)
{
    // Check if whence is valid
    if (whence < 0 || whence > 2)
        return -EINVAL; // Invalid whence value

    // Compute the new offset based on whence
    int ref = whence == SEEK_SET ? 0 :
        whence == SEEK_CUR ? offset : dentry->inode->size;
    int new_offset = ref + offset;

    // Check if the new offset is valid
    if (new_offset < 0)
        return -EINVAL; // Invalid offset
    if ((uint)new_offset > dentry->inode->size)
    {
        printf_error("lseek called with an offset which would result in a new offset greater than file size. "
               "This is compliant with the man page, but BrebOS VFS does not support this.");
        return -EINVAL; // Invalid offset
    }

    // Apply the new offset
    offset = new_offset;

    return new_offset;
}

// Todo: optimize that cause reading the whole file then writing it entirely is kinda insanely bad
int File::write(void* buf, uint count)
{
    if (dentry->inode->type != Inode::File)
        return -EINVAL; // Not a regular file

    uint file_size = dentry->inode->size;
    uint loaded_bytes = 0;
    auto file = dentry->inode->superblock->get_fs()->load_file_to_buf(dentry->name, dentry->parent, 0, file_size, loaded_bytes);
    if (!file)
        return -EIO; // IO error

    auto nf = realloc(file, file_size + count);
    if (!nf)
        return -ENOMEM; // Out of memory
    file = nf;

    if (flags & O_APPEND)
        offset = file_size;

    memmove((char*)file + offset + count, (char*)file + offset, file_size - offset);
    memcpy((char*)file + offset, buf, count);

    uint new_file_size = file_size + count;
    bool write_op = dentry->inode->superblock->get_fs()->write_buf_to_file(dentry, file, new_file_size);
    delete (char*)file;
    if (!write_op)
        return -EIO; // IO error

    offset += count;

    return (int)count;
}

int File::fstat(struct stat* statbuf)
{
    auto inode = dentry->inode;
    statbuf->st_dev = inode->superblock->get_fs()->get_dev();
    statbuf->st_ino = inode->id;
    statbuf->st_mode = inode->mode;
    statbuf->st_nlink = inode->nlink;
    statbuf->st_uid = inode->uid;
    statbuf->st_gid = inode->gid;
    statbuf->st_rdev = 0;
    statbuf->st_size = (off_t)inode->size;
    statbuf->st_blksize = dentry->inode->superblock->get_fs()->get_block_size();
    statbuf->st_blocks = inode->blocks;
    statbuf->st_atime = inode->atime;
    statbuf->st_mtime = inode->mtime;
    statbuf->st_ctime = inode->ctime;

    return 0;
}

TTY::TTY(int fd, int flags, Target target) : FileInterface(fd, flags, 0, FileType::TTY), target(target)
{
}

int TTY::read([[maybe_unused]] void* buf, [[maybe_unused]] uint count)
{
    irrecoverable_error("reading from a TTY is not supported yet");
}

int TTY::lseek([[maybe_unused]] int offset, [[maybe_unused]] int whence)
{
    return -ESPIPE; // A TTY cannot be sought
}

int TTY::write(void* buf, uint count)
{
    if (target == STDERR)
        FB::set_fg(FB_RED);
    FB::write((char*)buf, count);
    if (target == STDERR)
        FB::set_fg(FB_WHITE);

    return (int)count;
}

int TTY::fstat([[maybe_unused]] struct stat* statbuf)
{
    statbuf->st_dev = TTY_DEV;
    statbuf->st_ino = -1; // TTYs do not have an inode for now, it may be interesting to investigate why its != from Unix
    statbuf->st_mode = S_IFCHR | 0b110110110; // Character device, read-writable by anybody
    statbuf->st_nlink = 1;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_rdev = 0; // As there's only one terminal to date, it does not matter
    statbuf->st_size = 0;
    statbuf->st_blksize = 0;
    statbuf->st_blocks = 0;
    statbuf->st_atime = 0;
    statbuf->st_mtime = 0;
    statbuf->st_ctime = 0;

    return 0;
}

Pipe::Pipe(int rfd, int wrd, int flags, End end) : FileInterface(end == End::Read ? rfd : wrd, flags, 0, FileType::Pipe),
    end(end)
{
    if (end == Write)
    {
        buf = {new CircularBuffer<char>(BUF_SIZE, (CircularBuffer<char>::mem_allocator)lazy_malloc)};
    }
}

void Pipe::register_other_end(Pipe* other)
{
    if ((end == Read && other->end != Write) || (end == Write && other->end != Read))
        irrecoverable_error("Creating a pipe with wrong type of ends");

    other_end = other;
    if (end == Read)
        buf = other->buf;
}

int Pipe::read(void* buf, uint count)
{
    if (end != Read)
        return -EINVAL; // Unsuitable for reading

    CircularBuffer<char>::section* sections = this->buf->read(count);
    memcpy(buf, sections[0].beg, sections[0].size);
    memcpy((char*)buf + sections[0].size, sections[1].beg, sections[1].size);

    size_t read = sections[0].size + sections[1].size;
    delete sections;

    return (int)read;
}

int Pipe::lseek([[maybe_unused]] int offset, [[maybe_unused]] int whence)
{
    return -ESPIPE; // Pipes cannot be sought
}

int Pipe::write(void* buf, uint count)
{
    if (end == Read)
        return -EINVAL; // Unsuitable for writing
    if (other_end == nullptr)
        return -EPIPE; // Read end is closed

    return (int)this->buf->write((char*)buf, count);
}

int Pipe::fstat(struct stat* statbuf)
{
    statbuf->st_dev = PIPE_DEV;
    statbuf->st_ino = -1; // Pipes do not have an inode for now, it may be interesting to investigate why its != from Unix
    statbuf->st_mode = S_IFIFO | 0b110110110; // Fifo, read-writable by anybody (as it targets the pipe, not the read/write end)
    statbuf->st_nlink = 1;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_rdev = 0;
    statbuf->st_size = 0;
    statbuf->st_blksize = 0;
    statbuf->st_blocks = 0;
    statbuf->st_atime = 0;
    statbuf->st_mtime = 0;
    statbuf->st_ctime = 0;

    return 0;
}

Pipe::~Pipe()
{
    // Register our destruction in other end of the pipe
    if (other_end != nullptr)
        other_end->other_end = nullptr;
}

void Pipe::create_pipe(int rfd, int wrd, int flags, Pipe* pipes[])
{
    Pipe* rp = new Pipe(rfd, wrd, flags, Read);
    Pipe* wp = new Pipe(rfd, wrd, flags, Write);
    rp->register_other_end(wp);
    wp->register_other_end(rp);
    pipes[0] = rp;
    pipes[1] = wp;
}
