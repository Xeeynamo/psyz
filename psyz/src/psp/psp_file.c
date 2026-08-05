// Native PSP file I/O backend, avoids newlib shim
#include <pspiofilemgr.h>
#include <pspiofilemgr_stat.h>
#include <pspiofilemgr_fcntl.h>
#include <psyz.h>
#include <psyz/log.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <romio.h>

typedef struct {
    char name[256];
    int size;
} FileEntry;

typedef struct {
    char base_dir[1024];
    FileEntry* entries;
    int entry_count;
    int current_index;
} DirState;
static DirState g_dir = {0};

static int compare_entries(const void* a, const void* b) {
    return strcmp(((const FileEntry*)a)->name, ((const FileEntry*)b)->name);
}

static bool is_dir_open(void) { return g_dir.entries != NULL; }

static void close_dir(void) {
    if (is_dir_open()) {
        free(g_dir.entries);
        g_dir.entries = NULL;
        g_dir.entry_count = 0;
        g_dir.current_index = 0;
        g_dir.base_dir[0] = '\0';
    }
}

// Read every regular file in basePath into g_dir, sorted by name. Returns the
// number of entries, or a negative value if an error occurs.
static int open_dir(const char* basePath) {
    if (is_dir_open()) {
        WARNF("previous firstfile at '%s' was not closed", g_dir.base_dir);
        close_dir();
    }

    SceUID dfd = sceIoDopen(basePath);
    if (dfd < 0) {
        return -1;
    }

    int capacity = 32;
    int count = 0;
    FileEntry* entries = malloc(sizeof(FileEntry) * capacity);
    if (!entries) {
        sceIoDclose(dfd);
        return -1;
    }

    SceIoDirent dirent;
    int res;
    // sceIoDread: >0 while an entry was read, 0 at the end, <0 on error
    while ((res = (memset(&dirent, 0, sizeof(dirent)),
                   sceIoDread(dfd, &dirent))) > 0) {
        if (dirent.d_name[0] == '.') {
            continue; // skip ".", ".." and hidden entries
        }
        if (!FIO_S_ISREG(dirent.d_stat.st_mode)) {
            continue; // directories and other special nodes are not files
        }
        if (count == capacity) {
            capacity *= 2;
            FileEntry* grown = realloc(entries, sizeof(FileEntry) * capacity);
            if (!grown) {
                free(entries);
                sceIoDclose(dfd);
                return -1;
            }
            entries = grown;
        }
        strncpy(entries[count].name, dirent.d_name,
                sizeof(entries[count].name) - 1);
        entries[count].name[sizeof(entries[count].name) - 1] = '\0';
        entries[count].size = (int)dirent.d_stat.st_size;
        count++;
    }
    sceIoDclose(dfd);

    if (res < 0) {
        free(entries);
        return -1;
    }
    if (count == 0) {
        free(entries);
        return 0;
    }

    qsort(entries, count, sizeof(FileEntry), compare_entries);

    strncpy(g_dir.base_dir, basePath, sizeof(g_dir.base_dir) - 1);
    g_dir.base_dir[sizeof(g_dir.base_dir) - 1] = '\0';
    g_dir.entries = entries;
    g_dir.entry_count = count;
    g_dir.current_index = 0;
    return count;
}

static void populate_entry(struct DIRENTRY* dst, const FileEntry* src) {
    // TODO: names longer than 20 characters are not supported
    if (strlen(src->name) >= sizeof(dst->name) - 1) {
        WARNF("dir name '%s' will be truncated", src->name);
    }
    strncpy(dst->name, src->name, sizeof(dst->name) - 1);
    dst->name[sizeof(dst->name) - 1] = '\0';
    dst->attr = 0x10 | 0x40; // matches the desktop backend's attribute bits
    dst->size = src->size;
    dst->next = NULL;
    dst->system[0] = 0;
}

struct DIRENTRY* my_firstfile(char* dirPath, struct DIRENTRY* firstEntry) {
    char basePath[0x100];
    Psyz_AdjustPath(basePath, dirPath, sizeof(basePath));
    DEBUGF("sceIoDopen('%s')", basePath);
    if (open_dir(basePath) <= 0) {
        return NULL;
    }
    populate_entry(firstEntry, &g_dir.entries[g_dir.current_index++]);
    return firstEntry;
}

struct DIRENTRY* my_nextfile(struct DIRENTRY* outEntry) {
    if (!outEntry || !is_dir_open()) {
        return NULL;
    }
    if (g_dir.current_index >= g_dir.entry_count) {
        close_dir();
        return NULL;
    }
    populate_entry(outEntry, &g_dir.entries[g_dir.current_index++]);
    return outEntry;
}

long my_format(char* fs) {
    char path[0x200];
    Psyz_AdjustPath(path, fs, sizeof(path));
    DEBUGF("format('%s')", path);

    SceUID dfd = sceIoDopen(path);
    if (dfd < 0) {
        ERRORF("failed to open directory '%s'", path);
        return 0;
    }
    size_t path_end = strlen(path);
    // ensure a trailing separator so file names can be appended directly
    if (path_end > 0 && path[path_end - 1] != '/' &&
        path_end + 1 < sizeof(path)) {
        path[path_end++] = '/';
        path[path_end] = '\0';
    }

    long ok = 1;
    SceIoDirent dirent;
    while (sceIoDread(dfd, (memset(&dirent, 0, sizeof(dirent)), &dirent)) > 0) {
        if (!FIO_S_ISREG(dirent.d_stat.st_mode)) {
            continue;
        }
        strncpy(path + path_end, dirent.d_name, sizeof(path) - path_end - 1);
        path[sizeof(path) - 1] = '\0';
        if (sceIoRemove(path) < 0) {
            ok = 0;
            break;
        }
    }
    sceIoDclose(dfd);
    return ok;
}

long my_erase(char* path) {
    char adjPath[0x100];
    Psyz_AdjustPath(adjPath, path, sizeof(adjPath));
    DEBUGF("sceIoRemove('%s')", adjPath);
    return sceIoRemove(adjPath) >= 0;
}

int psyz_open(const char* devname, int flag) {
    int oflag;
    if ((flag & (FREAD | FWRITE)) == (FREAD | FWRITE)) {
        oflag = PSP_O_RDWR;
    } else if (flag & FWRITE) {
        oflag = PSP_O_WRONLY;
    } else if (flag & FCREAT) {
        // FCREAT without an explicit access mode means "create for writing",
        // matching the unix backend which maps this case to creat()
        oflag = PSP_O_WRONLY;
    } else {
        oflag = PSP_O_RDONLY;
    }
    if (flag & FAPPEND) {
        oflag |= PSP_O_APPEND;
    }
    if (flag & FCREAT) {
        // creat() implies truncation; mirror that so a re-created file starts
        // empty rather than keeping stale trailing bytes
        oflag |= PSP_O_CREAT | PSP_O_TRUNC;
    }
    if (flag & FTRUNC) {
        oflag |= PSP_O_TRUNC;
    }
    // flags PSY-Z does not model on this platform
    if (flag & FNBLOCK) {
        DEBUGF("FNBLOCK ignored for %s", devname);
    }
    if (flag & (FRLOCK | FWLOCK)) {
        DEBUGF("file locking ignored for %s", devname);
    }
    if (flag & FASYNC) {
        DEBUGF("FASYNC ignored for %s", devname);
    }

    char path[0x100];
    Psyz_AdjustPath(path, devname, sizeof(path));

    if (!(oflag & PSP_O_CREAT)) {
        // reject anything that is not an existing regular file, so callers get
        // the same "not found" behaviour as the unix backend
        SceIoStat st;
        if (sceIoGetstat(path, &st) < 0) {
            WARNF("path '%s' mapped from '%s' not found", path, devname);
            return -1;
        }
        if (!FIO_S_ISREG(st.st_mode)) {
            WARNF("path '%s' mapped from '%s' is not a regular file", path,
                  devname);
            return -1;
        }
    }

    SceUID fd = sceIoOpen(path, oflag, 0777);
    if (fd < 0) {
        WARNF("failed to open '%s' (mapped from '%s'): 0x%08x", path, devname,
              (unsigned int)fd);
        return -1;
    }
    return (int)fd;
}

int psyz_close(int fd) { return sceIoClose((SceUID)fd); }

long psyz_lseek(long fd, long offset, long flag) {
    return (long)sceIoLseek((SceUID)fd, (SceOff)offset, (int)flag);
}

long psyz_read(long fd, void* buf, long n) {
    return (long)sceIoRead((SceUID)fd, buf, (SceSize)n);
}

long psyz_write(long fd, void* buf, long n) {
    return (long)sceIoWrite((SceUID)fd, buf, (SceSize)n);
}

long psyz_ioctl(long fd, long com, long arg) {
    (void)fd;
    (void)com;
    (void)arg;
    // do games even use this?!
    NOT_IMPLEMENTED;
    return -1;
}
