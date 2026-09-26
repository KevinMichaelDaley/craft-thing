CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
CPPFLAGS += -Iinclude
LDLIBS += -lvulkan
GLSLANG ?= glslangValidator

GPU_OBJ = build/gpu.o
SHADER = build/shaders/pattern.comp.spv

.PHONY: all test clean
all: build/gpu_tests

build/gpu.o: src/vulkan/gpu.c include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/gpu_tests: tests/vulkan/gpu_tests.c $(GPU_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

test: build/gpu_tests
	./build/gpu_tests

clean:
	rm -rf build
