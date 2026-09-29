CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
CFLAGS += -pthread
CPPFLAGS += -Iinclude $(shell pkg-config --cflags sdl2)
LDLIBS += -lvulkan $(shell pkg-config --libs sdl2) -pthread
GLSLANG ?= glslangValidator

GPU_OBJ = build/gpu.o build/device.o build/chunk_gpu.o build/rigid_gpu.o build/tick_gpu.o build/shader.o build/halo_gpu.o build/fluid_gpu.o build/marker_gpu.o build/mpm_gpu.o build/interface_gpu.o build/frontier_gpu.o build/present_gpu.o
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
MPM_SHADER = build/shaders/mpm.comp.spv
MPM_ACTIVITY_SHADER = build/shaders/mpm_active.comp.spv
MPM_COMPONENT_SHADER = build/shaders/mpm_component.comp.spv
BOUNDARY_SHADER = build/shaders/workspace_boundary.comp.spv
FRONTIER_SHADER = build/shaders/wet_frontier.comp.spv

.PHONY: all test test_ui test_ui_long clean shaders
all: build/gpu_tests build/halo_tests build/fluid_tests build/particle_tests build/chunk_tests build/generate_tests build/stream_tests build/dungeoncraft

NATIVE_SRC = src/app/main.c src/app/level.c src/app/offscreen.c src/app/session.c $(patsubst build/%.o,src/vulkan/%.c,$(GPU_OBJ)) src/world/chunk.c src/world/stream.c src/world/generate.c
.PHONY: native
native: build/dungeoncraft_native

.PHONY: half_native test_half_native
half_native: build/dungeoncraft_half_native

test_half_native: build/half_native_config_tests build/dungeoncraft_half_native
	./build/half_native_config_tests
	./build/dungeoncraft_half_native --smoke-native
	./build/dungeoncraft_half_native --smoke-mud

.PHONY: test_half_native_spray
test_half_native_spray: build/dungeoncraft_half_native
	./build/dungeoncraft_half_native --smoke-spray-off
	./build/dungeoncraft_half_native --smoke-spray-on

build/half_native_config_tests: tests/app/half_native_config_tests.c src/app/view_config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_HALF_NATIVE_VIEW -DDC_GPU_CHUNK_SLOTS=187u $< -o $@

build/marker_slot_tests: tests/vulkan/marker_slot_tests.c $(patsubst build/%.o,src/vulkan/%.c,$(GPU_OBJ)) src/world/chunk.c $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER) $(FRONTIER_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_GPU_CHUNK_SLOTS=65u tests/vulkan/marker_slot_tests.c $(patsubst build/%.o,src/vulkan/%.c,$(GPU_OBJ)) src/world/chunk.c -o $@ $(LDLIBS)

.PHONY: test_native
test_native: build/dungeoncraft_native
	./build/dungeoncraft_native --smoke-native

build/dungeoncraft_native: $(NATIVE_SRC) src/app/level.h src/app/view_config.h include/dungeoncraft/gpu.h $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER) $(BOUNDARY_SHADER) $(FRONTIER_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_NATIVE_VIEW -DDC_GPU_CHUNK_SLOTS=608u $(NATIVE_SRC) -o $@ $(LDLIBS)

build/dungeoncraft_half_native: $(NATIVE_SRC) src/app/level.h src/app/view_config.h include/dungeoncraft/gpu.h $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER) $(BOUNDARY_SHADER) $(FRONTIER_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_HALF_NATIVE_VIEW -DDC_GPU_CHUNK_SLOTS=187u -DDC_PRESSURE_SWEEPS=16u $(NATIVE_SRC) -o $@ $(LDLIBS)

shaders: $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER) $(BOUNDARY_SHADER) $(FRONTIER_SHADER)

$(FRONTIER_SHADER): shaders/sim/wet_frontier.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

build/frontier_gpu.o: src/vulkan/frontier_gpu.c src/vulkan/gpu_internal.h $(FRONTIER_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BOUNDARY_SHADER): shaders/sim/workspace_boundary.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

build/interface_gpu.o: src/vulkan/interface_gpu.c src/vulkan/gpu_internal.h $(BOUNDARY_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(MPM_COMPONENT_SHADER): shaders/sim/mpm_component.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(MPM_ACTIVITY_SHADER): shaders/sim/mpm_active.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(MPM_SHADER): shaders/sim/mpm.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

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

build/mpm_gpu.o: src/vulkan/mpm_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
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

build/gpu_tests: tests/vulkan/gpu_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/gpu_tests.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/native_bench: tests/vulkan/native_bench.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/native_bench.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

.PHONY: bench_native
bench_native: build/native_bench
	./build/native_bench

build/halo_tests: tests/vulkan/halo_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/halo_tests.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/fluid_tests: tests/vulkan/fluid_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/fluid_tests.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/particle_tests: tests/vulkan/particle_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ) $(STREAM_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/particle_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ) $(STREAM_OBJ) -o $@ $(LDLIBS)

build/chunk_tests: tests/world/chunk_tests.c $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/world/chunk_tests.c $(CHUNK_OBJ) -o $@

build/generate_tests: tests/world/generate_tests.c $(GENERATE_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/world/generate_tests.c $(GENERATE_OBJ) $(CHUNK_OBJ) -o $@

build/stream_tests: tests/world/stream_tests.c $(STREAM_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/world/stream_tests.c $(STREAM_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ) -o $@ -pthread

build/dungeoncraft: src/app/main.c src/app/level.c src/app/offscreen.c src/app/offscreen.h src/app/level.h src/app/view_config.h src/app/session.c src/app/session.h $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/app/main.c src/app/level.c src/app/offscreen.c src/app/session.c $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) -o $@ $(LDLIBS)

build/controls_tests: tests/app/controls_tests.c src/app/level.c src/app/offscreen.c src/app/offscreen.h src/app/level.h src/app/session.c src/app/session.h $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/app/controls_tests.c src/app/level.c src/app/offscreen.c src/app/session.c $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) -o $@ $(LDLIBS)

test: build/gpu_tests build/halo_tests build/fluid_tests build/particle_tests build/marker_slot_tests build/chunk_tests build/generate_tests build/stream_tests
	./build/gpu_tests
	./build/halo_tests
	./build/fluid_tests
	./build/particle_tests
	./build/marker_slot_tests
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
	./build/dungeoncraft --smoke-granular
	./build/dungeoncraft --smoke-coupled
	./build/dungeoncraft --smoke-mud
	./build/dungeoncraft --smoke-sifting

test_ui_long: build/dungeoncraft
	sh tests/app/long_fluid.sh

clean:
	rm -rf build
