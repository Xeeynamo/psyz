#include <gtest/gtest.h>
extern "C" {
#include <psyz.h>
#include <kernel.h>
#include <libapi.h>
}

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define rmdir(path) _rmdir(path)
#else
#include <sys/stat.h>
#endif

class bu_Test : public testing::Test {
    static void make_file(const char* path, int len, int val, int inc) {
        FILE* f = fopen(path, "wb");
        assert(f != nullptr);
        auto* d = (unsigned char*)malloc(len);
        assert(d != nullptr);
        for (int i = 0; i < len; i++) {
            d[i] = val;
            val += inc;
        }
        assert(fwrite(d, 1, len, f) == len);
        assert(fclose(f) == 0);
        free(d);
    }

  protected:
    bu_Test() {}
    ~bu_Test() override = default;
    void SetUp() override {
        mkdir("bu00", 0755);
        mkdir("bu10", 0755);
        make_file("bu00/BASLUS-00000PSYZ00", 8192, 0, 1);
        make_file("bu00/BASLUS-00000PSYZ01", 8192, 0, 2);
    }
    void TearDown() override {
        assert(remove("bu00/BASLUS-00000PSYZ00") == 0);
        assert(remove("bu00/BASLUS-00000PSYZ01") == 0);
        assert(rmdir("bu00") == 0);
        assert(rmdir("bu10") == 0);
    }
};

TEST_F(bu_Test, for_invalid_path) {
    struct DIRENTRY d = {0};
    struct DIRENTRY* pd;
    pd = firstfile((char*)"none:", &d);
    EXPECT_EQ(pd, nullptr);
    EXPECT_STREQ(d.name, "");
    EXPECT_EQ(d.attr, 0);
    EXPECT_EQ(d.size, 0);
}

TEST_F(bu_Test, for_empty_path) {
    struct DIRENTRY d = {0};
    struct DIRENTRY* pd;
    pd = firstfile((char*)"bu10:", &d);
    EXPECT_EQ(pd, nullptr);
    EXPECT_STREQ(d.name, "");
    EXPECT_EQ(d.attr, 0);
    EXPECT_EQ(d.size, 0);
}

TEST_F(bu_Test, for_first_file) {
    struct DIRENTRY d = {0};
    struct DIRENTRY* pd;
    pd = firstfile((char*)"bu00:", &d);
    EXPECT_EQ(pd, &d);
    EXPECT_STREQ(d.name, "BASLUS-00000PSYZ00");
    EXPECT_EQ(d.attr, 80);
    EXPECT_EQ(d.size, 8192);
}

TEST_F(bu_Test, for_multiple_files) {
    struct DIRENTRY d[2];
    struct DIRENTRY* pd;
    pd = nextfile(firstfile((char*)"bu00:", d) + 1);
    EXPECT_EQ(pd, &d[1]);
    EXPECT_STREQ(pd->name, "BASLUS-00000PSYZ01");
    EXPECT_EQ(pd->attr, 80);
    EXPECT_EQ(pd->size, 8192);
}

TEST_F(bu_Test, for_end_of_file_list) {
    struct DIRENTRY d[3];
    struct DIRENTRY* pd;
    pd = nextfile(nextfile(firstfile((char*)"bu00:", d) + 1) + 2);
    EXPECT_EQ(pd, nullptr);
}

TEST_F(bu_Test, open_existing_file) {
    int fd = open("bu00:BASLUS-00000PSYZ00", FREAD);
    EXPECT_NE(fd, -1);
    close(fd);
}

TEST_F(bu_Test, create_and_write_file) {
    unsigned short expected = 1234, actual;
    int fd = open("bu00:BASLUS-00000PSYZ00", FCREAT);
    EXPECT_NE(fd, -1);
    write(fd, &expected, sizeof(unsigned short));
    close(fd);

    FILE* f = fopen("bu00/BASLUS-00000PSYZ00", "rb");
    int read = fread(&actual, 1, sizeof(unsigned short), f);
    fclose(f);
    EXPECT_EQ(read, sizeof(unsigned short));
    EXPECT_EQ(actual, expected);
}

TEST(PathAdjustment, basic_memory_card_path) {
    char dst[256] = {0};
    Psyz_AdjustPath(dst, "bu00:TESTFILE", sizeof(dst));
#ifdef _WIN32
    EXPECT_STREQ(dst, "bu00\\TESTFILE");
#else
    EXPECT_STREQ(dst, "bu00/TESTFILE");
#endif
}

TEST(PathAdjustment, basic_path_no_adjustment) {
    char dst[256] = {0};
    Psyz_AdjustPath(dst, "regular/path/file.txt", sizeof(dst));
    EXPECT_STREQ(dst, "regular/path/file.txt");
}

static int test_callback_override(char* dst, const char* src, int maxlen) {
    if (strncmp(src, "custom:", 7) == 0) {
        snprintf(dst, maxlen, "overridden/%s", src + 7);
        return (int)strlen(dst);
    }
    return -1;
}

TEST(PathAdjustment, callback_override_positive_return) {
    char dst[256] = {0};
    Psyz_AdjustPathCB(test_callback_override);
    Psyz_AdjustPath(dst, "custom:myfile", sizeof(dst));
    EXPECT_STREQ(dst, "overridden/myfile");
    Psyz_AdjustPathCB(NULL);
}

TEST(PathAdjustment, callback_negative_return_falls_back) {
    char dst[256] = {0};
    Psyz_AdjustPathCB(test_callback_override);
    Psyz_AdjustPath(dst, "bu00:FALLBACK", sizeof(dst));
#ifdef _WIN32
    EXPECT_STREQ(dst, "bu00\\FALLBACK");
#else
    EXPECT_STREQ(dst, "bu00/FALLBACK");
#endif
    Psyz_AdjustPathCB(NULL);
}

TEST(PathAdjustment, callback_null_uses_internal) {
    char dst[256] = {0};
    Psyz_AdjustPathCB(NULL);
    Psyz_AdjustPath(dst, "bu10:TESTFILE", sizeof(dst));
#ifdef _WIN32
    EXPECT_STREQ(dst, "bu10\\TESTFILE");
#else
    EXPECT_STREQ(dst, "bu10/TESTFILE");
#endif
}

TEST(PathAdjustment, truncates_long_filename_to_19_chars) {
    char dst[256] = {0};

    Psyz_AdjustPath(dst, "bu00:1234567890123456789", sizeof(dst));
#ifdef _WIN32
    EXPECT_STREQ(dst, "bu00\\1234567890123456789");
#else
    EXPECT_STREQ(dst, "bu00/1234567890123456789");
#endif

    Psyz_AdjustPath(dst, "bu00:12345678901234567890", sizeof(dst));
#ifdef _WIN32
    EXPECT_STREQ(dst, "bu00\\1234567890123456789");
#else
    EXPECT_STREQ(dst, "bu00/1234567890123456789");
#endif
}

static int test_callback_with_long_name(
    char* dst, const char* src, int maxlen) {
    if (strncmp(src, "custom:", 7) == 0) {
        snprintf(dst, maxlen, "custom_dir/%s", src + 7);
        return (int)strlen(dst);
    }
    return -1;
}

TEST(PathAdjustment, callback_result_also_truncated) {
    char dst[256] = {0};
    Psyz_AdjustPathCB(test_callback_with_long_name);
    Psyz_AdjustPath(dst, "custom:VERYLONGFILENAME1234567890", sizeof(dst));
    EXPECT_STREQ(dst, "custom_dir/VERYLONGFILENAME123");
    Psyz_AdjustPathCB(NULL);
}

class TruncationTest : public testing::Test {
    static void make_file(const char* path, int len) {
        FILE* f = fopen(path, "wb");
        assert(f != nullptr);
        auto* d = (unsigned char*)calloc(len, 1);
        assert(d != nullptr);
        assert(fwrite(d, 1, len, f) == (size_t)len);
        assert(fclose(f) == 0);
        free(d);
    }

  protected:
    static constexpr const char* LONG_FILENAME_21 = "BISLUS-00005aaa195120";
    static constexpr const char* TRUNCATED_NAME_19 = "BISLUS-00005aaa1951";

    void SetUp() override { mkdir("bu00", 0755); }

    void TearDown() override {
        // Clean up any files we created
        remove("bu00/BISLUS-00005aaa195120");
        remove("bu00/BISLUS-00005aaa1951");
        remove("bu00/AAAA-SHORT");
        rmdir("bu00");
    }
};

TEST_F(TruncationTest, open_truncates_long_filename) {
    FILE* f = fopen("bu00/BISLUS-00005aaa1951", "wb");
    ASSERT_NE(f, nullptr);
    fclose(f);

    int fd = open("bu00:BISLUS-00005aaa195120", FREAD);
    EXPECT_NE(fd, -1)
        << "open() should truncate filename and find the 19-char file";
    if (fd != -1) {
        close(fd);
    }
}

TEST_F(TruncationTest, firstfile_truncates_long_filename) {
    FILE* f = fopen("bu00/BISLUS-00005aaa195120", "wb");
    ASSERT_NE(f, nullptr);
    fclose(f);

    struct DIRENTRY d = {0};
    struct DIRENTRY* pd = firstfile((char*)"bu00:", &d);

    ASSERT_NE(pd, nullptr) << "firstfile() should find the file";
    EXPECT_STREQ(d.name, TRUNCATED_NAME_19)
        << "File name should be truncated to 19 characters";
    EXPECT_EQ(strlen(d.name), 19u)
        << "Truncated name length should be exactly 19";
}

TEST_F(TruncationTest, nextfile_truncates_long_filename) {
    FILE* f1 = fopen("bu00/BISLUS-00005aaa195120", "wb");
    ASSERT_NE(f1, nullptr);
    fclose(f1);

    FILE* f2 = fopen("bu00/AAAA-SHORT", "wb");
    ASSERT_NE(f2, nullptr);
    fclose(f2);

    struct DIRENTRY d[2] = {0};
    struct DIRENTRY* pd;

    pd = firstfile((char*)"bu00:", &d[0]);
    ASSERT_NE(pd, nullptr);
    EXPECT_STREQ(d[0].name, "AAAA-SHORT");

    pd = nextfile(&d[1]);
    ASSERT_NE(pd, nullptr) << "nextfile() should find the second file";
    EXPECT_STREQ(d[1].name, TRUNCATED_NAME_19)
        << "File name should be truncated to 19 characters";
    EXPECT_EQ(strlen(d[1].name), 19u)
        << "Truncated name length should be exactly 19";
}
