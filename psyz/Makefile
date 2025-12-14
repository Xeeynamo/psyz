BUILD_DIR := build
TEST_DIR := tests/build
TEST_DIR_SW := tests/build_sw
TEST_DIR_HW := tests/build_hw

.PHONY: build clean format test

build: $(BUILD_DIR)/libpsyz.a

$(BUILD_DIR)/libpsyz.a: CMakeLists.txt
	cmake -GNinja -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(TEST_DIR)
	rm -rf $(TEST_DIR_SW)
	rm -rf $(TEST_DIR_HW)

format:
	find src psxgen tests -type f -name '*.c' -exec clang-format -i {} +

test:
	@mkdir -p $(TEST_DIR_SW)
	cmake -DGTE_USE_HW_SQRT=OFF -DCMAKE_BUILD_TYPE=Debug -GNinja -S tests/ -B $(TEST_DIR_SW)
	cmake --build $(TEST_DIR_SW)
	cd $(TEST_DIR_SW) && SDL_VIDEODRIVER=offscreen ./psyz_tests

	@mkdir -p $(TEST_DIR_HW)
	cmake -DGTE_USE_HW_SQRT=ON -DCMAKE_BUILD_TYPE=Debug -GNinja -S tests/ -B $(TEST_DIR_HW)
	cmake --build $(TEST_DIR_HW)
	cd $(TEST_DIR_HW) && SDL_VIDEODRIVER=offscreen ./psyz_tests --gtest_filter='gte_Test.*'
