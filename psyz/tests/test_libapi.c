#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ztest.h"
#include <psyz.h>
#include <kernel.h>
#include <libapi.h>
#include <libetc.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define rmdir(path) _rmdir(path)
#define PATH_SEP "\\"
#elif defined(__psx__)
// No host filesystem on the console; the groups using it skip on ps1.
#define mkdir(path, mode) (-1)
#define rmdir(path) (-1)
#define PATH_SEP "/"
#else
#include <sys/stat.h>
#define PATH_SEP "/"
#endif

static void bu_make_file(const char* path, int len, int val, int inc) {
    FILE* f = fopen(path, "wb");
    assert(f != NULL);
    unsigned char* d = (unsigned char*)malloc(len);
    assert(d != NULL);
    for (int i = 0; i < len; i++) {
        d[i] = val;
        val += inc;
    }
    assert(fwrite(d, 1, len, f) == (size_t)len);
    assert(fclose(f) == 0);
    free(d);
}

ZTEST_SETUP(bu) {
    zskip_targets("ps1");
    mkdir("bu00", 0755);
    mkdir("bu10", 0755);
    bu_make_file("bu00/BASLUS-00000PSYZ00", 8192, 0, 1);
    bu_make_file("bu00/BASLUS-00000PSYZ01", 8192, 0, 2);
}

ZTEST_TEARDOWN(bu) {
    zskip_targets("ps1");
    assert(remove("bu00/BASLUS-00000PSYZ00") == 0);
    assert(remove("bu00/BASLUS-00000PSYZ01") == 0);
    assert(rmdir("bu00") == 0);
    assert(rmdir("bu10") == 0);
}

ZTEST(bu, for_invalid_path) {
    struct DIRENTRY d = {0};
    struct DIRENTRY* pd;
    pd = firstfile((char*)"none:", &d);
    zexpect_ptr_eq(NULL, pd);
    zexpect_str_eq("", d.name);
    zexpect_s32_eq(0, d.attr);
    zexpect_s32_eq(0, d.size);
}

ZTEST(bu, for_empty_path) {
    struct DIRENTRY d = {0};
    struct DIRENTRY* pd;
    pd = firstfile((char*)"bu10:", &d);
    zexpect_ptr_eq(NULL, pd);
    zexpect_str_eq("", d.name);
    zexpect_s32_eq(0, d.attr);
    zexpect_s32_eq(0, d.size);
}

ZTEST(bu, for_first_file) {
    struct DIRENTRY d = {0};
    struct DIRENTRY* pd;
    pd = firstfile((char*)"bu00:", &d);
    zexpect_ptr_eq(&d, pd);
    zexpect_str_eq("BASLUS-00000PSYZ00", d.name);
    zexpect_s32_eq(80, d.attr);
    zexpect_s32_eq(8192, d.size);
}

ZTEST(bu, for_multiple_files) {
    struct DIRENTRY d[2];
    struct DIRENTRY* pd;
    pd = nextfile(firstfile((char*)"bu00:", d) + 1);
    zexpect_ptr_eq(&d[1], pd);
    zexpect_str_eq("BASLUS-00000PSYZ01", pd->name);
    zexpect_s32_eq(80, pd->attr);
    zexpect_s32_eq(8192, pd->size);
}

ZTEST(bu, for_end_of_file_list) {
    struct DIRENTRY d[3];
    struct DIRENTRY* pd;
    pd = nextfile(nextfile(firstfile((char*)"bu00:", d) + 1) + 2);
    zexpect_ptr_eq(NULL, pd);
}

ZTEST(bu, open_existing_file) {
    int fd = open("bu00:BASLUS-00000PSYZ00", FREAD);
    zexpect_s32_ne(-1, fd);
    close(fd);
}

ZTEST(bu, create_and_write_file) {
    unsigned short expected = 1234, actual;
    int fd = open("bu00:BASLUS-00000PSYZ00", FCREAT);
    zexpect_s32_ne(-1, fd);
    write(fd, (char*)&expected, sizeof(unsigned short));
    close(fd);

    FILE* f = fopen("bu00/BASLUS-00000PSYZ00", "rb");
    int read = fread(&actual, 1, sizeof(unsigned short), f);
    fclose(f);
    zexpect_s32_eq(sizeof(unsigned short), read);
    zexpect_u16_eq(expected, actual);
}

// A save game: the file is created, closed, then reopened to be written.
ZTEST(bu, write_to_existing_file) {
    unsigned short expected = 4321, actual;
    int fd = open("bu00:BASLUS-00000PSYZ01", FWRITE);
    zexpect_s32_ne(-1, fd);
    zexpect_s32_eq(sizeof(unsigned short),
                   write(fd, (char*)&expected, sizeof(unsigned short)));
    close(fd);

    FILE* f = fopen("bu00/BASLUS-00000PSYZ01", "rb");
    int read = fread(&actual, 1, sizeof(unsigned short), f);
    fclose(f);
    zexpect_s32_eq(sizeof(unsigned short), read);
    zexpect_u16_eq(expected, actual);
}

ZTEST(path_adjustment, basic_memory_card_path) {
    char dst[256] = {0};
    Psyz_AdjustPath(dst, "bu00:TESTFILE", sizeof(dst));
    zexpect_str_eq("bu00" PATH_SEP "TESTFILE", dst);
}

ZTEST(path_adjustment, basic_path_no_adjustment) {
    char dst[256] = {0};
    Psyz_AdjustPath(dst, "regular/path/file.txt", sizeof(dst));
    zexpect_str_eq("regular/path/file.txt", dst);
}

static int test_callback_override(char* dst, const char* src, int maxlen) {
    if (strncmp(src, "custom:", 7) == 0) {
        snprintf(dst, maxlen, "overridden/%s", src + 7);
        return (int)strlen(dst);
    }
    return -1;
}

ZTEST(path_adjustment, callback_override_positive_return) {
    char dst[256] = {0};
    Psyz_AdjustPathCB(test_callback_override);
    Psyz_AdjustPath(dst, "custom:myfile", sizeof(dst));
    zexpect_str_eq("overridden/myfile", dst);
    Psyz_AdjustPathCB(NULL);
}

ZTEST(path_adjustment, callback_negative_return_falls_back) {
    char dst[256] = {0};
    Psyz_AdjustPathCB(test_callback_override);
    Psyz_AdjustPath(dst, "bu00:FALLBACK", sizeof(dst));
    zexpect_str_eq("bu00" PATH_SEP "FALLBACK", dst);
    Psyz_AdjustPathCB(NULL);
}

ZTEST(path_adjustment, callback_null_uses_internal) {
    char dst[256] = {0};
    Psyz_AdjustPathCB(NULL);
    Psyz_AdjustPath(dst, "bu10:TESTFILE", sizeof(dst));
    zexpect_str_eq("bu10" PATH_SEP "TESTFILE", dst);
}

ZTEST(path_adjustment, truncates_long_filename_to_19_chars) {
    char dst[256] = {0};

    Psyz_AdjustPath(dst, "bu00:1234567890123456789", sizeof(dst));
    zexpect_str_eq("bu00" PATH_SEP "1234567890123456789", dst);

    Psyz_AdjustPath(dst, "bu00:12345678901234567890", sizeof(dst));
    zexpect_str_eq("bu00" PATH_SEP "1234567890123456789", dst);
}

static int test_callback_with_long_name(
    char* dst, const char* src, int maxlen) {
    if (strncmp(src, "custom:", 7) == 0) {
        snprintf(dst, maxlen, "custom_dir/%s", src + 7);
        return (int)strlen(dst);
    }
    return -1;
}

ZTEST(path_adjustment, callback_result_also_truncated) {
    char dst[256] = {0};
    Psyz_AdjustPathCB(test_callback_with_long_name);
    Psyz_AdjustPath(dst, "custom:VERYLONGFILENAME1234567890", sizeof(dst));
    zexpect_str_eq("custom_dir/VERYLONGFILENAME123", dst);
    Psyz_AdjustPathCB(NULL);
}

#define LONG_FILENAME_21 "BISLUS-00005aaa195120"
#define TRUNCATED_NAME_19 "BISLUS-00005aaa1951"

ZTEST_SETUP(truncation) {
    zskip_targets("ps1");
    mkdir("bu00", 0755);
}

ZTEST_TEARDOWN(truncation) {
    zskip_targets("ps1");
    remove("bu00/BISLUS-00005aaa195120");
    remove("bu00/BISLUS-00005aaa1951");
    remove("bu00/AAAA-SHORT");
    rmdir("bu00");
}

ZTEST(truncation, open_truncates_long_filename) {
    FILE* f = fopen("bu00/BISLUS-00005aaa1951", "wb");
    zassert_ptr_ne(NULL, f);
    fclose(f);

    int fd = open("bu00:BISLUS-00005aaa195120", FREAD);
    zprintf("open() should truncate filename and find the 19-char file\n");
    zexpect_s32_ne(-1, fd);
    if (fd != -1) {
        close(fd);
    }
}

ZTEST(truncation, firstfile_truncates_long_filename) {
    FILE* f = fopen("bu00/BISLUS-00005aaa195120", "wb");
    zassert_ptr_ne(NULL, f);
    fclose(f);

    struct DIRENTRY d = {0};
    struct DIRENTRY* pd = firstfile((char*)"bu00:", &d);

    zprintf("firstfile() should find the file\n");
    zassert_ptr_ne(NULL, pd);
    zprintf("File name should be truncated to 19 characters\n");
    zexpect_str_eq(TRUNCATED_NAME_19, d.name);
    zprintf("Truncated name length should be exactly 19\n");
    zexpect_u32_eq(19, strlen(d.name));
}

ZTEST(truncation, nextfile_truncates_long_filename) {
    FILE* f1 = fopen("bu00/BISLUS-00005aaa195120", "wb");
    zassert_ptr_ne(NULL, f1);
    fclose(f1);

    FILE* f2 = fopen("bu00/AAAA-SHORT", "wb");
    zassert_ptr_ne(NULL, f2);
    fclose(f2);

    struct DIRENTRY d[2] = {0};
    struct DIRENTRY* pd;

    pd = firstfile((char*)"bu00:", &d[0]);
    zassert_ptr_ne(NULL, pd);
    zexpect_str_eq("AAAA-SHORT", d[0].name);

    pd = nextfile(&d[1]);
    zprintf("nextfile() should find the second file\n");
    zassert_ptr_ne(NULL, pd);
    zprintf("File name should be truncated to 19 characters\n");
    zexpect_str_eq(TRUNCATED_NAME_19, d[1].name);
    zprintf("Truncated name length should be exactly 19\n");
    zexpect_u32_eq(19, strlen(d[1].name));
}

// getScratchAddr() counts 32-bit words, and its memory keeps what is written.
ZTEST(libetc, scratchpad_words) {
    u_long* base = getScratchAddr(0);
    *(int*)getScratchAddr(1) = 0x12345678;
    *(int*)getScratchAddr(255) = -1;
    zexpect_u32_eq(4, (unsigned int)((char*)getScratchAddr(1) - (char*)base));
    zexpect_u32_eq(0x12345678, *(unsigned int*)((char*)base + 4));
    zexpect_s32_eq(-1, *(int*)((char*)base + 1020));
}
