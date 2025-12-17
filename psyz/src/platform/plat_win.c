#include <psyz.h>
#include <log.h>
#include <string.h>
#include <kernel.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <direct.h>

static void _adjust_path(char* dst, const char* src, int maxlen) {
    size_t len = strnlen(src, maxlen);
    if (len >= 5 && src[0] == 'b' && src[1] == 'u' && src[4] == ':') {
        // adjust memory card path
        strncpy(dst, src, maxlen);
        dst[4] = '\0';
        _mkdir(dst);  // Windows mkdir only takes one argument
        dst[4] = '\\';  // Use Windows path separator
        if (dst[5] == '\0' || dst[5] == '*') { // handles 'bu00:*'
            dst[5] = '\0';
        }
        return;
    } else {
        strncpy(dst, src, maxlen);
        dst[maxlen - 1] = '\0';
    }
}

struct DIRENTRY* my_firstfile(char* dirPath, struct DIRENTRY* firstEntry) {
    NOT_IMPLEMENTED;
    return NULL;
}

struct DIRENTRY* my_nextfile(struct DIRENTRY* outEntry) {
    NOT_IMPLEMENTED;
    return NULL;
}

long my_erase(char* path) {
    char adjPath[0x100];
    _adjust_path(adjPath, path, sizeof(adjPath));

    DEBUGF("remove('%s')", adjPath);
    return remove(adjPath) == 0;
}

long my_format(char* fs) {
    NOT_IMPLEMENTED;
    return 0;
}

int my_open(const char* devname, int flag) {
    // Map PS1 flags to Windows flags
    int oflag = flag & (_O_WRONLY | _O_RDWR | _O_CREAT);
    char path[0x100];
    _adjust_path(path, devname, sizeof(path));

    if (oflag & _O_CREAT) {
        return _creat(path, _S_IREAD | _S_IWRITE);
    } else {
        struct _stat st;
        if (_stat(path, &st) != 0) {
            WARNF("path '%s' mapped from '%s' not found", path, devname);
            return -1;
        }
        if (!(st.st_mode & _S_IFREG)) {
            WARNF("path '%s' mapped from '%s' is not a regular file", path, devname);
            return -1;
        }
        return _open(path, oflag);
    }
}

int my_close(int fd) {
    return _close(fd);
}

long my_lseek(long fd, long offset, long flag) {
    return _lseek((int)fd, offset, (int)flag);
}

long my_read(long fd, void* buf, long n) {
    return _read((int)fd, buf, (unsigned int)n);
}

long my_write(long fd, void* buf, long n) {
    return _write((int)fd, buf, (unsigned int)n);
}

long my_ioctl(long fd, long com, long arg) {
    NOT_IMPLEMENTED;
    return -1;
}
