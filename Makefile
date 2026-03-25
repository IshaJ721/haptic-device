# Keep `make` working from this folder by delegating to a local CMake build.

CMAKE ?= cmake
BUILD_DIR ?= build
BUILD_TYPE ?= Release
JOBS ?= $(shell nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

.PHONY: all configure clean rebuild

all: configure
	$(CMAKE) --build $(BUILD_DIR) --parallel $(JOBS)

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

clean:
	@if [ -d "$(BUILD_DIR)" ]; then $(CMAKE) --build $(BUILD_DIR) --target clean; fi
	rm -rf $(BUILD_DIR)

rebuild: clean all
