TARGETS := $(wildcard */)
CLANG_FORMAT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))../bin/clang-format
CLANG_FORMAT_URL := https://github.com/Xeeynamo/llvm-clang-format-tidy/releases/download/llvmorg-22.1.4/clang-format.xz

$(CLANG_FORMAT):
	@cd $(dir $(CLANG_FORMAT)) && wget --quiet -O clang-format.xz $(CLANG_FORMAT_URL)
	@cd $(dir $(CLANG_FORMAT)) && sha256sum -c clang-format.xz.sha256
	@cd $(dir $(CLANG_FORMAT)) && xz -d clang-format.xz
	@chmod +x $@

build: build_psx

clean: clean_psx

.PHONY: build_psx
build_psx:
	for dir in $(TARGETS); do \
		cd "$$dir" && make && cd ..; \
	done

.PHONY: clean_psx
clean_psx:
	for dir in $(TARGETS); do \
		cd "$$dir" && make clean && cd ..; \
	done

.PHONY: build_pc
build_pc:
	for dir in $(TARGETS); do \
		cd "$$dir" && cmake -GNinja -DSDL_STATIC=ON -B build/  && cmake --build build/ && cd ..; \
	done

clean_pc:
	for dir in $(TARGETS); do \
		cd "$$dir" && (rm -rf build/ || true) && cd ..; \
	done

.PHONY: format
format: $(CLANG_FORMAT)
	find . -not -path '*/build/*' -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 -P$$(nproc) -n1 $(CLANG_FORMAT) -i
