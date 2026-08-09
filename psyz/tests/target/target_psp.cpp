#include <gtest/gtest.h>

extern "C" {
#include <psyz.h>
}
#include <stdio.h>

extern "C" void target_puts(const char* text) {
    fputs(text, stdout);
    fflush(stdout);
}

extern "C" void target_setup(void) {
    Psyz_VideoSetAspectMode(PSYZ_ASPECT_SQUARE);
    testing::AddTestExclude("spu_Test.");
}

extern "C" void target_report(void) {
    // the runner relies on this line to know test is done
    printf("PSYZ_TESTS_DONE passed=%d failed=%d skipped=%d\n",
           testing::Passed(), testing::Failed(), testing::Skipped());
    fflush(stdout);
}
