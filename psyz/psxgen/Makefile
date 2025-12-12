TARGET = test
TYPE = ps-exe
PCSX = $$(cat .emu)

SRCS = $(TARGET).c \
../../nugget/common/crt0/crt0.s \

include ../../psyz/psx.mk

# Compile test.c and run it PCSX Redux, then generate the VRAM screenshots
.PHONY: gen
gen: all
	git checkout pcsx.json
	python3 ./psxgen.py $(PCSX) $(TARGET).$(TYPE)
	git checkout pcsx.json

# Compile test.c and run it PCSX Redux
.PHONY: run
run: all
	git checkout pcsx.json
	$(PCSX) -loadexe $(TARGET).$(TYPE) -run
	git checkout pcsx.json
