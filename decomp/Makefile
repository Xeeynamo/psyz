PSYQ_VER	:= 400
AS			:= mipsel-linux-gnu-as
CC			:= ./bin/cc1-psx-272
CPP			:= mipsel-linux-gnu-cpp
LD			:= mipsel-linux-gnu-ld
OBJCOPY		:= mipsel-linux-gnu-objcopy
MASPSX		:= python3 tools/maspsx/maspsx.py --expand-div --aspsx-version=2.56

ASFLAGS		:= -Iinclude -march=r3000 -mtune=r3000 -no-pad-sections -O1 -G0
CPPFLAGS	:= -Iinclude -Iinclude/og -Wno-builtin-declaration-mismatch \
	-Dmips -D__OPTIMIZE__ -D__mips__ -Dpsx -D__psx__ -D__psx -D_PSYQ \
	-D__EXTENSIONS__ -D_MIPSEL -D_LANGUAGE_C -DLANGUAGE_C
CFLAGS		:= -G0 -w -O2 -g -fno-builtin -funsigned-char -fpeephole \
	-ffunction-cse -fpcc-struct-return -fcommon -fverbose-asm -msoft-float

LIBS_350    := libapi libc libcd libcard libcomb libetc libgpu libgs libgte
LIBS_350    += libgun libmath libpress libsnd libspu libtap
LIBS_360    := libapi libc libcd libcard libcomb libetc libgpu libgs libgte
LIBS_360    += libgun libmath libpress libsnd libspu libtap
LIBS_400    := libapi libc2 libcd libcard libcomb libds libetc libgpu libgs libgte
LIBS_400    += libgun libmath libmcrd libpress libsio libsnd libspu libtap
LIBS_470	:= libapi libc2 libcd libcard libcomb libds libetc libgpu libgs libgte
LIBS_470	+= libgun libhmd libmath libmcrd libmcx libpad libpress libsio libsnd libspu libtap
LIBS		:= $(LIBS_$(PSYQ_VER))

SRCS := $(foreach lib,$(LIBS),$(wildcard src/$(lib)/*.c))
OBJS := $(patsubst src/%.c,build/src/%.c.o,$(SRCS))
ASM_SRCS := $(wildcard asm/data/*.s) $(foreach lib,$(LIBS),$(wildcard asm/data/$(lib)/*.s))
ASM_OBJS := $(patsubst asm/%.s,build/asm/%.s.o,$(ASM_SRCS))

.PHONY: build
build: build/psyq$(PSYQ_VER).elf

all:
	make build/psyq$(PSYQ_VER).ld -j
	make expected -j

dependencies: tools/maspsx/maspsx.py bin/cc1-psx-272
	cd sdk && make psyq400
	make include/og/libgpu.h

.PHONY: clean
clean:
	git clean -xfd obj/ build/ asm/ assets/
	rm -f *.elf* *.map undefined_*_auto*.txt

.PHONY: format
format:
	find src -type f -name '*.c' -exec clang-format -i {} +

.PHONY: expected
expected: build/psyq$(PSYQ_VER).elf
	rm -rf expected/
	cp -r build expected/

build/psyq$(PSYQ_VER).elf: build/psyq$(PSYQ_VER).ld $(OBJS) $(ASSETS_OBJS)
	@$(MAKE) --no-print-directory build/psyq$(PSYQ_VER).elf.link

build/psyq$(PSYQ_VER).elf.link: $(ASM_OBJS)
	$(LD) -o build/psyq$(PSYQ_VER).elf.intermediate -Map=build/psyq$(PSYQ_VER).map \
		-T symbols.$(PSYQ_VER).bss.txt \
		-T undefined_funcs_auto.$(PSYQ_VER).txt undefined_syms_auto.$(PSYQ_VER).txt -T build/psyq$(PSYQ_VER).ld \
		$(OBJS) $(ASM_OBJS)
	$(OBJCOPY) -O binary build/psyq$(PSYQ_VER).elf.intermediate build/psyq$(PSYQ_VER).elf
	rm -f build/psyq$(PSYQ_VER).elf.intermediate
	sha256sum -c psyq$(PSYQ_VER).elf.sha256
build/src/%.c.o: src/%.c build/psyq$(PSYQ_VER).ld
	mkdir -p $(dir $@)
	$(CPP) $(CPPFLAGS) -lang-c $< | $(CC) $(CFLAGS) -quiet | $(MASPSX) | $(AS) $(ASFLAGS) -o $@
build/asm/%.s.o: asm/%.s
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

build/psyq$(PSYQ_VER).ld: splat$(PSYQ_VER).yaml psyq$(PSYQ_VER).elf.sha256 symbols.$(PSYQ_VER).txt
	rm -rf $@ asm/ build/asm build/src
	.venv/bin/splat split $<
psyq$(PSYQ_VER).elf.sha256: psyq$(PSYQ_VER).elf
	mkdir -p build/
	mv $< build/
	sha256sum build/$< > $@
	mv build/$< .
psyq$(PSYQ_VER).elf: $(patsubst %,obj/$(PSYQ_VER)/%, $(LIBS))
	./rm-o-dups-$(PSYQ_VER).sh
	./psyq-gen.sh $(PSYQ_VER)
	sha256sum -c psyq$(PSYQ_VER).map.sha256
obj/$(PSYQ_VER)/lib%: sdk/psyq$(PSYQ_VER) bin/psyqdump
	mkdir -p $@ && \
		bin/psyqdump $</LIB/LIB$(shell echo $* | tr '[:lower:]' '[:upper:]').LIB $@ || \
		rm -r $@
	@for file in $@/*.OBJ; do \
		output="$$(echo $${file%.OBJ}.o | tr '[:upper:]' '[:lower:]')"; \
		bin/psyq-obj-parser "$$file" -o "$$output" > /dev/null; \
	done
include/og/libgpu.h: sdk/psyq$(PSYQ_VER) bin/psyq2elf
	mkdir -p include/og
	cd bin/psyq2elf && ./psyqinc.sh ../../$</INCLUDE ../../include/og
bin/psyqdump: bin/psyq2elf
	cd bin/psyq2elf && make
	cp bin/psyq2elf/psyqdump bin/
bin/psyq2elf:
	git clone --depth 1 https://gitlab.com/jype/psyq2elf.git bin/psyq2elf
bin/cc1-psx-272:
	wget -O bin/gcc-2.7.2-psx.tar.gz https://github.com/decompals/old-gcc/releases/download/0.13/gcc-2.7.2-psx.tar.gz
	sha256sum -c bin/gcc-2.7.2-psx.tar.gz.sha256
	cd bin && tar -xf gcc-2.7.2-psx.tar.gz
	rm -f bin/gcc-2.7.2-psx.tar.gz bin/cc1plus bin/cpp bin/g++ bin/gcc
	mv bin/cc1 $@
tools/maspsx/maspsx.py:
	git submodule update tools/maspsx
