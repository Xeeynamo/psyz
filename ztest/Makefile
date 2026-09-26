# Builds the ztest self-test for a target and checks its transcript against
# selftest/selftest.expected.txt. Every target runs headless.
#
#   make test-native | test-wine | test-psp-emu | test-psp-hw
#        test-ps1-emu | test-ps1-hw | test-nds-emu | test-n64-emu
#        test-android

CMAKE := cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
RUN := python3 zrunner.py
SELFTEST = --workdir selftest --selftest selftest/selftest.expected.txt $(RUNNER_ARGS)
# Extra zrunner options, e.g. RUNNER_ARGS="--emulator-path=... --bios=..."
RUNNER_ARGS ?=

PSPDEV ?= $(HOME)/pspdev
BLOCKSDS ?= /opt/blocksds/core
ANDROID_SDK ?= $(HOME)/Android/Sdk
ANDROID_NDK ?= $(lastword $(sort $(wildcard $(ANDROID_SDK)/ndk/*)))
LIBDRAGON_IMAGE ?= ztest-libdragon:local
LIBDRAGON_SRC ?= $(HOME)/.cache/ztest/libdragon

test-native:
	$(CMAKE) -S . -B build/native
	cmake --build build/native
	$(RUN) native --exe build/native/ztest_selftest $(SELFTEST)

test-wine:
	$(CMAKE) -S . -B build/windows \
		-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc
	cmake --build build/windows
	$(RUN) wine --exe build/windows/ztest_selftest.exe $(SELFTEST)

build-psp:
	$(CMAKE) -S . -B build/psp \
		-DCMAKE_TOOLCHAIN_FILE=$(PSPDEV)/psp/share/pspdev.cmake
	cmake --build build/psp

build-psp-prx:
	$(CMAKE) -S . -B build/psp-prx -DBUILD_PRX=ON \
		-DCMAKE_TOOLCHAIN_FILE=$(PSPDEV)/psp/share/pspdev.cmake
	cmake --build build/psp-prx

test-psp-emu: build-psp
	$(RUN) psp-emu --exe build/psp/ztest_selftest $(SELFTEST)

test-psp-hw: build-psp-prx
	$(RUN) psp-hw --exe build/psp-prx/ztest_selftest.prx $(SELFTEST)

build-ps1:
	$(MAKE) -f makefile.psx.mk

# Pass RUNNER_ARGS=--bios=<image> when pcsx-redux has no BIOS configured.
test-ps1-emu: build-ps1
	$(RUN) ps1-emu --exe build/ps1/ztest_selftest.ps-exe $(SELFTEST)

test-ps1-hw: build-ps1
	$(RUN) ps1-hw --exe build/ps1/ztest_selftest.ps-exe --timeout 600 $(SELFTEST)

build-nds:
	$(CMAKE) -S . -B build/nds \
		-DCMAKE_TOOLCHAIN_FILE=$(BLOCKSDS)/cmake/BlocksDS.cmake
	cmake --build build/nds

test-nds-emu: build-nds
	$(RUN) nds-emu --exe build/nds/ztest_selftest.nds $(SELFTEST)

# The libdragon docker image only ships the toolchain: build libdragon into it
# once as $(LIBDRAGON_IMAGE).
libdragon-image:
	test -d $(LIBDRAGON_SRC) || git clone --depth 1 \
		https://github.com/DragonMinded/libdragon.git $(LIBDRAGON_SRC)
	docker rm -f ztest-libdragon-setup >/dev/null 2>&1 || true
	docker run --name ztest-libdragon-setup -v $(LIBDRAGON_SRC):/libdragon \
		-w /libdragon ghcr.io/dragonminded/libdragon:latest \
		sh -c 'make -j$$(nproc) install && make -j$$(nproc) tools-install'
	docker commit ztest-libdragon-setup $(LIBDRAGON_IMAGE)
	docker rm ztest-libdragon-setup

build-n64:
	mkdir -p build/n64
	docker run --rm -u $$(id -u):$$(id -g) -v $(CURDIR):/ztest \
		-w /ztest/build/n64 $(LIBDRAGON_IMAGE) \
		make -f /ztest/makefile.n64.mk

test-n64-emu: build-n64
	$(RUN) n64-emu --exe build/n64/ztest_selftest.z64 $(SELFTEST)

build-android:
	for abi in arm64-v8a x86_64; do \
		$(CMAKE) -S . -B build/android-$$abi -DANDROID_ABI=$$abi \
			-DANDROID_PLATFORM=android-24 \
			-DCMAKE_TOOLCHAIN_FILE=$(ANDROID_NDK)/build/cmake/android.toolchain.cmake \
			&& cmake --build build/android-$$abi || exit 1; \
	done

# Runs on the first device adb lists, or boots an emulator when none is online.
test-android: build-android
	$(RUN) android --exe 'build/android-{abi}/ztest_selftest' --timeout 600 \
		--emulator-path=$(ANDROID_SDK)/emulator/emulator $(SELFTEST)

clean:
	rm -rf build

.PHONY: test-native test-wine build-psp build-psp-prx test-psp-emu test-psp-hw \
	build-ps1 test-ps1-emu test-ps1-hw build-nds test-nds-emu libdragon-image \
	build-n64 test-n64-emu build-android test-android clean
