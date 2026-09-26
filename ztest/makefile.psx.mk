# Builds the ztest self-test as a PS1 executable with nugget alone, without PSY-Q:
#   make -f makefile.psx.mk
ZTEST := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
NUGGET ?= $(abspath $(ZTEST)/../nugget)
PREFIX ?= mipsel-none-elf
BUILD ?= $(ZTEST)/build/ps1

CC := $(PREFIX)-gcc
ARCH := -march=mips1 -mabi=32 -EL -fno-pic -mno-shared -mno-abicalls -mfp32 \
	-mno-llsc -fno-stack-protector -nostdlib -ffreestanding
CFLAGS := $(ARCH) -std=c11 -Os -g -ffunction-sections -fdata-sections \
	-mno-gpopt -fno-builtin -Wall -Wextra -D__psx__ -DZTEST_HEAP_SIZE=524288 \
	-I$(ZTEST) -I$(ZTEST)/target/ps1/include
# nugget ships no libc: ztest.c provides what it and stb need.
LIBC := MEMCPY MEMMOVE MEMSET MEMCMP STRLEN STRCMP STRNCMP STRCHR STRCSPN \
	STRCPY STRCAT ABS
CFLAGS += $(addprefix -DZTEST_NEED_,$(LIBC))
LDFLAGS := $(ARCH) -g -static -Wl,--gc-sections -T$(NUGGET)/nooverlay.ld \
	-T$(NUGGET)/ps-exe.ld -Wl,--oformat=elf32-littlemips \
	-Wl,-Map=$(BUILD)/ztest_selftest.map
LIBGCC := $(shell $(CC) -march=mips1 -mabi=32 -EL -print-libgcc-file-name)
OBJS := $(BUILD)/ztest.o $(BUILD)/selftest.o $(BUILD)/crt0.o

all: $(BUILD)/ztest_selftest.ps-exe

$(BUILD)/ztest_selftest.ps-exe: $(BUILD)/ztest_selftest.elf
	$(PREFIX)-objcopy -O binary $< $@

$(BUILD)/ztest_selftest.elf: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBGCC)

$(BUILD)/ztest.o: $(ZTEST)/ztest.c $(ZTEST)/ztest.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/selftest.o: $(ZTEST)/selftest/selftest.c $(ZTEST)/ztest.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/crt0.o: $(NUGGET)/common/crt0/crt0.s | $(BUILD)
	$(CC) $(ARCH) -I$(NUGGET) -Wa,-I$(NUGGET) -c -o $@ $<

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
