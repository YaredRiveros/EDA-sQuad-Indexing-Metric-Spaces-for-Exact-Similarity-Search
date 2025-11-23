#include "DSACLX.h"
#include <cstring>   // memcpy, memset
#include <cerrno>

#ifdef _WIN32
  #include <io.h>
  #include <fcntl.h>
#else
  #include <unistd.h>   // ::read, ::write, ::lseek, ::close
  #include <fcntl.h>    // ::open, O_*
#endif

extern double IOread;
extern double IOwrite;

namespace {

// Lee exactamente 'nbytes' en 'buf' (o hasta error/EOF). Devuelve bytes leídos.
static int readFully(int fd, char* buf, int nbytes) {
    int total = 0;
    while (total < nbytes) {
    #ifdef _WIN32
        int r = _read(fd, buf + total, nbytes - total);
    #else
        int r = ::read(fd, buf + total, nbytes - total);
    #endif
        if (r > 0) {
            total += r;
        } else if (r == 0) {
            // EOF antes de completar
            break;
        } else {
        #ifndef _WIN32
            if (errno == EINTR) continue;
        #endif
            break; // error
        }
    }
    return total;
}

// Escribe exactamente 'nbytes' desde 'buf'. Devuelve bytes escritos.
static int writeFully(int fd, const char* buf, int nbytes) {
    int total = 0;
    while (total < nbytes) {
    #ifdef _WIN32
        int w = _write(fd, buf + total, nbytes - total);
    #else
        int w = ::write(fd, buf + total, nbytes - total);
    #endif
        if (w > 0) {
            total += w;
        } else if (w == 0) {
            break; // inusual: write 0
        } else {
        #ifndef _WIN32
            if (errno == EINTR) continue;
        #endif
            break; // error
        }
    }
    return total;
}

} // namespace

void DSACLfile::create(const char *filename, Objvector root) {
    if (isOpened()) return;

#ifdef _WIN32
    fileHandle = _open(filename, O_RDWR | O_BINARY);
    if (fileHandle < 0) {
        fileHandle = _open(filename, O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
                           _S_IREAD | _S_IWRITE);
    }
#else
    fileHandle = ::open(filename, O_RDWR);
    if (fileHandle < 0) {
        fileHandle = ::open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    }
#endif
    if (fileHandle < 0) return;

    setOpen(true);
    pageNum = 0;

    DSACLpage *page = allocate();
    ClusterSize = (page->pageSize() - 5 * sizeof(double) - 2 * sizeof(int) - sizeOfObjvector())
                  / sizeOfObjentry();

    Node *node = new Node(root);
    page->setNode(node);

    writePage(page);
}

DSACLpage * DSACLfile::allocate() {
    DSACLpage *page = new DSACLpage(pageNum);
    pageNum++;
    return page;
}

void DSACLfile::open(const char *filename) {
    if (isOpened()) return;
#ifdef _WIN32
    fileHandle = _open(filename, O_RDWR | O_BINARY);
#else
    fileHandle = ::open(filename, O_RDWR);
#endif
    if (fileHandle < 0) return;
    setOpen(true);
}

void DSACLfile::close() {
    if (!isOpened()) return;
#ifdef _WIN32
    _close(fileHandle);
#else
    ::close(fileHandle);
#endif
    setOpen(false);
}

bool DSACLfile::isOpened() { return isOpen; }
void DSACLfile::setOpen(bool state) { isOpen = state; }

void DSACLfile::read(double pageNo, int size, char *buf) {
    if (!isOpened()) return;
#ifdef _WIN32
    _lseeki64(fileHandle, static_cast<__int64>(pageNo * BlockSize), SEEK_SET);
#else
    {
        off_t off = static_cast<off_t>(pageNo) * BlockSize;
        ::lseek(fileHandle, off, SEEK_SET);
    }
#endif
    (void)readFully(fileHandle, buf, size);
    IOread++;
}

void DSACLfile::write(double pageNo, int size, const char *buf) {
    if (!isOpened()) return;
#ifdef _WIN32
    _lseeki64(fileHandle, static_cast<__int64>(pageNo * BlockSize), SEEK_SET);
#else
    {
        off_t off = static_cast<off_t>(pageNo) * BlockSize;
        ::lseek(fileHandle, off, SEEK_SET);
    }
#endif
    (void)writeFully(fileHandle, buf, size);
    IOwrite++;
}
