CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
CPPFLAGS += -Iinclude $(shell pkg-config --cflags sdl2)
LDLIBS += -lvulkan $(shell pkg-config --libs sdl2)
GLSLANG ?= glslangValidator

GPU_OBJ = build/gpu.o
SHADER = build/shaders/pattern.comp.spv

.PHONY: all test test_ui clean shaders
all: build/gpu_tests build/dungeoncraft

shaders: $(SHADER)

$(SHADER): shaders/sim/pattern.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

build/gpu.o: src/vulkan/gpu.c include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/gpu_tests: tests/vulkan/gpu_tests.c $(GPU_OBJ) $(SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/gpu_tests.c $(GPU_OBJ) -o $@ $(LDLIBS)

build/dungeoncraft: src/app/main.c $(GPU_OBJ) $(SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/app/main.c $(GPU_OBJ) -o $@ $(LDLIBS)

test: build/gpu_tests
	./build/gpu_tests

test_ui: build/dungeoncraft
	./build/dungeoncraft --smoke

clean:
	rm -rf build
