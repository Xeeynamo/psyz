CMAKE_GEN ?= Ninja
CMAKE_BUILD_TYPE ?= Release

.PHONY: all clean format test test-wine help

all: build/native/libpsyz.a

# native build
build/native/libpsyz.a: CMakeLists.txt $(shell find src include -type f)
	cmake -G$(CMAKE_GEN) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -B build/native
	cmake --build build/native

# cross-compilation build
build/%/libpsyz.a: CMakeLists.txt cmake/%.cmake $(shell find src include -type f)
	cmake -G$(CMAKE_GEN) -DCMAKE_TOOLCHAIN_FILE=cmake/$*.cmake -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -B build/$*
	cmake --build build/$*

UNAME_S := $(shell uname)

test:
	@mkdir -p tests/build/native-sw
	cmake -G$(CMAKE_GEN) -DGTE_USE_HW_SQRT=OFF -DCMAKE_BUILD_TYPE=Debug -S tests/ -B tests/build/native-sw
	cmake --build tests/build/native-sw
ifeq ($(UNAME_S),Linux)
	cd tests/build && SDL_VIDEODRIVER=offscreen ./native-sw/psyz_tests
else
	cd tests/build && ./native-sw/psyz_tests
endif

	@mkdir -p tests/build/native-hw
	cmake -G$(CMAKE_GEN) -DGTE_USE_HW_SQRT=ON -DCMAKE_BUILD_TYPE=Debug -S tests/ -B tests/build/native-hw
	cmake --build tests/build/native-hw
ifeq ($(UNAME_S),Linux)
	cd tests/build && SDL_VIDEODRIVER=offscreen ./native-hw/psyz_tests --gtest_filter='gte_Test.*'
else
	cd tests/build && ./native-hw/psyz_tests --gtest_filter='gte_Test.*'
endif

test-wine:
	@mkdir -p tests/build/wine
	cmake -G$(CMAKE_GEN) -DCMAKE_TOOLCHAIN_FILE=../cmake/windows-x86_64.cmake -DCMAKE_BUILD_TYPE=Debug -S tests/ -B tests/build/wine
	cmake --build tests/build/wine
	cd tests/build && WINEPATH="/usr/x86_64-w64-mingw32/bin;build/sdl" WINEDEBUG=-all SDL_VIDEODRIVER=offscreen wine ./wine/psyz_tests.exe

clean:
	rm -rf build tests/build

format:
	find src psxgen tests -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) -exec clang-format -i {} +
