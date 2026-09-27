ROOT := $(abspath ../../../..)
NUGGET ?= $(ROOT)/nugget
ZTEST := $(ROOT)/ztest
TESTS := $(abspath ../..)
BUILD ?= $(TESTS)/build/ps1
PREFIX ?= mipsel-none-elf

CC := $(PREFIX)-gcc
ARCH := -march=mips1 -mabi=32 -EL -fno-pic -mno-shared -mno-abicalls -mfp32 \
	-mno-llsc -fno-stack-protector -nostdlib -ffreestanding
CFLAGS := $(ARCH) -std=gnu11 -Os -g -ffunction-sections -fdata-sections \
	-mno-gpopt -fno-builtin -Wno-attributes -D__psx__ \
	-DZTEST_NEED_MEMCPY -DZTEST_NEED_MEMSET \
	-include $(CURDIR)/ps1_compat.h \
	-I$(CURDIR) -I$(TESTS) -I$(ZTEST) -I$(TESTS)/../include \
	-I$(NUGGET)/psyq/include -I$(NUGGET) -I$(ZTEST)/target/ps1/include
PSYQ_LIBS := -lapi -lc -lc2 -lcard -lcd -lcomb -letc -lgpu -lgs -lgte -lgun \
	-lhmd -lmath -lmcrd -lmcx -lpad -lpress -lsio -lsnd -lspu -ltap
LIBGCC := $(shell $(CC) -march=mips1 -mabi=32 -EL -print-libgcc-file-name)
LDFLAGS := $(ARCH) -g -static -Wl,--gc-sections -T$(NUGGET)/nooverlay.ld \
	-T$(NUGGET)/ps-exe.ld -Wl,--oformat=elf32-littlemips \
	-Wl,-Map=$(BUILD)/psyz_tests.map -L$(NUGGET)/psyq/lib

SRCS := $(TESTS)/test_main.c $(TESTS)/test_libapi.c $(TESTS)/test_libgte.c \
	$(TESTS)/test_libgpu.c $(TESTS)/test_libspu.c \
	$(TESTS)/frontbuffer_psyz.c $(ZTEST)/ztest.c ps1_psyz_stubs.c ps1_heap.c
OBJS := $(addprefix $(BUILD)/,$(notdir $(SRCS:.c=.o))) $(BUILD)/crt0.o

vpath %.c $(TESTS) $(ZTEST) $(CURDIR)

all: $(BUILD)/psyz_tests.ps-exe

$(BUILD)/psyz_tests.ps-exe: $(BUILD)/psyz_tests.elf
	$(PREFIX)-objcopy -O binary $< $@

$(BUILD)/psyz_tests.elf: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -Wl,--start-group $(PSYQ_LIBS) \
		-Wl,--end-group $(LIBGCC)

$(BUILD)/%.o: %.c ps1_compat.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(BUILD)/crt0.o: $(NUGGET)/common/crt0/crt0.s | $(BUILD)
	$(CC) $(ARCH) -Wa,-I$(NUGGET) -c -o $@ $<

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

.PHONY: all clean
