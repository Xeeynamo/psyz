CMAKE_GEN ?= Ninja
CMAKE_BUILD_TYPE ?= Release

.PHONY: all clean format test test-linux32 test-psp-emu test-psp-hw test-ps1-emu test-ps1-hw test-android test-wine help

all: build/native/libpsyz.a

# native build
build/native/libpsyz.a: CMakeLists.txt $(shell find src include -type f)
	cmake -G$(CMAKE_GEN) --log-level=WARNING -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -B build/native
	cmake --build build/native

# cross-compilation build
build/%/libpsyz.a: CMakeLists.txt cmake/%.cmake $(shell find src include -type f)
	cmake -G$(CMAKE_GEN) --log-level=WARNING -DCMAKE_TOOLCHAIN_FILE=cmake/$*.cmake -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -B build/$*
	cmake --build build/$*

UNAME_S := $(shell uname)

ZTEST_RUN := python3 ../ztest/zrunner.py
# Extra zrunner options, e.g. RUNNER_ARGS="--emulator-path=... --bios=..."
RUNNER_ARGS ?=

test:
	@mkdir -p tests/build/sdl3-gpu-hw
	cmake -G$(CMAKE_GEN) --log-level=WARNING -DGTE_USE_HW_SQRT=ON -DCMAKE_BUILD_TYPE=Debug -S tests/ -B tests/build/sdl3-gpu-hw
	cmake --build tests/build/sdl3-gpu-hw
ifeq ($(UNAME_S),Linux)
	SDL_VIDEODRIVER=offscreen $(ZTEST_RUN) native --exe tests/build/sdl3-gpu-hw/psyz_tests --workdir tests $(RUNNER_ARGS)
else
	$(ZTEST_RUN) native --exe tests/build/sdl3-gpu-hw/psyz_tests --workdir tests $(RUNNER_ARGS)
endif

ifneq ($(UNAME_S),Darwin)
	@mkdir -p tests/build/sdl3-gl-sw
	cmake -G$(CMAKE_GEN) --log-level=WARNING -DPSYZ_RENDERER=sdl3_gl -DGTE_USE_HW_SQRT=OFF -DCMAKE_BUILD_TYPE=Debug -S tests/ -B tests/build/sdl3-gl-sw
	cmake --build tests/build/sdl3-gl-sw
ifeq ($(UNAME_S),Linux)
	SDL_VIDEODRIVER=offscreen $(ZTEST_RUN) native --exe tests/build/sdl3-gl-sw/psyz_tests --workdir tests --filter='gpu::*' $(RUNNER_ARGS)
else
	$(ZTEST_RUN) native --exe tests/build/sdl3-gl-sw/psyz_tests --workdir tests --filter='gpu::*' $(RUNNER_ARGS)
endif
endif

test-linux32:
	@mkdir -p tests/build/linux32
	@docker build -t psyz-i686 - < Dockerfile.i686
	@docker run --rm $$([ -t 0 ] && echo -t) -u $$(id -u):$$(id -g) -e HOME=/tmp -e LANG=C.UTF-8 -e CI -e OS -w /src/psyz -v $(abspath ..):/src \
		-v $(CURDIR)/tests/build/linux32:/src/psyz/tests/build psyz-i686 make test

PSP_TEST_CMAKE = cmake -G$(CMAKE_GEN) --log-level=WARNING -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/src/psp/psp.cmake -DCMAKE_BUILD_TYPE=Debug -S tests/ -B tests/build/psp
test-psp-emu:
	$(PSP_TEST_CMAKE) -DBUILD_PRX=OFF -DIS_PPSSPP_EMU=1
	cmake --build tests/build/psp
	$(ZTEST_RUN) psp-emu --exe tests/psyz_tests_psp --workdir tests $(RUNNER_ARGS)

test-psp-hw:
	$(PSP_TEST_CMAKE) -DBUILD_PRX=ON -DIS_PPSSPP_EMU=0
	cmake --build tests/build/psp
	$(ZTEST_RUN) psp-hw --exe tests/psyz_tests_psp.prx --workdir tests $(RUNNER_ARGS)

# PS1 builds link the real PSY-Q libraries through nugget. Pass zrunner options
# with RUNNER_ARGS, e.g. --bios=<image> for pcsx-redux or --serial=<port>.
test-ps1-emu:
	$(MAKE) -C tests/target/ps1
	$(ZTEST_RUN) ps1-emu --exe tests/build/ps1/psyz_tests.ps-exe --workdir tests $(RUNNER_ARGS)

test-ps1-hw:
	$(MAKE) -C tests/target/ps1 packed
	$(ZTEST_RUN) ps1-hw --exe tests/build/ps1/psyz_tests.upx.ps-exe --workdir tests --timeout 1500 $(RUNNER_ARGS)

# Android runs the suite as a native executable over adb shell.
ANDROID_HOME ?= $(HOME)/Android/Sdk
ANDROID_NDK ?= $(lastword $(sort $(wildcard $(ANDROID_HOME)/ndk/*)))
android-build:
	for abi in arm64-v8a x86_64; do \
		cmake -G$(CMAKE_GEN) --log-level=WARNING -DCMAKE_BUILD_TYPE=Debug -DANDROID_ABI=$$abi -DANDROID_PLATFORM=android-24 \
			-DCMAKE_TOOLCHAIN_FILE=$(ANDROID_NDK)/build/cmake/android.toolchain.cmake \
			-S tests/ -B tests/build/android-$$abi && cmake --build tests/build/android-$$abi || exit 1; \
	done

# Runs on the first device adb lists, or boots an emulator when none is online.
test-android: android-build
	$(ZTEST_RUN) android --exe 'tests/build/android-{abi}/psyz_tests' --workdir tests --timeout 900 \
		--emulator-path=$(ANDROID_HOME)/emulator/emulator $(RUNNER_ARGS)

# MinGW runtime DLLs for wine; llvm-mingw keeps them in <root>/x86_64-w64-mingw32/bin
MINGW_BIN ?= /usr/x86_64-w64-mingw32/bin
test-wine:
	@mkdir -p tests/build/wine
	cmake -G$(CMAKE_GEN) --log-level=WARNING -DCMAKE_TOOLCHAIN_FILE=../cmake/windows-x86_64.cmake -DCMAKE_BUILD_TYPE=Debug -S tests/ -B tests/build/wine
	cmake --build tests/build/wine
	WINEPATH="$(MINGW_BIN);$(CURDIR)/tests/build/wine/build/sdl" SDL_VIDEODRIVER=offscreen \
		$(ZTEST_RUN) wine --exe tests/build/wine/psyz_tests.exe --workdir tests $(RUNNER_ARGS)

clean:
	rm -rf build tests/build

CLANG_FORMAT := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))../bin/clang-format
CLANG_FORMAT_URL := https://github.com/Xeeynamo/llvm-clang-format-tidy/releases/download/llvmorg-22.1.4/clang-format.xz

$(CLANG_FORMAT):
	@cd $(dir $(CLANG_FORMAT)) && wget --quiet -O clang-format.xz $(CLANG_FORMAT_URL)
	@cd $(dir $(CLANG_FORMAT)) && sha256sum -c clang-format.xz.sha256
	@cd $(dir $(CLANG_FORMAT)) && xz -d clang-format.xz
	@chmod +x $@

format: $(CLANG_FORMAT)
	find src psxgen tests ../ztest -type f \( -name '*.c' -o -name '*.h' \) -not -name 'stb_image*.h' -not -path '*/build/*' -print0 | xargs -0 -P$$(nproc) -n1 $(CLANG_FORMAT) -i
