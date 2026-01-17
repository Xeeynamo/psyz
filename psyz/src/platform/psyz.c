#include <psyz.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define PATH_SEP '\\'
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#define PATH_SEP '/'
#endif

static int (*adjust_path_cb)(char* dst, const char* src, int maxlen) = NULL;

static void default_adjust_path(char* dst, const char* src, int maxlen) {
    size_t len = strlen(src);
    if (len >= 5 && src[0] == 'b' && src[1] == 'u' && src[4] == ':') {
        // adjust memory card path (bu00:, bu10:, etc.)
        strncpy(dst, src, maxlen);
        dst[maxlen - 1] = '\0';
        if (maxlen > 4) {
            dst[4] = '\0';
#ifdef _WIN32
            mkdir(dst, 0);
#else
            struct stat st = {0};
            if (stat(dst, &st) == -1) {
                mkdir(dst, 0755);
            }
#endif
            dst[4] = PATH_SEP;
            if (dst[5] == '\0' || dst[5] == '*') { // handles 'bu00:*'
                dst[5] = '\0';
            }
        }
    } else {
        strncpy(dst, src, maxlen);
        dst[maxlen - 1] = '\0';
    }
}

static void truncate_filename(char* path) {
    // find the last path separator, then truncate it
    // memcard save names cannot exceed 20 characters (19 + null terminator)
    const int max_filename_len = 20;
    char* filename = path;
    char* p = path;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
        p++;
    }

    // truncate if filename exceeds max length
    size_t filename_len = strlen(filename);
    if (filename_len >= max_filename_len) {
        filename[max_filename_len - 1] = '\0';
    }
}

void Psyz_AdjustPathCB(
    int (*callback)(char* dst, const char* src, int maxlen)) {
    adjust_path_cb = callback;
}

void Psyz_AdjustPath(char* dst, const char* src, int maxlen) {
    if (!adjust_path_cb || adjust_path_cb(dst, src, maxlen) < 0) {
        default_adjust_path(dst, src, maxlen);
    }
    truncate_filename(dst);
}

char* Psyz_JoinPath(char* left, const char* right, int maxlen) {
    size_t left_len = strlen(left);
    if (left_len >= maxlen - 1) {
        return NULL;
    }
    if (left[left_len - 1] != PATH_SEP && right[0] != PATH_SEP) {
        if (left_len < maxlen - 1) {
            left[left_len] = PATH_SEP;
            left[left_len + 1] = '\0';
            left_len++;
        } else {
            return NULL;
        }
    } else if (left[left_len - 1] == PATH_SEP && right[0] == PATH_SEP) {
        // Avoid double path separator
        left[left_len - 1] = '\0';
        left_len--;
    }
#ifdef _WIN32
    strncpy_s(left + left_len, maxlen - left_len, right, _TRUNCATE);
#else
    strncpy(left + left_len, right, maxlen - left_len - 1);
    left[maxlen - 1] = '\0';
#endif
    return left;
}
