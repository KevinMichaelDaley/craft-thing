CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
CFLAGS += -pthread
CPPFLAGS += -Iinclude $(shell pkg-config --cflags sdl2)
LDLIBS += -lvulkan $(shell pkg-config --libs sdl2) -pthread
GLSLANG ?= glslangValidator

GPU_OBJ = build/gpu.o build/device.o build/chunk_gpu.o build/rigid_gpu.o
CHUNK_OBJ = build/chunk.o
STREAM_OBJ = build/stream.o
GENERATE_OBJ = build/generate.o
SHADER = build/shaders/pattern.comp.spv
RIGID_SHADER = build/shaders/rigid.comp.spv

.PHONY: all test test_ui clean shaders
all: build/gpu_tests build/chunk_tests build/generate_tests build/stream_tests build/dungeoncraft

shaders: $(SHADER) $(RIGID_SHADER)

$(SHADER): shaders/sim/pattern.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(RIGID_SHADER): shaders/sim/rigid.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

build/gpu.o: src/vulkan/gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/device.o: src/vulkan/device.c src/vulkan/gpu_internal.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/chunk_gpu.o: src/vulkan/chunk_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/chunk.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/rigid_gpu.o: src/vulkan/rigid_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/chunk.o: src/world/chunk.c include/dungeoncraft/chunk.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/stream.o: src/world/stream.c include/dungeoncraft/stream.h include/dungeoncraft/chunk.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/generate.o: src/world/generate.c include/dungeoncraft/generate.h include/dungeoncraft/chunk.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/gpu_tests: tests/vulkan/gpu_tests.c $(GPU_OBJ) $(SHADER) $(RIGID_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/gpu_tests.c $(GPU_OBJ) -o $@ $(LDLIBS)

build/chunk_tests: tests/world/chunk_tests.c $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/world/chunk_tests.c $(CHUNK_OBJ) -o $@

build/generate_tests: tests/world/generate_tests.c $(GENERATE_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/world/generate_tests.c $(GENERATE_OBJ) -o $@

build/stream_tests: tests/world/stream_tests.c $(STREAM_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/world/stream_tests.c $(STREAM_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ) -o $@ -pthread

build/dungeoncraft: src/app/main.c src/app/level.c src/app/level.h $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) $(SHADER) $(RIGID_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/app/main.c src/app/level.c $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) -o $@ $(LDLIBS)

test: build/gpu_tests build/chunk_tests build/generate_tests build/stream_tests
	./build/gpu_tests
	./build/chunk_tests
	./build/generate_tests
	./build/stream_tests

test_ui: build/dungeoncraft
	./build/dungeoncraft --smoke-stream

clean:
	rm -rf build
