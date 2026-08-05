// Minimal GoogleTest-compatible shim videogame consoles.
// All output goes to target_puts, which is configured per-console.
#include <cassert>
#include <ctime>
#include <ios>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <type_traits>

extern "C" void target_puts(const char* text);

namespace testing {

class Message {
  public:
    Message() { buf_[0] = '\0'; }

    Message& operator<<(const char* s) {
        Append("%s", s ? s : "(null)");
        return *this;
    }
    Message& operator<<(char* s) { return *this << (const char*)s; }
    Message& operator<<(const std::string& s) { return *this << s.c_str(); }
    Message& operator<<(bool v) {
        Append("%s", v ? "true" : "false");
        return *this;
    }
    Message& operator<<(char v) {
        Append("%c", v);
        return *this;
    }
    Message& operator<<(const Message& m) {
        Append("%s", m.c_str());
        return *this;
    }
    // std::hex / std::dec toggle the base used for the integral inserters
    Message& operator<<(std::ios_base& (*manip)(std::ios_base&)) {
        hex_ =
            manip == static_cast<std::ios_base& (*)(std::ios_base&)>(std::hex);
        return *this;
    }
    template <typename T> Message& operator<<(const T& v) {
        if constexpr (std::is_integral_v<T>) {
            if (hex_) {
                Append("%llx", (unsigned long long)v);
            } else if constexpr (std::is_signed_v<T>) {
                Append("%lld", (long long)v);
            } else {
                Append("%llu", (unsigned long long)v);
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            Append("%g", (double)v);
        } else if constexpr (std::is_pointer_v<T>) {
            Append("%p", (const void*)v);
        } else if constexpr (std::is_enum_v<T>) {
            Append("%lld", (long long)v);
        } else {
            Append("(unsupported type)");
        }
        return *this;
    }

    const char* c_str() const { return buf_; }

  private:
    void Append(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        size_t len = strlen(buf_);
        if (len + 1 >= sizeof(buf_)) {
            return;
        }
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf_ + len, sizeof(buf_) - len, fmt, args);
        va_end(args);
    }

    char buf_[1024];
    bool hex_ = false;
};

// result object returned by custom assertion predicates: its truthiness
// drives EXPECT_TRUE/ASSERT_TRUE and the streamed text joins the report
class AssertionResult {
  public:
    explicit AssertionResult(bool ok) : ok_(ok) {}
    operator bool() const { return ok_; }
    template <typename T> AssertionResult& operator<<(const T& v) {
        msg_ << v;
        return *this;
    }
    const char* message() const { return msg_.c_str(); }

  private:
    bool ok_;
    Message msg_;
};

inline AssertionResult AssertionSuccess() { return AssertionResult(true); }
inline AssertionResult AssertionFailure() { return AssertionResult(false); }

namespace internal {

// per-run state
struct State {
    bool failed;
    bool fatal;
    bool skipped;
    int total_failed;
    int total_passed;
    int total_skipped;
    const char* traces[8];
    int trace_count;
};
inline State g_state;

inline void PrintFailureLocation(const char* file, int line) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s:%d: Failure\n", file, line);
    target_puts(buf);
}

inline void PrintTraces(void) {
    for (int i = g_state.trace_count - 1; i >= 0; i--) {
        target_puts(i == g_state.trace_count - 1 ? "Google Test trace:\n" : "");
        target_puts(g_state.traces[i]);
        target_puts("\n");
    }
}

// reports the failure when assigned a streamed Message (mirrors how real
// gtest binds the trailing `<< ...` of an assertion macro)
class AssertHelper {
  public:
    AssertHelper(bool fatal, const char* file, int line, const char* summary)
        : fatal_(fatal), file_(file), line_(line), summary_(summary) {}

    void operator=(const Message& message) const {
        PrintFailureLocation(file_, line_);
        target_puts(summary_);
        const char* extra = message.c_str();
        if (extra[0] != '\0') {
            target_puts(extra);
            target_puts("\n");
        }
        PrintTraces();
        g_state.failed = true;
        if (fatal_) {
            g_state.fatal = true;
        }
    }

  private:
    bool fatal_;
    const char* file_;
    int line_;
    const char* summary_;
};

class SkipHelper {
  public:
    void operator=(const Message& message) const {
        g_state.skipped = true;
        const char* extra = message.c_str();
        if (extra[0] != '\0') {
            target_puts(extra);
            target_puts("\n");
        }
    }
};

template <typename T> void PrintValue(char* out, size_t n, const T& v) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        snprintf(out, n, "\"%s\"", ((const std::string&)v).c_str());
    } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        snprintf(out, n, "%s", v ? "true" : "false");
    } else if constexpr (std::is_integral_v<std::decay_t<T>>) {
        if constexpr (std::is_signed_v<std::decay_t<T>>) {
            snprintf(out, n, "%lld", (long long)v);
        } else {
            snprintf(out, n, "%llu", (unsigned long long)v);
        }
    } else if constexpr (std::is_floating_point_v<std::decay_t<T>>) {
        snprintf(out, n, "%g", (double)v);
    } else if constexpr (std::is_convertible_v<T, const char*>) {
        const char* s = (const char*)v;
        snprintf(out, n, "\"%s\"", s ? s : "(null)");
    } else if constexpr (std::is_pointer_v<std::decay_t<T>> ||
                         std::is_null_pointer_v<std::decay_t<T>>) {
        snprintf(out, n, "%p", (const void*)v);
    } else if constexpr (std::is_enum_v<std::decay_t<T>>) {
        snprintf(out, n, "%lld", (long long)v);
    } else {
        snprintf(out, n, "(unsupported type)");
    }
}

struct CmpResult {
    bool ok;
    char text[640];
};

template <typename T1, typename T2, typename Op>
CmpResult Compare(const T1& v1, const T2& v2, const char* expr1,
                  const char* expr2, const char* op_str, Op op) {
    CmpResult r;
    r.ok = op(v1, v2);
    r.text[0] = '\0';
    if (!r.ok) {
        char s1[128], s2[128];
        PrintValue(s1, sizeof(s1), v1);
        PrintValue(s2, sizeof(s2), v2);
        snprintf(r.text, sizeof(r.text),
                 "Expected: (%s) %s (%s), actual: %s vs %s\n", expr1, op_str,
                 expr2, s1, s2);
    }
    return r;
}

inline CmpResult CompareStr(
    const char* v1, const char* v2, const char* expr1, const char* expr2) {
    CmpResult r;
    if (v1 == nullptr || v2 == nullptr) {
        r.ok = v1 == v2;
    } else {
        r.ok = strcmp(v1, v2) == 0;
    }
    r.text[0] = '\0';
    if (!r.ok) {
        snprintf(r.text, sizeof(r.text),
                 "Expected equality of these values:\n  %s\n    Which is: "
                 "\"%s\"\n  %s\n    Which is: \"%s\"\n",
                 expr1, v1 ? v1 : "(null)", expr2, v2 ? v2 : "(null)");
    }
    return r;
}

template <typename T>
CmpResult CompareBool(const T& v, const char* expr, bool expected) {
    CmpResult r;
    r.ok = ((bool)v) == expected;
    r.text[0] = '\0';
    if (!r.ok) {
        snprintf(r.text, sizeof(r.text),
                 "Value of: %s\n  Actual: %s\nExpected: %s\n", expr,
                 expected ? "false" : "true", expected ? "true" : "false");
    }
    return r;
}

// AssertionResult carries its own failure text, include it in the report
inline CmpResult CompareBool(
    const AssertionResult& v, const char* expr, bool expected) {
    CmpResult r;
    r.ok = ((bool)v) == expected;
    r.text[0] = '\0';
    if (!r.ok) {
        snprintf(
            r.text, sizeof(r.text), "Value of: %s\n%s\n", expr, v.message());
    }
    return r;
}

class ScopedTrace {
  public:
    ScopedTrace(const char* file, int line, const Message& message) {
        snprintf(buf_, sizeof(buf_), "%s:%d: %s", file, line, message.c_str());
        if (g_state.trace_count <
            (int)(sizeof(g_state.traces) / sizeof(g_state.traces[0]))) {
            g_state.traces[g_state.trace_count++] = buf_;
        }
    }
    ~ScopedTrace() {
        if (g_state.trace_count > 0) {
            g_state.trace_count--;
        }
    }

  private:
    char buf_[256];
};

} // namespace internal

class Test {
  public:
    virtual ~Test() {}
    virtual void SetUp() {}
    virtual void TearDown() {}
    virtual void TestBody() = 0;
};

namespace internal {

struct TestInfo {
    const char* suite;
    const char* name;
    Test* (*factory)();
};
inline TestInfo g_tests[512];
inline int g_test_count = 0;
inline const char* g_filter = nullptr; // substring on "suite.name"
inline const char* g_excludes[8];
inline int g_exclude_count = 0;

struct Registrar {
    Registrar(const char* suite, const char* name, Test* (*factory)()) {
        if (g_test_count < (int)(sizeof(g_tests) / sizeof(g_tests[0]))) {
            g_tests[g_test_count++] = {suite, name, factory};
        }
    }
};

} // namespace internal

inline void InitGoogleTest(int* argc, char** argv) {
    (void)argc;
    (void)argv;
}

// substring filter on "suite.name"; nullptr runs everything
inline void SetTestFilter(const char* substring) {
    internal::g_filter = substring;
}

// tests whose "suite.name" contains the substring are not run
inline void AddTestExclude(const char* substring) {
    if (internal::g_exclude_count <
        (int)(sizeof(internal::g_excludes) / sizeof(internal::g_excludes[0]))) {
        internal::g_excludes[internal::g_exclude_count++] = substring;
    }
}

inline int Passed(void) { return internal::g_state.total_passed; }
inline int Failed(void) { return internal::g_state.total_failed; }
inline int Skipped(void) { return internal::g_state.total_skipped; }

} // namespace testing

inline int RUN_ALL_TESTS(void) {
    using namespace testing::internal;
    char buf[256], full_name[160];
    g_state.total_failed = 0;
    g_state.total_passed = 0;
    g_state.total_skipped = 0;
    snprintf(
        buf, sizeof(buf), "[==========] Running %d tests.\n", g_test_count);
    target_puts(buf);
    for (int i = 0; i < g_test_count; i++) {
        const TestInfo* info = &g_tests[i];
        snprintf(
            full_name, sizeof(full_name), "%s.%s", info->suite, info->name);
        if (g_filter && !strstr(full_name, g_filter)) {
            continue;
        }
        bool excluded = false;
        for (int j = 0; j < g_exclude_count; j++) {
            if (strstr(full_name, g_excludes[j])) {
                excluded = true;
                break;
            }
        }
        if (excluded) {
            continue;
        }
        snprintf(buf, sizeof(buf), "[ RUN      ] %s\n", full_name);
        target_puts(buf);
        g_state.failed = false;
        g_state.fatal = false;
        g_state.skipped = false;
        g_state.trace_count = 0;
        testing::Test* test = info->factory();
        test->SetUp();
        if (!g_state.fatal && !g_state.skipped) {
            test->TestBody();
        }
        test->TearDown();
        delete test;
        const char* outcome;
        if (g_state.failed) {
            outcome = "[  FAILED  ]";
            g_state.total_failed++;
        } else if (g_state.skipped) {
            outcome = "[  SKIPPED ]";
            g_state.total_skipped++;
        } else {
            outcome = "[       OK ]";
            g_state.total_passed++;
        }
        snprintf(buf, sizeof(buf), "%s %s\n", outcome, full_name);
        target_puts(buf);
    }
    snprintf(buf, sizeof(buf),
             "[==========] %d passed, %d failed, %d skipped.\n",
             g_state.total_passed, g_state.total_failed, g_state.total_skipped);
    target_puts(buf);
    return g_state.total_failed != 0;
}

#define PSYZ_GTEST_CONCAT_(a, b) a##b
#define PSYZ_GTEST_CONCAT(a, b) PSYZ_GTEST_CONCAT_(a, b)

#define PSYZ_GTEST_REPORT_(result, is_fatal)                                   \
    ::testing::internal::AssertHelper(                                         \
        is_fatal, __FILE__, __LINE__, (result).text) = ::testing::Message()

#define PSYZ_GTEST_CMP_(v1, v2, op, op_str, is_fatal, on_fatal)                \
    if (auto psyz_gtest_ar = ::testing::internal::Compare(                     \
            (v1), (v2), #v1, #v2, op_str,                                      \
            [](const auto& a, const auto& b) { return a op b; });              \
        psyz_gtest_ar.ok)                                                      \
        ;                                                                      \
    else                                                                       \
        on_fatal PSYZ_GTEST_REPORT_(psyz_gtest_ar, is_fatal)

#define EXPECT_EQ(v1, v2) PSYZ_GTEST_CMP_(v1, v2, ==, "==", false, )
#define EXPECT_NE(v1, v2) PSYZ_GTEST_CMP_(v1, v2, !=, "!=", false, )
#define EXPECT_LT(v1, v2) PSYZ_GTEST_CMP_(v1, v2, <, "<", false, )
#define EXPECT_LE(v1, v2) PSYZ_GTEST_CMP_(v1, v2, <=, "<=", false, )
#define EXPECT_GT(v1, v2) PSYZ_GTEST_CMP_(v1, v2, >, ">", false, )
#define EXPECT_GE(v1, v2) PSYZ_GTEST_CMP_(v1, v2, >=, ">=", false, )
#define ASSERT_EQ(v1, v2) PSYZ_GTEST_CMP_(v1, v2, ==, "==", true, return)
#define ASSERT_NE(v1, v2) PSYZ_GTEST_CMP_(v1, v2, !=, "!=", true, return)
#define ASSERT_LT(v1, v2) PSYZ_GTEST_CMP_(v1, v2, <, "<", true, return)
#define ASSERT_LE(v1, v2) PSYZ_GTEST_CMP_(v1, v2, <=, "<=", true, return)
#define ASSERT_GT(v1, v2) PSYZ_GTEST_CMP_(v1, v2, >, ">", true, return)
#define ASSERT_GE(v1, v2) PSYZ_GTEST_CMP_(v1, v2, >=, ">=", true, return)

#define PSYZ_GTEST_BOOL_(cond, expected, is_fatal, on_fatal)                   \
    if (auto psyz_gtest_ar =                                                   \
            ::testing::internal::CompareBool((cond), #cond, expected);         \
        psyz_gtest_ar.ok)                                                      \
        ;                                                                      \
    else                                                                       \
        on_fatal PSYZ_GTEST_REPORT_(psyz_gtest_ar, is_fatal)

#define EXPECT_TRUE(cond) PSYZ_GTEST_BOOL_(cond, true, false, )
#define EXPECT_FALSE(cond) PSYZ_GTEST_BOOL_(cond, false, false, )
#define ASSERT_TRUE(cond) PSYZ_GTEST_BOOL_(cond, true, true, return)
#define ASSERT_FALSE(cond) PSYZ_GTEST_BOOL_(cond, false, true, return)

#define PSYZ_GTEST_STREQ_(v1, v2, is_fatal, on_fatal)                          \
    if (auto psyz_gtest_ar =                                                   \
            ::testing::internal::CompareStr((v1), (v2), #v1, #v2);             \
        psyz_gtest_ar.ok)                                                      \
        ;                                                                      \
    else                                                                       \
        on_fatal PSYZ_GTEST_REPORT_(psyz_gtest_ar, is_fatal)

#define EXPECT_STREQ(v1, v2) PSYZ_GTEST_STREQ_(v1, v2, false, )
#define ASSERT_STREQ(v1, v2) PSYZ_GTEST_STREQ_(v1, v2, true, return)

#define ADD_FAILURE()                                                          \
    ::testing::internal::AssertHelper(false, __FILE__, __LINE__, "Failed\n") = \
        ::testing::Message()

#define GTEST_SKIP()                                                           \
    return ::testing::internal::SkipHelper() = ::testing::Message()

#define SCOPED_TRACE(message)                                                  \
    ::testing::internal::ScopedTrace PSYZ_GTEST_CONCAT(                        \
        psyz_gtest_trace_, __LINE__)(                                          \
        __FILE__, __LINE__, ::testing::Message() << message)

#define PSYZ_GTEST_CLASS_(suite, name) suite##_##name##_PsyzTest

#define PSYZ_GTEST_DEFINE_(suite, name, parent)                                \
    class PSYZ_GTEST_CLASS_(suite, name) : public parent {                     \
        void TestBody() override;                                              \
                                                                               \
      public:                                                                  \
        static ::testing::Test* Create() {                                     \
            return new PSYZ_GTEST_CLASS_(suite, name)();                       \
        }                                                                      \
    };                                                                         \
    static ::testing::internal::Registrar PSYZ_GTEST_CONCAT(                   \
        psyz_gtest_reg_, PSYZ_GTEST_CLASS_(suite, name))(                      \
        #suite, #name, &PSYZ_GTEST_CLASS_(suite, name)::Create);               \
    void PSYZ_GTEST_CLASS_(suite, name)::TestBody()

#define TEST(suite, name) PSYZ_GTEST_DEFINE_(suite, name, ::testing::Test)
#define TEST_F(fixture, name) PSYZ_GTEST_DEFINE_(fixture, name, fixture)
