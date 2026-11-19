#include "FileInterface.h"

#include "kstring.h"
#include "superblock.h"
#include "../utils/comparison.h"
#include <errno.h>
#include "../core/fb.h"
#include <fcntl.h>
#include "../core/memory/memory.h"

FileInterface::FileInterface(int fd, int flags, uint offset, FileType type) : fd(fd), flags(flags), offset(offset), type(type)
{
}

File::File(int fd, int flags, uint offset, const SharedPointer<Inode>& inode) : FileInterface(fd, flags, offset, FileType::File), inode(inode)
{

}

int File::read(void* buf, uint count)
{
    if (inode->type != Inode::File)
        return -EINVAL; // Not a regular file
    if (offset >= inode->size)
        return 0; // EOF
    count = min(count, inode->size - offset);

    // Read through the page cache, which correctly handles the fact that buf may be lazily allocated, which DMA isn't fond of
    if (!inode->read_cached(buf, offset, count).warn_is_ok())
        return -EIO; // IO error

    offset += count;

    return (int)count;
}

int File::lseek(int offset, int whence)
{
    // Check if whence is valid
    if (whence < 0 || whence > 2)
        return -EINVAL; // Invalid whence value

    // Compute the new offset based on whence
    int ref = whence == SEEK_SET ? 0 :
        whence == SEEK_CUR ? offset : inode->size;
    int new_offset = ref + offset;

    // Check if the new offset is valid
    if (new_offset < 0)
        return -EINVAL; // Invalid offset
    if ((uint)new_offset > inode->size)
    {
        printf_error("lseek called with an offset which would result in a new offset greater than file size. "
               "This is compliant with the man page, but BrebOS VFS does not support this.");
        return -EINVAL; // Invalid offset
    }

    // Apply the new offset
    this->offset = new_offset;

    return new_offset;
}

int File::write(void* buf, uint count)
{
    if (inode->type != Inode::File)
        return -EINVAL; // Not a regular file

    if (flags & O_APPEND)
        offset = inode->size;
    if (!count)
        return 0;

    // Grow the file if needed
    const uint old_size = inode->size;
    if (offset + count > inode->size)
        if (!inode->superblock->get_fs()->resize(inode.get(), offset + count).warn_is_ok())
            return -EIO; // IO error

    // Write in the page cache: the disk never reads buf directly, and pages are written back by sync
    uint processed_bytes = 0;
    while (processed_bytes < count)
    {
        const uint pos = offset + processed_bytes;
        const uint page_start = PAGE_ALIGN(pos);
        const uint offset_in_page = pos - page_start;
        const uint n = min(PAGE_SIZE - offset_in_page, count - processed_bytes); // Rest of the page, within the request

        // The previous content of an uncached page is only needed if some data of the file before it grew is not
        // overwritten
        const bool keeps_old_data = page_start < old_size &&
                                    (pos > page_start || pos + n < min(old_size, page_start + PAGE_SIZE));
        Memory::cached_page* page = TRY_OR(inode->get_cached_page(pos / PAGE_SIZE, keeps_old_data), nullptr);
        if (!page)
            return -EIO; // IO error

        // Past the old end of file, the page holds zeros or what the disk had there: zero it, so that a gap between the
        // old end of file and the written data reads as zeros
        char* page_addr = (char*)PAGE_ADDR(page->kernel_pte);
        if (page_start + PAGE_SIZE > old_size)
        {
            const uint zero_from = max(old_size, page_start) - page_start;
            memset(page_addr + zero_from, 0, PAGE_SIZE - zero_from);
        }

        memcpy(page_addr + offset_in_page, (char*)buf + processed_bytes, n);
        page->dirty = true;
        processed_bytes += n;
    }

    if (!Memory::dirty_inodes.contains(inode))
        Memory::dirty_inodes.add(inode);

    offset += count;

    return (int)count;
}

int File::fstat(struct stat* statbuf)
{
    statbuf->st_dev = inode->superblock->get_fs()->get_device()->get_dev();
    statbuf->st_ino = inode->id;
    statbuf->st_mode = inode->mode;
    statbuf->st_nlink = inode->nlink;
    statbuf->st_uid = inode->uid;
    statbuf->st_gid = inode->gid;
    statbuf->st_rdev = 0;
    statbuf->st_size = (off_t)inode->size;
    statbuf->st_blksize = inode->superblock->get_fs()->get_block_size();
    statbuf->st_blocks = inode->blocks;
    statbuf->st_atime = inode->atime;
    statbuf->st_mtime = inode->mtime;
    statbuf->st_ctime = inode->ctime;

    return 0;
}

bool File::should_wait_for_data_on_read() const
{
    return false;
}

int File::get_write_fd() const
{
    return -1;
}

int File::get_read_fd() const
{
    return -1;
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

bool TTY::should_wait_for_data_on_read() const
{
    return false; // Not handled for now
}

int TTY::get_write_fd() const
{
    return -1;
}

int TTY::get_read_fd() const
{
    return -1;
}

Pipe::Pipe(int rfd, int wrd, int flags, End end) : FileInterface(end == End::Read ? rfd : wrd, flags, 0, FileType::Pipe),
                                                   end(end)
{
    if (end == Write)
    {
        buf = SharedPointer<CircularBuffer<char>>(new CircularBuffer<char>(BUF_SIZE, (CircularBuffer<char>::mem_allocator)lazy_malloc));
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

    const CircularBuffer<char>::section* sections = this->buf->read(count);
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

bool Pipe::should_wait_for_data_on_read() const
{
    if (end == Write)
        irrecoverable_error("%s: shouldn't be called on pipe write end", __FUNCTION__);
    return other_end;
}

int Pipe::get_write_fd() const
{
    return end == Write ? fd : (other_end ? other_end->fd : -1);
}

int Pipe::get_read_fd() const
{
    return end == Read ? fd : (other_end ? other_end->fd : -1);
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
