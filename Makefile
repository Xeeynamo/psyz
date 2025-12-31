PSYQ_VER := 470
PSYQ_SDK := decomp/sdk/psyq$(PSYQ_VER)/LIB/LIBGPU.LIB

.PHONY: build, clean, format, test
build: psyq_libs psyq_incs
clean:
	cd psyq2elf && git clean -xfd
	cd nugget && git clean -xfd
format:
	cd decomp && make format
	cd psyz && make format
test:
	cd psyz && make test

.PHONY: psyq_libs
psyq_libs: nugget/psyq/lib/libgpu.a
.PHONY: psyq_incs
psyq_incs: nugget/psyq/include/libgpu.h

nugget/psyq/lib/libgpu.a: $(PSYQ_SDK) bin/psyqdump bin/psyq-obj-parser
	cd decomp/bin/psyq2elf && ../../../bin/psyqlib.sh ../../sdk/psyq$(PSYQ_VER)/LIB ../../../nugget/psyq/lib
nugget/psyq/include/libgpu.h: $(PSYQ_SDK) bin/psyqdump
	cd decomp/bin/psyq2elf && ./psyqinc.sh ../../sdk/psyq$(PSYQ_VER)/INCLUDE ../../../nugget/psyq/include
$(PSYQ_SDK):
	cd decomp/sdk && make psyq470
bin/psyq-obj-parser: decomp/bin/psyq-obj-parser
	cp $< $@
bin/psyqdump: decomp/bin/psyqdump
	cp $< $@
decomp/bin/psyq-obj-parser:
	cd decomp/bin && wget https://github.com/psyqz/psyz/releases/download/requirements/psyq-obj-parser.xz
	cd decomp/bin && sha256sum -c psyq-obj-parser.xz.sha256
	cd decomp/bin && xz -d psyq-obj-parser.xz
	chmod +x $@
decomp/bin/psyqdump:
	cd decomp && make bin/psyqdump