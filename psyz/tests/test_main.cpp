#include <gtest/gtest.h>

// Only the PSP target supplies these hooks (tests/target/target_psp.cpp);
// every other platform builds against the stubs below. MSVC has no weak
// symbols, so the choice has to be made by the preprocessor.
#ifdef __PSP__
extern "C" void target_setup(void);
extern "C" void target_report(void);
#else
extern "C" void target_setup(void) {}
extern "C" void target_report(void) {}
#endif

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    target_setup();
    int rc = RUN_ALL_TESTS();
    target_report();
    return rc;
}
