#include <psyz.h>
#include <log.h>
#include <string.h>
#include <kernel.h>

// Undefine macros before including Windows headers to avoid conflicts
#undef open
#undef close
#undef lseek
#undef read
#undef write
#undef ioctl

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <direct.h>
#include <stdlib.h>
#include <share.h>

static void _adjust_path(char* dst, const char* src, int maxlen) {
    size_t len = strnlen(src, maxlen);
    if (len >= 5 && src[0] == 'b' && src[1] == 'u' && src[4] == ':') {
        // adjust memory card path
        strncpy_s(dst, maxlen, src, _TRUNCATE);
        dst[4] = '\0';
        _mkdir(dst);
        dst[4] = '\\';
        if (dst[5] == '\0' || dst[5] == '*') { // handles 'bu00:*'
            dst[5] = '\0';
        }
        return;
    } else {
        strncpy_s(dst, maxlen, src, _TRUNCATE);
    }
}

static char* path_join(char* left, const char* right, int maxlen) {
    size_t left_len = strlen(left);
    if (left_len >= maxlen - 1) {
        return NULL;
    }
    if (left[left_len - 1] != '\\' && right[0] != '\\') {
        if (left_len < maxlen - 1) {
            left[left_len] = '\\';
            left[left_len + 1] = '\0';
            left_len++;
        } else {
            return NULL;
        }
    } else if (left[left_len - 1] == '\\' && right[0] == '\\') {
        // Avoid double backslash
        left[left_len - 1] = '\0';
        left_len--;
    }
    strncpy_s(left + left_len, maxlen - left_len, right, _TRUNCATE);
    return left;
}

typedef struct {
    char** filenames;
    int file_count;
    int current_index;
    char base_dir[1024];
} WIN_DIRENTRY_RESERVED;

static WIN_DIRENTRY_RESERVED singleton_dir = {0};

static int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static void close_filesearch_handle() {
    if (singleton_dir.filenames) {
        for (int i = 0; i < singleton_dir.file_count; i++) {
            free(singleton_dir.filenames[i]);
        }
        free(singleton_dir.filenames);
        singleton_dir.filenames = NULL;
        singleton_dir.file_count = 0;
        singleton_dir.current_index = 0;
        singleton_dir.base_dir[0] = '\0';
    }
}

static int open_filesearch_handle(const char* basePath) {
    close_filesearch_handle();

    strncpy_s(singleton_dir.base_dir, sizeof(singleton_dir.base_dir), basePath,
              _TRUNCATE);

    // Build search pattern
    char search_pattern[1024];
    strncpy_s(search_pattern, sizeof(search_pattern), basePath,
              sizeof(search_pattern) - 3);

    // Ensure path ends with backslash
    size_t len = strlen(search_pattern);
    if (len > 0 && search_pattern[len - 1] != '\\') {
        strcat_s(search_pattern, sizeof(search_pattern), "\\");
    }
    strcat_s(search_pattern, sizeof(search_pattern), "*");

    struct _finddata_t file_info;
    intptr_t handle = _findfirst(search_pattern, &file_info);

    if (handle == -1) {
        return 0;
    }

    // Count and collect all regular files
    int capacity = 16;
    singleton_dir.filenames = (char**)malloc(capacity * sizeof(char*));
    singleton_dir.file_count = 0;

    do {
        // skip directories: ps1 memcards are flat
        // skip hidden files starting with `.`
        if (!(file_info.attrib & _A_SUBDIR) && file_info.name[0] != '.') {
            if (singleton_dir.file_count >= capacity) {
                capacity *= 2;
                singleton_dir.filenames = (char**)realloc(
                    singleton_dir.filenames, capacity * sizeof(char*));
            }
            singleton_dir.filenames[singleton_dir.file_count] =
                _strdup(file_info.name);
            singleton_dir.file_count++;
        }
    } while (!_findnext(handle, &file_info));

    _findclose(handle);

    if (singleton_dir.file_count == 0) {
        free(singleton_dir.filenames);
        singleton_dir.filenames = NULL;
        return 0;
    }

    // ensure directory entries are stored alphabetically
    qsort(singleton_dir.filenames, singleton_dir.file_count, sizeof(char*),
          compare_strings);

    singleton_dir.current_index = 0;
    return 1;
}

static void populate_entry(
    const char* baseDir, struct DIRENTRY* dst, const char* filename) {
    char buf[512];
    struct _stat fileStat = {0};

    strncpy_s(buf, sizeof(buf), baseDir, _TRUNCATE);
    if (!path_join(buf, filename, sizeof(buf))) {
        ERRORF("failed to join '%s' and '%s': strings are too large", baseDir,
               filename);
        return;
    }

    if (_stat(buf, &fileStat) != 0) {
        ERRORF("failed to stat '%s'", buf);
        return;
    }

    // ensure max 20 characters per name entry
    if (strlen(filename) >= sizeof(dst->name)) {
        WARNF("'%s' will be truncated", filename);
    }
    strncpy_s(dst->name, sizeof(dst->name), filename, _TRUNCATE);
    dst->attr = 0x10 | 0x40; // Same as Unix version
    dst->size = (long)fileStat.st_size;
    dst->next = NULL;
    dst->system[0] = 0;
}

struct DIRENTRY* my_firstfile(char* dirPath, struct DIRENTRY* firstEntry) {
    char basePath[0x100];
    _adjust_path(basePath, dirPath, sizeof(basePath));
    DEBUGF("opendir('%s')", basePath);

    if (!open_filesearch_handle(basePath)) {
        return NULL;
    }
    if (singleton_dir.current_index >= singleton_dir.file_count) {
        return NULL;
    }
    populate_entry(singleton_dir.base_dir, firstEntry,
                   singleton_dir.filenames[singleton_dir.current_index++]);
    return firstEntry;
}

struct DIRENTRY* my_nextfile(struct DIRENTRY* outEntry) {
    if (!outEntry || !singleton_dir.filenames) {
        return NULL;
    }
    if (singleton_dir.current_index >= singleton_dir.file_count) {
        close_filesearch_handle();
        return NULL;
    }
    populate_entry(singleton_dir.base_dir, outEntry,
                   singleton_dir.filenames[singleton_dir.current_index++]);
    return outEntry;
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

int psyz_open(const char* devname, int flag, ...) {
    // Map PS1 flags to Windows flags
    int oflag = flag & (_O_WRONLY | _O_RDWR | _O_CREAT);
    char path[0x100];
    _adjust_path(path, devname, sizeof(path));

    if (oflag & _O_CREAT) {
        int fd;
        errno_t err = _sopen_s(
            &fd, path, _O_CREAT | _O_WRONLY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
        return (err == 0) ? fd : -1;
    } else {
        struct _stat st;
        if (_stat(path, &st) != 0) {
            WARNF("path '%s' mapped from '%s' not found", path, devname);
            return -1;
        }
        if (!(st.st_mode & _S_IFREG)) {
            WARNF("path '%s' mapped from '%s' is not a regular file", path,
                  devname);
            return -1;
        }
        int fd;
        errno_t err = _sopen_s(&fd, path, oflag, _SH_DENYNO, 0);
        return (err == 0) ? fd : -1;
    }
}

int psyz_close(int fd) { return _close(fd); }

long psyz_lseek(int fd, long offset, int flag) {
    return _lseek(fd, offset, flag);
}

int psyz_read(int fd, void* buf, unsigned int n) {
    return _read(fd, buf, n);
}

int psyz_write(int fd, const void* buf, unsigned int n) {
    return _write(fd, buf, n);
}

long psyz_ioctl(long fd, long com, long arg) {
    NOT_IMPLEMENTED;
    return -1;
}
