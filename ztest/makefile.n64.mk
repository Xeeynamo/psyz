# Builds the ztest self-test ROM with libdragon, from the output directory:
#   make -C <out> -f <ztest>/makefile.n64.mk
# with N64_INST pointing at an installed libdragon (see ztest/README.md).
ZTEST := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR = obj
SOURCE_DIR = $(ZTEST)
include $(N64_INST)/include/n64.mk

# The runner patches the argument marker in the ROM, so keep it uncompressed.
N64_ROM_ELFCOMPRESS = 0
N64_MKDFS_ROOT = filesystem
CFLAGS += -I$(ZTEST)

all: ztest_selftest.z64

filesystem: $(wildcard $(ZTEST)/selftest/expected/*.png)
	rm -rf $@ && mkdir -p $@/expected
	cp $(filter-out %.actual.png,$^) $@/expected/

$(BUILD_DIR)/ztest_selftest.dfs: filesystem
$(BUILD_DIR)/ztest_selftest.elf: $(BUILD_DIR)/ztest.o \
	$(BUILD_DIR)/selftest/selftest.o
ztest_selftest.z64: N64_ROM_TITLE = "ztest selftest"
ztest_selftest.z64: $(BUILD_DIR)/ztest_selftest.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem ztest_selftest.z64

.PHONY: all clean
