CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
CFLAGS += -pthread
CPPFLAGS += -Iinclude $(shell pkg-config --cflags sdl2)
LDLIBS += -lvulkan $(shell pkg-config --libs sdl2) -pthread
GLSLANG ?= glslangValidator

GPU_OBJ = build/gpu.o build/device.o build/chunk_gpu.o build/rigid_gpu.o build/tick_gpu.o build/shader.o build/halo_gpu.o build/fluid_gpu.o build/marker_gpu.o build/present_gpu.o
CHUNK_OBJ = build/chunk.o
STREAM_OBJ = build/stream.o
GENERATE_OBJ = build/generate.o
SHADER = build/shaders/pattern.comp.spv
RIGID_SHADER = build/shaders/rigid.comp.spv
PROBE_SHADER = build/shaders/tick_probe.comp.spv
HALO_SHADER = build/shaders/halo.comp.spv
FLUID_SHADER = build/shaders/fluid.comp.spv
PROJECTION_SHADER = build/shaders/projection.comp.spv
MARKER_SHADER = build/shaders/marker.comp.spv
SHIFT_SHADER = build/shaders/shift_velocity.comp.spv

.PHONY: all test test_ui test_ui_long clean shaders
all: build/gpu_tests build/halo_tests build/fluid_tests build/particle_tests build/chunk_tests build/generate_tests build/stream_tests build/dungeoncraft

shaders: $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER)

$(SHIFT_SHADER): shaders/sim/shift_velocity.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(MARKER_SHADER): shaders/sim/marker.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(PROJECTION_SHADER): shaders/sim/projection.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(SHADER): shaders/sim/pattern.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(RIGID_SHADER): shaders/sim/rigid.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(PROBE_SHADER): shaders/sim/tick_probe.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(HALO_SHADER): shaders/sim/halo.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(FLUID_SHADER): shaders/sim/fluid.comp
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

build/tick_gpu.o: src/vulkan/tick_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/shader.o: src/vulkan/shader.c src/vulkan/gpu_internal.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/halo_gpu.o: src/vulkan/halo_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/fluid_gpu.o: src/vulkan/fluid_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/present_gpu.o: src/vulkan/present_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/marker_gpu.o: src/vulkan/marker_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
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

build/gpu_tests: tests/vulkan/gpu_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/gpu_tests.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/halo_tests: tests/vulkan/halo_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/halo_tests.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/fluid_tests: tests/vulkan/fluid_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/fluid_tests.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/particle_tests: tests/vulkan/particle_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ) $(STREAM_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/particle_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ) $(STREAM_OBJ) -o $@ $(LDLIBS)

build/chunk_tests: tests/world/chunk_tests.c $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/world/chunk_tests.c $(CHUNK_OBJ) -o $@

build/generate_tests: tests/world/generate_tests.c $(GENERATE_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/world/generate_tests.c $(GENERATE_OBJ) $(CHUNK_OBJ) -o $@

build/stream_tests: tests/world/stream_tests.c $(STREAM_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/world/stream_tests.c $(STREAM_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ) -o $@ -pthread

build/dungeoncraft: src/app/main.c src/app/level.c src/app/level.h src/app/session.c src/app/session.h $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/app/main.c src/app/level.c src/app/session.c $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) -o $@ $(LDLIBS)

build/controls_tests: tests/app/controls_tests.c src/app/level.c src/app/level.h src/app/session.c src/app/session.h $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/app/controls_tests.c src/app/level.c src/app/session.c $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) -o $@ $(LDLIBS)

test: build/gpu_tests build/halo_tests build/fluid_tests build/particle_tests build/chunk_tests build/generate_tests build/stream_tests
	./build/gpu_tests
	./build/halo_tests
	./build/fluid_tests
	./build/particle_tests
	./build/chunk_tests
	./build/generate_tests
	./build/stream_tests

test_ui: build/dungeoncraft build/controls_tests
	./build/controls_tests
	./build/dungeoncraft --smoke-controls-ui
	./build/dungeoncraft --smoke-stream
	./build/dungeoncraft --smoke-world-transfer
	./build/dungeoncraft --smoke-display
	./build/dungeoncraft --smoke-halo-flow
	./build/dungeoncraft --smoke-camera-velocity
	./build/dungeoncraft --smoke-motion

test_ui_long: build/dungeoncraft
	sh tests/app/long_fluid.sh

clean:
	rm -rf build
