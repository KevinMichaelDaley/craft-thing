CC ?= cc
build/appearance_tests: tests/renderer/appearance_tests.c src/renderer/appearance.c include/ferrum/renderer/appearance.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/renderer/appearance_tests.c src/renderer/appearance.c -o $@ -lm
test: build/appearance_tests
test: build/texture_tests build/texture_gpu_tests build/occlusion_gpu_tests build/lighting_gpu_tests build/shading_tests build/scene_gpu_tests
build/texture_tests: tests/renderer/texture_tests.c src/renderer/appearance.c src/renderer/texture.c include/ferrum/renderer/texture.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/renderer/texture_tests.c src/renderer/appearance.c src/renderer/texture.c -o $@ -lm
build/shaders/texture.comp.spv: shaders/renderer/texture.comp shaders/renderer/texture.glsl
	mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
build/texture_gpu_tests: tests/renderer/texture_gpu_tests.c src/vulkan/texture_gpu.c src/vulkan/renderer_resource.c src/vulkan/renderer_internal.h src/renderer/appearance.c src/renderer/texture.c build/fluid_tests build/shaders/texture.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/texture_gpu.c src/renderer/appearance.c src/renderer/texture.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/shaders/occlusion.comp.spv: shaders/renderer/occlusion.comp
	mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
build/occlusion_gpu_tests: tests/renderer/occlusion_gpu_tests.c src/vulkan/occlusion_gpu.c src/vulkan/renderer_resource.c src/vulkan/renderer_internal.h build/fluid_tests build/shaders/occlusion.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/shaders/lighting.comp.spv: shaders/renderer/lighting.comp include/ferrum/renderer/light_transport.h
	mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
build/shaders/lighting_subgroup.comp.spv: shaders/renderer/lighting.comp include/ferrum/renderer/light_transport.h
	mkdir -p build/shaders
	$(GLSLANG) -DFR_SUBGROUP_SCAN=1 -V --target-env vulkan1.3 $< -o $@
build/lighting_gpu_tests: tests/renderer/lighting_gpu_tests.c src/vulkan/lighting_gpu.c src/vulkan/occlusion_gpu.c src/vulkan/renderer_resource.c src/vulkan/renderer_internal.h build/fluid_tests build/shaders/occlusion.comp.spv build/shaders/lighting.comp.spv build/shaders/lighting_subgroup.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/shading_tests: tests/renderer/shading_tests.c src/renderer/shading.c include/ferrum/renderer/shading.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/renderer/shading.c -o $@ -lm
build/scene_gpu_tests: tests/renderer/scene_gpu_tests.c src/vulkan/scene_gpu.c include/ferrum/renderer/gpu.h build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=fr_occlusion_record
build/renderer_visual_tests: tests/renderer/visual_tests.c tests/renderer/concave_scene.h $(RENDERER_SHADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(PERFORMANCE_OBJ) -o $@ $(LDLIBS)
build/renderer_performance_tests: tests/renderer/performance_tests.c tests/renderer/concave_scene.h $(RENDERER_SHADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(PERFORMANCE_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=fr_renderer_record -Wl,--wrap=fr_occlusion_record -Wl,--wrap=fr_lighting_record -Wl,--wrap=vkCmdDispatch -Wl,--wrap=vkCmdDispatchIndirect
build/authoring_request_tests: tests/structural/authoring_request_tests.c src/structural/authoring.c include/ferrum/structural/authoring.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/structural/authoring_request_tests.c src/structural/authoring.c -o $@
.DEFAULT_GOAL := all
.PHONY: tools tools-offline test_tools
tools:
	python3 tools/bootstrap_vulkan.py bootstrap
tools-offline:
	python3 tools/bootstrap_vulkan.py bootstrap --offline
test_tools:
	python3 tests/tools/toolchain_tests.py
test: test_tools
STRUCTURAL_REFERENCE = tests/structural/reference_solver.c src/structural/material.c
build/structural_failure_tests: tests/structural/failure_tests.c tests/structural/placement_tests.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/structural_equilibrium_tests: tests/structural/equilibrium_tests.c tests/structural/equilibrium_limits_cases.h tests/structural/placement_tests.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/structural_placement_tests: tests/structural/placement_tests.c include/ferrum/structural/gpu.h build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/structural_mechanics_tests: tests/structural/mechanics_tests.c $(STRUCTURAL_REFERENCE) include/ferrum/structural/mechanics.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/structural/mechanics_tests.c $(STRUCTURAL_REFERENCE) -o $@ -lm
test: build/structural_mechanics_tests build/structural_placement_tests build/structural_equilibrium_tests build/structural_failure_tests build/structural_fracture_tests build/structural_impact_tests
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
CFLAGS += -pthread
CPPFLAGS += -Iinclude $(shell pkg-config --cflags sdl2)
LDLIBS += -lvulkan $(shell pkg-config --libs sdl2) -pthread -lm
GLSLANG ?= glslangValidator

GPU_OBJ = build/gpu.o build/device.o build/chunk_gpu.o build/rigid_gpu.o build/body_store_gpu.o build/broadphase_gpu.o build/contact_gpu.o build/tick_gpu.o build/shader.o build/halo_gpu.o build/fluid_gpu.o build/marker_gpu.o build/mpm_gpu.o build/present_gpu.o
GPU_OBJ += build/runtime_body_gpu.o build/runtime_geometry_gpu.o build/body_address_gpu.o build/body_storage_gpu.o
build/runtime_geometry_gpu.o: src/vulkan/runtime_geometry_gpu.c src/vulkan/runtime_body_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/runtime_body_gpu.o: src/vulkan/runtime_body_gpu.c src/vulkan/runtime_body_gpu.h src/vulkan/body_storage_gpu.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/body_address_gpu.o: src/vulkan/body_address_gpu.c src/vulkan/body_address_gpu.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/body_storage_gpu.o: src/vulkan/body_storage_gpu.c src/vulkan/body_storage_gpu.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
GPU_OBJ += build/solver_gpu.o build/solver_commands_gpu.o
GPU_OBJ += build/draw_gpu.o
GPU_OBJ += build/compound_gpu.o build/component_gpu.o build/pixel_gpu.o
GPU_OBJ += build/structural_store_gpu.o
GPU_OBJ += build/structural_store_validate.o
GPU_OBJ += build/structural_store_chunk.o
GPU_OBJ += build/color_gpu.o
build/color_gpu.o: src/vulkan/color_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/color.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
GPU_OBJ += build/window_gpu.o
GPU_OBJ += build/structural_gpu.o build/structural_material.o
GPU_OBJ += build/scene_gpu.o build/renderer_resource.o build/occlusion_gpu.o build/lighting_gpu.o
RENDERER_SHADERS = build/shaders/lighting_subgroup.comp.spv build/shaders/scene.comp.spv build/shaders/occlusion.comp.spv build/shaders/lighting.comp.spv
$(GPU_OBJ): $(RENDERER_SHADERS)
build/scene_gpu.o: src/vulkan/scene_gpu.c src/vulkan/renderer_internal.h src/vulkan/gpu_internal.h include/ferrum/renderer/gpu.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/renderer_resource.o: src/vulkan/renderer_resource.c src/vulkan/renderer_internal.h src/vulkan/gpu_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/occlusion_gpu.o: src/vulkan/occlusion_gpu.c src/vulkan/renderer_internal.h src/vulkan/gpu_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/lighting_gpu.o: src/vulkan/lighting_gpu.c src/vulkan/renderer_internal.h src/vulkan/gpu_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/shaders/scene.comp.spv: shaders/renderer/scene.comp shaders/renderer/texture.glsl shaders/sim/rigid_body.glsl shaders/sim/rigid_shape.glsl shaders/sim/rigid_clip.glsl
	mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@

build/shaders/scene.comp.spv: include/ferrum/renderer/light_transport.h
build/shaders/scene.comp.spv build/shaders/lighting.comp.spv build/shaders/lighting_subgroup.comp.spv: shaders/renderer/directional_storage.glsl
GPU_OBJ += build/mpm_activity_gpu.o
GPU_OBJ += build/authored_gpu.o build/authored_request.o
GPU_OBJ += build/prop_gpu.o build/prop_asset.o
build/prop_gpu.o: src/vulkan/prop_gpu.c src/vulkan/renderer_internal.h src/vulkan/gpu_internal.h include/ferrum/assets/prop.h include/ferrum/assets/prop_gpu.h build/shaders/prop_place.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/prop_asset.o: src/assets/prop.c include/ferrum/assets/prop.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/authored_request.o: src/structural/authoring.c include/ferrum/structural/authoring.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/authored_gpu.o: src/vulkan/authored_gpu.c src/vulkan/gpu_internal.h include/ferrum/structural/authoring.h build/shaders/authored.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
GPU_OBJ += build/pressure_work_gpu.o
build/pressure_work_gpu.o: src/vulkan/pressure_work_gpu.c src/vulkan/gpu_internal.h build/shaders/pressure_work.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/shaders/pressure_work.comp.spv: shaders/sim/pressure_work.comp
	mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
GPU_SRC = $(patsubst build/%.o,src/vulkan/%.c,$(filter-out build/structural_material.o build/authored_request.o build/prop_asset.o,$(GPU_OBJ))) src/structural/material.c src/structural/authoring.c src/assets/prop.c
build/authored_stage_tests: tests/vulkan/authored_stage_tests.c build/compound_body_tests $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/authored_stage_tests
build/shaders/authored.comp.spv: shaders/sim/authored.comp
	mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
build/structural_material.o: src/structural/material.c include/ferrum/structural/mechanics.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(GPU_OBJ) build/chunk.o build/structural_mechanics_tests: include/dungeoncraft/material_density.h
build/shaders/pattern.comp.spv build/shaders/mpm.comp.spv build/shaders/rigid.comp.spv build/shaders/rigid_solver.comp.spv build/shaders/structural.comp.spv build/shaders/broadphase.comp.spv build/shaders/contacts.comp.spv: include/dungeoncraft/material_density.h
build/structural_gpu.o: src/vulkan/structural_gpu.c src/vulkan/gpu_internal.h include/ferrum/structural/gpu.h build/shaders/structural.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/structural_store_gpu.o: src/vulkan/structural_store_gpu.c src/vulkan/gpu_internal.h include/ferrum/structural/gpu.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/structural_store_validate.o: src/vulkan/structural_store_validate.c src/vulkan/structural_store_internal.h src/vulkan/gpu_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/structural_store_gpu.o: src/vulkan/structural_store_internal.h
build/structural_store_chunk.o: src/vulkan/structural_store_chunk.c src/vulkan/structural_store_internal.h src/vulkan/gpu_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/shaders/structural.comp.spv: shaders/sim/structural.comp shaders/sim/structural_equilibrium.glsl shaders/sim/structural_attachment.glsl shaders/sim/structural_load.glsl shaders/sim/structural_stress.glsl shaders/sim/structural_refine.glsl shaders/sim/structural_rank.glsl shaders/sim/structural_failure.glsl shaders/sim/structural_debris.glsl shaders/sim/structural_bonds.glsl shaders/sim/rigid_body.glsl shaders/sim/rigid_shape.glsl
	mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
build/shaders/structural.comp.spv: shaders/sim/rigid_fragment.glsl shaders/sim/pixel_fragment.glsl
build/shaders/structural.comp.spv: shaders/sim/body_carve.glsl
build/shaders/structural.comp.spv: shaders/sim/leaf_debris.glsl
build/shaders/structural.comp.spv build/shaders/scene.comp.spv: shaders/sim/leaf_color.glsl include/dungeoncraft/leaf_material.h
build/shaders/mpm.comp.spv: shaders/sim/mpm_terrain_support.glsl shaders/sim/rigid_body.glsl include/dungeoncraft/terrain_flags.h
$(GPU_OBJ): build/shaders/structural.comp.spv
build/dungeoncraft_native build/dungeoncraft_half_native build/native_bench build/marker_slot_tests build/quarter_native_fluid_tests build/quarter_native_solver_tests build/quarter_native_rigid_solver_tests build/quarter_native_bench build/dungeoncraft_quarter_native: build/shaders/structural.comp.spv
build/window_gpu.o: src/vulkan/window_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/compound_gpu.o: src/vulkan/compound_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
SOLVER_SHADER = build/shaders/rigid_solver.comp.spv
$(GPU_OBJ): $(SOLVER_SHADER)
$(SOLVER_SHADER): shaders/sim/rigid_solver.comp shaders/sim/rigid_body.glsl shaders/sim/structural_load.glsl shaders/sim/rigid_bonds.glsl shaders/sim/structural_bonds.glsl shaders/sim/rigid_bond_budget.glsl
	mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
build/quarter_native_fluid_tests build/quarter_native_solver_tests build/quarter_native_rigid_tests build/quarter_native_bench build/dungeoncraft_quarter_native build/marker_slot_tests build/dungeoncraft_native build/dungeoncraft_half_native build/native_bench: $(SOLVER_SHADER)

build/solver_gpu.o: src/vulkan/solver_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/draw_gpu.o: src/vulkan/draw_gpu.c src/vulkan/gpu_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

CHUNK_OBJ = build/chunk.o
STREAM_OBJ = build/stream.o
GENERATE_OBJ = build/generate.o
SHADER = build/shaders/pattern.comp.spv
RIGID_SHADER = build/shaders/rigid.comp.spv
BROADPHASE_SHADER = build/shaders/broadphase.comp.spv
CONTACT_SHADER = build/shaders/contacts.comp.spv
PROBE_SHADER = build/shaders/tick_probe.comp.spv
HALO_SHADER = build/shaders/halo.comp.spv
FLUID_SHADER = build/shaders/fluid_transport_16.comp.spv build/shaders/fluid.comp.spv build/shaders/fluid_aperture.comp.spv build/shaders/fluid_transport.comp.spv build/shaders/fluid_displacement.comp.spv
PROJECTION_SHADER = build/shaders/projection.comp.spv build/shaders/hydro_head.comp.spv build/shaders/pressure_work.comp.spv
MARKER_SHADER = build/shaders/marker.comp.spv
SHIFT_SHADER = build/shaders/shift_velocity.comp.spv
MPM_SHADER = build/shaders/mpm.comp.spv
MPM_ACTIVITY_SHADER = build/shaders/mpm_active.comp.spv
MPM_COMPACTION_SHADER = build/shaders/mpm_compact.comp.spv
MPM_COMPONENT_SHADER = build/shaders/mpm_component.comp.spv

build/compound_body_tests: tests/vulkan/compound_body_tests.c tests/vulkan/compound_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(BROADPHASE_SHADER) $(CONTACT_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -lm

build/rigid_solver_tests: tests/vulkan/rigid_solver_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -lm

.PHONY: all test test_ui test_ui_long test_rigid_dynamics test_presentation clean shaders
all: build/gpu_tests build/halo_tests build/fluid_tests build/particle_tests build/chunk_tests build/generate_tests build/stream_tests build/dungeoncraft

NATIVE_SRC = src/app/main.c src/app/prop_scene.c src/app/structural_demo.c src/app/level.c src/app/session.c $(GPU_SRC) src/world/chunk.c src/world/stream.c src/world/generate.c
build/rigid_dynamics_visual_tests: tests/app/rigid_dynamics_visual_tests.c tests/vulkan/compound_fixture.h $(GPU_OBJ) src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_QUARTER_NATIVE_VIEW -DDC_GPU_CHUNK_SLOTS=70u -DDC_PRESSURE_SWEEPS=16u $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS) -lm

test_rigid_dynamics: build/compound_body_tests build/rigid_dynamics_visual_tests
	./build/compound_body_tests
	./build/rigid_dynamics_visual_tests

test: build/compound_body_tests
build/presentation_tests: tests/app/presentation_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/rigid_dynamics_visual_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -lm -Wl,--wrap=vkCreateSwapchainKHR -Wl,--wrap=vkAcquireNextImageKHR -Wl,--wrap=vkQueueWaitIdle -Wl,--wrap=vkQueuePresentKHR -Wl,--wrap=vkWaitForFences -Wl,--wrap=vkCmdPipelineBarrier2

test_presentation: build/presentation_tests
	./build/presentation_tests

test_ui test_quarter_native: build/rigid_dynamics_visual_tests build/presentation_tests
$(GPU_OBJ): $(BROADPHASE_SHADER) $(CONTACT_SHADER)
$(RIGID_SHADER) $(PROBE_SHADER) $(BROADPHASE_SHADER): shaders/sim/rigid_body.glsl
$(RIGID_SHADER): shaders/sim/rigid_shape.glsl
$(RIGID_SHADER): shaders/sim/structural_load.glsl
build/quarter_native_fluid_tests build/quarter_native_solver_tests build/quarter_native_rigid_tests build/quarter_native_bench build/dungeoncraft_quarter_native build/marker_slot_tests: $(BROADPHASE_SHADER)
build/dungeoncraft_native build/dungeoncraft_half_native build/native_bench: $(BROADPHASE_SHADER) src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
build/quarter_native_fluid_tests build/quarter_native_solver_tests build/quarter_native_rigid_tests build/quarter_native_bench build/dungeoncraft_quarter_native build/marker_slot_tests build/dungeoncraft_native build/dungeoncraft_half_native build/native_bench: $(CONTACT_SHADER)
$(CONTACT_SHADER): shaders/sim/contacts.comp shaders/sim/rigid_body.glsl shaders/sim/contact_geometry.glsl include/dungeoncraft/contact_work_layout.h
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<
$(BROADPHASE_SHADER): shaders/sim/broadphase.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<
build/broadphase_gpu.o: src/vulkan/broadphase_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/body_store_gpu.o: src/vulkan/body_store_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/contact_gpu.o: src/vulkan/contact_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/contact_tests: tests/vulkan/contact_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/world_body_tests: tests/vulkan/world_body_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/convex_body_tests: tests/vulkan/convex_body_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
QUARTER_FLAGS = -DDC_QUARTER_NATIVE_VIEW -DDC_GPU_CHUNK_SLOTS=70u -DDC_PRESSURE_SWEEPS=16u
PERFORMANCE_OBJ = $(patsubst %.c,build/performance/%.o,$(GPU_SRC) src/world/chunk.c)
build/performance/%.o: %.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) -MMD -MP -c $< -o $@
build/renderer_performance_tests: $(PERFORMANCE_OBJ)
build/renderer_visual_tests: $(PERFORMANCE_OBJ)
QUARTER_APP_OBJ = $(patsubst %.c,build/performance/%.o,$(filter-out $(GPU_SRC) src/world/chunk.c,$(NATIVE_SRC)))
-include $(QUARTER_APP_OBJ:.o=.d)
FRAME_TRACE_LINK = -Wl,--wrap=dc_gpu_present_chunks_steps -Wl,--wrap=vkBeginCommandBuffer -Wl,--wrap=dc_gpu_record_rigid -Wl,--wrap=dc_gpu_record_fluid_phase -Wl,--wrap=dc_gpu_record_mpm -Wl,--wrap=dc_gpu_record_contacts -Wl,--wrap=vkCmdBindPipeline -Wl,--wrap=vkCmdDispatch -Wl,--wrap=vkCmdDispatchIndirect -Wl,--wrap=fr_renderer_record
FRAME_TRACE_LINK += -Wl,--wrap=vkCmdPushConstants -Wl,--wrap=dc_gpu_record_component_extraction -Wl,--wrap=dc_gpu_record_broadphase -Wl,--wrap=dc_gpu_record_solver
FRAME_TRACE_LINK += -Wl,--wrap=vkCreateComputePipelines -Wl,--wrap=vkAllocateMemory -Wl,--wrap=vkAcquireNextImageKHR -Wl,--wrap=vkQueuePresentKHR -Wl,--wrap=vkWaitForFences -Wl,--wrap=vkQueueWaitIdle -Wl,--wrap=vkDeviceWaitIdle -Wl,--wrap=dc_gpu_runtime_bodies_service
build/diagnostics/quarter_app_frame_trace: tools/diagnostics/frame_trace.c $(QUARTER_APP_OBJ) $(PERFORMANCE_OBJ) $(RENDERER_SHADERS) build/dungeoncraft_quarter_native
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(QUARTER_APP_OBJ) $(PERFORMANCE_OBJ) -o $@ $(LDLIBS) $(FRAME_TRACE_LINK)
NATIVE360_OBJ = $(patsubst %.c,build/native360/%.o,$(NATIVE_SRC))
build/native360/%.o: %.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_360_VIEW -DDC_GPU_CHUNK_SLOTS=96u -DDC_PRESSURE_SWEEPS=16u -MMD -MP -c $< -o $@
-include $(NATIVE360_OBJ:.o=.d)
build/dungeoncraft_360: $(NATIVE360_OBJ) build/dungeoncraft_quarter_native $(RENDERER_SHADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NATIVE360_OBJ) -o $@ $(LDLIBS)

build/lighting_flag_tests: tests/app/lighting_flag_tests.c build/dungeoncraft_360
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
.PHONY: test_lighting_flag
test_lighting_flag: build/lighting_flag_tests
	./build/lighting_flag_tests
-include $(PERFORMANCE_OBJ:.o=.d)
NATIVE_720_FLAGS = -DDC_GPU_CHUNK_SLOTS=270u
build/broadphase_tests: tests/vulkan/broadphase_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/quarter_native_fluid_tests build/quarter_native_solver_tests build/quarter_native_bench build/dungeoncraft_quarter_native: src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
.PHONY: quarter_native test_quarter_native bench_quarter_native test_quarter_native_spray
build/quarter_native_structural_placement_tests: tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
build/quarter_native_structural_equilibrium_tests: tests/structural/equilibrium_tests.c tests/structural/equilibrium_limits_cases.h tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
build/quarter_native_structural_failure_tests: tests/structural/failure_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
build/quarter_native_structural_fracture_tests: tests/structural/fracture_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
build/structural_cadence_tests: tests/structural/cadence_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
build/structural_fracture_tests: tests/structural/fracture_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/structural_impact_tests: tests/structural/impact_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/structural_buckling_tests: tests/structural/buckling_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/structural_streaming_tests: tests/structural/streaming_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/structural_store_tests: tests/structural/store_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/structural_root_identity_tests: tests/structural/root_identity_tests.c tests/structural/placement_tests.c include/ferrum/structural/gpu.h $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/structural_particle_identity_tests: tests/structural/particle_identity_tests.c tests/structural/placement_tests.c include/ferrum/structural/gpu.h $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/quarter_native_structural_particle_identity_tests: tests/structural/particle_identity_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
build/quarter_native_structural_root_identity_tests: tests/structural/root_identity_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
build/quarter_native_structural_store_tests: tests/structural/store_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
build/quarter_native_structural_streaming_tests: tests/structural/streaming_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
test: build/structural_streaming_tests build/structural_store_tests build/structural_root_identity_tests build/structural_particle_identity_tests
test_quarter_native: build/quarter_native_structural_streaming_tests build/quarter_native_structural_store_tests build/quarter_native_structural_root_identity_tests build/quarter_native_structural_particle_identity_tests
build/quarter_native_structural_buckling_tests: tests/structural/buckling_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
test: build/structural_buckling_tests
test_quarter_native: build/quarter_native_structural_buckling_tests
build/quarter_native_structural_impact_tests: tests/structural/impact_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
test_quarter_native: build/quarter_native_structural_impact_tests
$(SOLVER_SHADER): shaders/sim/rigid_impact.glsl
build/shaders/structural.comp.spv: shaders/sim/structural_impact.glsl
build/shaders/structural.comp.spv: shaders/sim/structural_identity.glsl
build/structural_bench: tests/structural/benchmark.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_SRC) src/world/chunk.c build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)
.PHONY: bench_structural
bench_structural: build/structural_bench
	./build/structural_bench
.PHONY: test_structural_budget
test_structural_budget: build/structural_bench
	./build/structural_bench --check-budget
test_ui test_quarter_native: build/structural_cadence_tests
test_quarter_native: build/quarter_native_structural_placement_tests build/quarter_native_structural_equilibrium_tests build/quarter_native_structural_failure_tests build/quarter_native_structural_fracture_tests
.PHONY: test_quarter_native_perf
test_quarter_native_perf: build/quarter_native_perf_tests build/quarter_native_bench build/pixel_performance_tests
	./build/quarter_native_perf_tests
	env -u DC_VK_VALIDATE ./build/pixel_performance_tests

build/quarter_native_perf_tests: tests/vulkan/quarter_native_perf_tests.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

build/quarter_native_fluid_tests: tests/vulkan/quarter_native_fluid_tests.c Makefile src/app/view_config.h $(GPU_SRC) src/world/chunk.c $(SHADER) $(RIGID_SHADER) $(PROJECTION_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

build/quarter_native_solver_tests: tests/vulkan/fluid_tests.c Makefile $(GPU_SRC) src/world/chunk.c $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

quarter_native: build/dungeoncraft_quarter_native

build/quarter_native_rigid_tests: tests/app/quarter_native_rigid_tests.c Makefile src/app/level.h src/app/view_config.h $(filter-out src/app/main.c,$(NATIVE_SRC)) include/dungeoncraft/gpu.h src/vulkan/gpu_internal.h $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(filter-out src/app/main.c,$(NATIVE_SRC)) -o $@ $(LDLIBS)
build/structural_world_tests: tests/app/structural_world_tests.c Makefile src/app/level.h src/app/view_config.h $(filter-out src/app/main.c,$(NATIVE_SRC)) build/quarter_native_rigid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(filter-out src/app/main.c,$(NATIVE_SRC)) -o $@ $(LDLIBS)

build/quarter_native_rigid_solver_tests: tests/vulkan/rigid_solver_tests.c Makefile src/app/view_config.h $(GPU_SRC) src/world/chunk.c $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER) $(SOLVER_SHADER) $(BROADPHASE_SHADER) $(CONTACT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS) -lm

test_quarter_native: build/quarter_native_config_tests build/quarter_native_fluid_tests build/quarter_native_solver_tests build/quarter_native_rigid_tests build/quarter_native_rigid_solver_tests build/dungeoncraft_quarter_native build/structural_world_tests
	./build/pixel_dynamics_visual_tests
	./build/quarter_native_structural_placement_tests
	./build/quarter_native_structural_equilibrium_tests
	./build/quarter_native_structural_failure_tests
	./build/quarter_native_structural_fracture_tests
	./build/quarter_native_structural_impact_tests
	./build/quarter_native_structural_buckling_tests
	./build/quarter_native_structural_streaming_tests
	./build/quarter_native_structural_store_tests
	./build/quarter_native_structural_root_identity_tests
	./build/quarter_native_structural_particle_identity_tests
	./build/structural_cadence_tests
	./build/presentation_tests
	./build/quarter_native_config_tests
	./build/quarter_native_fluid_tests
	./build/quarter_native_solver_tests
	./build/quarter_native_buoyancy_tests
	./build/quarter_native_structural_submerged_load_tests
	./build/quarter_native_component_material_tests
	./build/quarter_native_component_extraction_tests
	./build/quarter_native_component_acceptance_tests
	./build/component_world_tests
	./build/quarter_native_rigid_tests
	./build/structural_world_tests
	./build/structural_visual_scene_tests
	./build/structural_material_visual_tests
	./build/structural_demo_controls_tests
	./build/quarter_native_rigid_solver_tests
	./build/rigid_dynamics_visual_tests
	./build/dungeoncraft_quarter_native --smoke-native

test_quarter_native_spray: build/dungeoncraft_quarter_native
	./build/dungeoncraft_quarter_native --smoke-spray-off
	./build/dungeoncraft_quarter_native --smoke-spray-on

bench_quarter_native: build/quarter_native_bench
	./build/quarter_native_bench

build/dungeoncraft_quarter_native: Makefile $(NATIVE_SRC) src/app/level.h src/app/view_config.h include/dungeoncraft/gpu.h $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $(QUARTER_APP_OBJ) $(PERFORMANCE_OBJ) -o $@ $(LDLIBS)
build/dungeoncraft_quarter_native: $(QUARTER_APP_OBJ) $(PERFORMANCE_OBJ)

build/quarter_native_bench: tests/vulkan/native_bench.c Makefile tests/vulkan/native_scene.h src/app/view_config.h $(GPU_SRC) src/world/chunk.c $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) tests/vulkan/native_bench.c $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

.PHONY: native
native: build/dungeoncraft_native

.PHONY: half_native test_half_native
build/quarter_native_config_tests: tests/app/quarter_native_config_tests.c Makefile src/app/view_config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< -o $@

half_native: build/dungeoncraft_half_native

test_half_native: build/half_native_config_tests build/dungeoncraft_half_native
	./build/half_native_config_tests
	./build/dungeoncraft_half_native --smoke-native

.PHONY: test_half_native_spray
test_half_native_spray: build/dungeoncraft_half_native
	./build/dungeoncraft_half_native --smoke-spray-off
	./build/dungeoncraft_half_native --smoke-spray-on

build/half_native_config_tests: tests/app/half_native_config_tests.c src/app/view_config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_HALF_NATIVE_VIEW -DDC_GPU_CHUNK_SLOTS=187u $< -o $@

build/marker_slot_tests: tests/vulkan/marker_slot_tests.c $(GPU_SRC) src/world/chunk.c $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_GPU_CHUNK_SLOTS=65u tests/vulkan/marker_slot_tests.c $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

.PHONY: test_native
test_native: build/dungeoncraft_native
	./build/dungeoncraft_native --smoke-native

build/dungeoncraft_native: $(NATIVE_SRC) src/app/level.h src/app/view_config.h include/dungeoncraft/gpu.h $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_NATIVE_VIEW -DDC_GPU_CHUNK_SLOTS=608u $(NATIVE_SRC) -o $@ $(LDLIBS)

build/dungeoncraft_half_native: $(NATIVE_SRC) src/app/level.h src/app/view_config.h include/dungeoncraft/gpu.h $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_HALF_NATIVE_VIEW -DDC_GPU_CHUNK_SLOTS=187u -DDC_PRESSURE_SWEEPS=16u $(NATIVE_SRC) -o $@ $(LDLIBS)

shaders: $(CONTACT_SHADER) $(BROADPHASE_SHADER) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)

$(MPM_COMPONENT_SHADER): shaders/sim/mpm_component.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(MPM_ACTIVITY_SHADER): shaders/sim/mpm_active.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

$(MPM_COMPACTION_SHADER): shaders/sim/mpm_compact.comp shaders/sim/rigid_body.glsl shaders/sim/mpm_rigid_cache.glsl include/dungeoncraft/material_density.h include/dungeoncraft/mpm_activity_layout.h
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

build/shaders/projection.comp.spv: shaders/sim/projection.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<

build/shaders/hydro_head.comp.spv: shaders/sim/hydro_head.comp
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

build/shaders/fluid.comp.spv: shaders/sim/fluid.comp shaders/sim/fluid_aperture.glsl
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

build/mpm_activity_gpu.o: src/vulkan/mpm_activity_gpu.c src/vulkan/gpu_internal.h $(MPM_COMPACTION_SHADER)
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

build/rigid_tests: tests/vulkan/rigid_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/native_bench: tests/vulkan/native_bench.c tests/vulkan/native_scene.h $(GPU_SRC) src/world/chunk.c $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(HALO_SHADER) $(FLUID_SHADER) $(PROJECTION_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_GPU_CHUNK_SLOTS=608u tests/vulkan/native_bench.c $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

build/native_scene_tests: tests/vulkan/native_scene_tests.c tests/vulkan/native_scene.h $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(CHUNK_OBJ) -o $@

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

build/dungeoncraft: src/app/structural_demo.c src/app/structural_demo.h src/app/main.c src/app/level.c src/app/level.h src/app/view_config.h src/app/session.c src/app/session.h $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/app/main.c src/app/prop_scene.c src/app/structural_demo.c src/app/level.c src/app/session.c $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) -o $@ $(LDLIBS)

build/controls_tests: tests/app/controls_tests.c src/app/level.c src/app/level.h src/app/session.c src/app/session.h $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) $(SHADER) $(RIGID_SHADER) $(PROBE_SHADER) $(MARKER_SHADER) $(SHIFT_SHADER) $(MPM_SHADER) $(MPM_ACTIVITY_SHADER) $(MPM_COMPONENT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/app/controls_tests.c src/app/level.c src/app/session.c $(GPU_OBJ) $(CHUNK_OBJ) $(STREAM_OBJ) $(GENERATE_OBJ) -o $@ $(LDLIBS)

test: build/authoring_request_tests build/rigid_solver_tests build/contact_tests build/convex_body_tests build/broadphase_tests build/world_body_tests build/rigid_tests build/quarter_native_config_tests build/quarter_native_fluid_tests build/native_scene_tests build/gpu_tests build/halo_tests build/fluid_tests build/particle_tests build/marker_slot_tests build/chunk_tests build/generate_tests build/stream_tests
	./build/appearance_tests
	./build/texture_tests
	./build/texture_gpu_tests
	./build/occlusion_gpu_tests
	./build/lighting_gpu_tests
	./build/shading_tests
	./build/scene_gpu_tests
	./build/albedo_work_tests
	./build/dry_fluid_tests
	./build/fluid_specialization_tests
	./build/pressure_work_tests
	./build/grain_bed_work_tests
	./build/terrain_prop_tests
	./build/body_color_tests
	./build/body_carve_tests
	./build/sequential_detachment_tests
	./build/dense_support_tests
	./build/loose_particle_role_tests
	./build/mpm_support_dirty_tests
	./build/mpm_gather_tests
	./build/mpm_gather_partial_tile_tests
	./build/mpm_transfer_load_tests
	./build/mpm_shader_storage_tests
	./build/mpm_copy_tests
	./build/solver_motion_store_tests
	./build/solver_contact_start_tests
	./build/solver_grain_fusion_tests
	./build/fluid_recompute_requests_tests
	./build/fluid_dry_line_tests
	./tools/with-vulkan python3 tests/vulkan/transport_request_storage_test.py
	./build/solver_recording_tests
	./build/solver_commands_tests
	./build/body_address_gpu_tests
	./build/body_storage_gpu_tests
	./build/body_scan_gpu_tests
	./build/body_page_init_gpu_tests
	./build/pixel_motion_cache_tests
	./build/component_dispatch_tests
	./build/pixel_properties_gpu_tests
	./build/hydro_head_tests
	./build/hydro_head_work_tests
	./build/hydro_head_tall_work_tests
	./build/fluid_aperture_tests
	./build/raster_dispatch_tests
	./build/contact_dispatch_tests
	./build/contact_terrain_work_tests
	./build/contact_piece_work_tests
	./build/mpm_control_tests
	./build/mpm_compaction_tests
	./build/mpm_descriptor_tests
	./build/raster_equivalence_tests
	./build/fluid_transport_tests
	./build/water_color_tests
	./build/fluid_transport_long_tests
	./build/fluid_transport_slow_tests
	./build/fluid_transport_partial_tests
	./build/fluid_color_sync_tests
	./build/fluid_displacement_tests
	./build/fluid_displacement_work_tests
	./build/fluid_displacement_wide_tests
	./build/fluid_displacement_tall_tests
	./build/diagnostics/fluid_case displacement-runtime
	./build/authoring_request_tests
	./build/authored_stage_tests
	./build/pixel_component_tests
	./build/pixel_world_tests
	./build/pixel_geometry_tests
	./build/pixel_boundary_tests
	./build/structural_mechanics_tests
	./build/structural_placement_tests
	./build/structural_submerged_load_tests
	./build/structural_equilibrium_tests
	./build/structural_failure_tests
	./build/structural_fracture_tests
	./build/structural_impact_tests
	./build/structural_buckling_tests
	./build/structural_streaming_tests
	./build/structural_store_tests
	./build/structural_shadow_chunk_store_tests
	./build/structural_composition_fracture_tests
	./build/structural_composition_energy_tests
	./build/structural_root_identity_tests
	./build/structural_particle_identity_tests
	./build/rigid_solver_tests
	./build/compound_body_tests
	./build/contact_tests
	./build/convex_body_tests
	./build/broadphase_tests
	./build/world_body_tests
	./build/rigid_tests
	./build/quarter_native_config_tests
	./build/quarter_native_fluid_tests
	./build/native_scene_tests
	./build/gpu_tests
	./build/halo_tests
	./build/fluid_tests
	./build/buoyancy_tests
	./build/component_material_tests
	./build/component_extraction_tests
	./build/dynamic_runtime_tests
	./build/component_cache_tests
	./build/fluid_one_cell_scan_tests
	./build/component_acceptance_tests
	./build/particle_tests
	./build/marker_slot_tests
	./build/chunk_tests
	./build/generate_tests
	./build/scene_ground_tests
	./build/stream_tests

test_ui: build/dungeoncraft build/controls_tests build/structural_world_tests
	./build/level03_support_tests
	./build/pixel_dynamics_visual_tests
	./build/structural_cadence_tests
	./build/presentation_tests
	./build/controls_tests
	./build/component_world_tests
	./build/structural_world_tests
	./build/structural_visual_scene_tests
	./build/structural_material_visual_tests
	./build/structural_demo_controls_tests
	./build/rigid_dynamics_visual_tests
	./build/dungeoncraft --smoke-controls-ui
	./build/dungeoncraft --smoke-stream
	./build/dungeoncraft --smoke-world-transfer
	./build/dungeoncraft --smoke-display
	./build/dungeoncraft --smoke-halo-flow
	./build/dungeoncraft --smoke-camera-velocity
	./build/dungeoncraft --smoke-motion
	./build/dungeoncraft --smoke-granular
	./build/dungeoncraft --smoke-coupled
	./build/dungeoncraft --smoke-sifting

test_ui_long: build/dungeoncraft
	sh tests/app/long_fluid.sh

clean:
	rm -rf build

build/buoyancy_tests: tests/vulkan/buoyancy_tests.c build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/shaders/rigid_solver.comp.spv: shaders/sim/rigid_hydro.glsl shaders/sim/rigid_grain.glsl shaders/sim/grain_bed.glsl
build/shaders/rigid.comp.spv build/shaders/rigid_solver.comp.spv: shaders/sim/rigid_clip.glsl

test: build/buoyancy_tests
test_quarter_native: build/quarter_native_buoyancy_tests
build/quarter_native_buoyancy_tests: tests/vulkan/buoyancy_tests.c Makefile build/quarter_native_solver_tests $(GPU_SRC) src/world/chunk.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

build/component_material_tests: tests/vulkan/component_material_tests.c build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

test: build/component_material_tests
test_quarter_native: build/quarter_native_component_material_tests
build/quarter_native_component_material_tests: tests/vulkan/component_material_tests.c Makefile build/quarter_native_solver_tests $(GPU_SRC) src/world/chunk.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

build/component_extraction_tests: tests/vulkan/component_extraction_tests.c build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/shaders/component_extract.comp.spv: shaders/sim/component_extract.comp shaders/sim/rigid_body.glsl shaders/sim/pixel_properties.glsl include/dungeoncraft/terrain_flags.h
	mkdir -p build/shaders
	glslangValidator -V --target-env vulkan1.3 $< -o $@
build/shaders/component_extract.comp.spv $(STRUCTURAL_SHADER) build/shaders/scene.comp.spv: shaders/sim/shape_colors.glsl
build/fluid_tests: build/shaders/component_extract.comp.spv
build/loose_particle_role_tests: tests/vulkan/loose_particle_role_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

test: build/loose_particle_role_tests

build/shaders/component_local.comp.spv: shaders/sim/component_local.comp shaders/sim/rigid_body.glsl include/dungeoncraft/terrain_flags.h
	mkdir -p build/shaders
	glslangValidator -V --target-env vulkan1.3 $< -o $@

$(GPU_OBJ) build/fluid_tests: build/shaders/component_local.comp.spv
build/shaders/pixel_hydro.comp.spv: shaders/sim/pixel_hydro.comp shaders/sim/rigid_body.glsl shaders/sim/rigid_clip.glsl shaders/sim/rigid_hydro.glsl shaders/sim/structural_load.glsl
	mkdir -p build/shaders
	glslangValidator -V --target-env vulkan1.3 $< -o $@

$(GPU_OBJ) build/fluid_tests: build/shaders/pixel_hydro.comp.spv

build/component_gpu.o: src/vulkan/component_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(GPU_OBJ): build/shaders/component_extract.comp.spv

test: build/component_extraction_tests
test_quarter_native: build/quarter_native_component_extraction_tests
build/quarter_native_component_extraction_tests: tests/vulkan/component_extraction_tests.c Makefile build/quarter_native_solver_tests $(GPU_SRC) src/world/chunk.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

build/component_world_tests: tests/app/component_world_tests.c Makefile src/app/level.h src/app/view_config.h $(filter-out src/app/main.c,$(NATIVE_SRC)) build/quarter_native_rigid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(filter-out src/app/main.c,$(NATIVE_SRC)) -o $@ $(LDLIBS)
build/level03_support_tests: tests/app/level03_support_tests.c Makefile src/app/level.h src/app/view_config.h src/vulkan/gpu_internal.h src/vulkan/contact_shader_contract.h $(CONTACT_SHADER) $(MPM_SHADER) $(SOLVER_SHADER) $(filter-out src/app/main.c,$(NATIVE_SRC)) build/quarter_native_rigid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(filter-out src/app/main.c,$(NATIVE_SRC)) -o $@ $(LDLIBS) -Wl,--wrap=dc_gpu_read_contacts
test_ui: build/level03_support_tests

build/component_acceptance_tests: tests/vulkan/component_acceptance_tests.c build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

test: build/component_acceptance_tests
test_ui: build/component_world_tests
test_quarter_native: build/component_world_tests build/quarter_native_component_acceptance_tests
build/quarter_native_component_acceptance_tests: tests/vulkan/component_acceptance_tests.c Makefile build/quarter_native_solver_tests $(GPU_SRC) src/world/chunk.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

build/structural_visual_scene_tests: tests/structural/visual_scene_tests.c tests/structural/placement_tests.c Makefile $(GPU_SRC) src/world/chunk.c build/quarter_native_solver_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) -DDC_STRUCTURAL_PRESENTATION_TEST $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

build/structural_composition_fracture_tests: tests/structural/composition_fracture_tests.c tests/structural/placement_tests.c Makefile include/ferrum/structural/gpu.h $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/structural_shadow_chunk_store_tests: tests/structural/shadow_chunk_store_tests.c tests/structural/placement_tests.c Makefile $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/shaders/pattern.comp.spv: shaders/sim/rigid_body.glsl shaders/sim/rigid_shape.glsl include/dungeoncraft/terrain_flags.h

build/structural_composition_energy_tests: tests/structural/composition_fracture_energy_tests.c tests/structural/placement_tests.c Makefile $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

test: build/structural_shadow_chunk_store_tests build/structural_composition_fracture_tests build/structural_composition_energy_tests
test_ui test_quarter_native: build/structural_visual_scene_tests

build/structural_demo_controls_tests: tests/app/structural_demo_controls_tests.c Makefile build/dungeoncraft_quarter_native
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

build/structural_submerged_load_tests: tests/structural/submerged_load_tests.c tests/structural/placement_tests.c Makefile $(GPU_OBJ) $(CHUNK_OBJ) build/compound_body_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

test_ui test_quarter_native: build/structural_demo_controls_tests
test: build/structural_submerged_load_tests

build/structural_material_visual_tests: tests/structural/material_visual_tests.c tests/structural/placement_tests.c Makefile $(GPU_SRC) src/world/chunk.c build/quarter_native_solver_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) -DDC_STRUCTURAL_PRESENTATION_TEST $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

test_ui test_quarter_native: build/structural_material_visual_tests
test_quarter_native: build/quarter_native_structural_submerged_load_tests
build/quarter_native_structural_submerged_load_tests: tests/structural/submerged_load_tests.c tests/structural/placement_tests.c Makefile $(GPU_SRC) src/world/chunk.c build/quarter_native_solver_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

build/shaders/mpm_active.comp.spv: shaders/sim/rigid_body.glsl include/dungeoncraft/material_density.h
$(MPM_ACTIVITY_SHADER): shaders/sim/mpm_rigid_cache.glsl
$(MPM_SHADER) $(MPM_COMPONENT_SHADER) $(MPM_ACTIVITY_SHADER): include/dungeoncraft/mpm_activity_layout.h

build/pixel_component_tests: tests/vulkan/pixel_component_tests.c Makefile $(GPU_OBJ) $(CHUNK_OBJ) build/component_extraction_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/pixel_gpu.o: src/vulkan/pixel_gpu.c src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/pixel_geometry_tests: tests/vulkan/pixel_geometry_tests.c Makefile $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=vkUpdateDescriptorSets

build/pixel_world_tests: tests/vulkan/pixel_world_tests.c tests/vulkan/pixel_component_tests.c Makefile $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/pixel_dynamics_visual_tests: tests/app/pixel_dynamics_visual_tests.c tests/vulkan/pixel_component_tests.c Makefile $(GPU_SRC) src/world/chunk.c build/quarter_native_solver_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

build/pixel_performance_tests: tests/vulkan/pixel_performance_tests.c Makefile $(GPU_SRC) src/world/chunk.c build/pixel_component_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

.PHONY: test_pixel_performance
test_pixel_performance: build/pixel_performance_tests
	./build/pixel_performance_tests

test: build/pixel_component_tests build/pixel_world_tests build/pixel_geometry_tests
test_ui test_quarter_native: build/pixel_dynamics_visual_tests
build/native_720_component_tests: tests/vulkan/native_720_component_tests.c tests/vulkan/pixel_component_tests.c Makefile $(GPU_SRC) src/world/chunk.c build/pixel_component_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NATIVE_720_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS) -lm

.PHONY: test_native_720_components
test_native_720_components: build/native_720_component_tests
	./build/native_720_component_tests

build/native_720_performance_tests: tests/app/native_720_performance_tests.c tests/vulkan/pixel_component_tests.c tests/vulkan/native_scene.h Makefile $(GPU_SRC) src/world/chunk.c build/pixel_component_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NATIVE_720_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS) -lm

.PHONY: test_native_720_performance
test_native_720_performance: build/native_720_performance_tests
	./build/native_720_performance_tests

build/native_360_performance_tests: tests/app/native_720_performance_tests.c tests/vulkan/pixel_component_tests.c tests/vulkan/native_scene.h Makefile $(GPU_SRC) src/world/chunk.c build/pixel_component_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_GPU_CHUNK_SLOTS=90u -DNATIVE_PERF_WIDTH=640 -DNATIVE_PERF_HEIGHT=360 $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS)

.PHONY: test_native_360_performance
test_native_360_performance: build/native_360_performance_tests
	./build/native_360_performance_tests

.PHONY: test_native_360_sustained_performance
test_native_360_sustained_performance: build/native_360_performance_tests
	DC_NATIVE_SUSTAINED=1 ./build/native_360_performance_tests

build/pixel_motion_cache_tests: tests/vulkan/pixel_motion_cache_tests.c $(GPU_OBJ) build/chunk.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS) -lm

.PHONY: test_pixel_motion_cache
test_pixel_motion_cache: build/pixel_motion_cache_tests
	./build/pixel_motion_cache_tests

test: build/pixel_motion_cache_tests

build/diagnostics/component_extract_probe.spv: shaders/sim/component_extract.comp shaders/sim/rigid_body.glsl shaders/sim/pixel_properties.glsl
	mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.1 -S comp -o $@ $<

build/component_dispatch_tests: tests/vulkan/component_dispatch_tests.c tools/diagnostics/vulkan_probe.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/component_extract_probe.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

test: build/component_dispatch_tests

build/diagnostics/engine_init_probe: tools/diagnostics/engine_init_probe.c tools/diagnostics/engine_trace.c $(GPU_OBJ) $(CHUNK_OBJ) build/component_extraction_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/diagnostics/engine_init_probe.c tools/diagnostics/engine_trace.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=dc_gpu_load_shader_module -Wl,--wrap=vkCreateComputePipelines -Wl,--wrap=vkQueueSubmit

build/diagnostics/contact_scaling: tools/diagnostics/contact_scaling.c tests/vulkan/pixel_component_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/pixel_component_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/diagnostics/contact_scaling.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=dc_gpu_load_shader_module

build/diagnostics/contacts_terrain_scan.comp.spv: shaders/sim/contacts.comp shaders/sim/contact_geometry.glsl shaders/sim/rigid_body.glsl $(CONTACT_SHADER)
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_TERRAIN_SCAN_PROFILE=1 -o $@ $<
build/diagnostics/runtime_contacts_terrain_scan.comp.spv: shaders/sim/contacts.comp $(wildcard shaders/sim/*.glsl) include/dungeoncraft/body_storage_layout.h
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_TERRAIN_SCAN_PROFILE=1 -DDC_RUNTIME_BODY_STORAGE=1 -o $@ $<
build/contact_terrain_work_tests build/diagnostics/contact_scaling: build/diagnostics/runtime_contacts_terrain_scan.comp.spv

build/diagnostics/fluid_transport_profile.comp.spv: shaders/sim/fluid_transport.comp shaders/sim/fluid_aperture.glsl
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_TRANSPORT_PROFILE=1 -o $@ $<

build/fluid_transport_partial_tests: tests/vulkan/fluid_transport_partial_tests.c tests/vulkan/fluid_transport_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/fluid_transport_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/contact_terrain_work_tests: tests/vulkan/contact_terrain_work_tests.c tools/diagnostics/contact_scaling.c tests/vulkan/pixel_component_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/pixel_component_tests build/diagnostics/contacts_terrain_scan.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_CONTACT_TERRAIN_PROFILE $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=dc_gpu_load_shader_module

.PHONY: test_contact_terrain_work
test_contact_terrain_work: build/contact_terrain_work_tests
	./build/contact_terrain_work_tests

test: build/contact_terrain_work_tests

build/diagnostics/native360_fluid_trace: tests/app/native_720_performance_tests.c tools/diagnostics/fluid_dispatch_trace.c $(GPU_SRC) src/world/chunk.c build/pixel_component_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_GPU_CHUNK_SLOTS=90u -DNATIVE_PERF_WIDTH=640 -DNATIVE_PERF_HEIGHT=360 tests/app/native_720_performance_tests.c tools/diagnostics/fluid_dispatch_trace.c $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS) -Wl,--wrap=dc_gpu_tick_capture -Wl,--wrap=vkBeginCommandBuffer -Wl,--wrap=vkCmdBindPipeline -Wl,--wrap=vkCmdPushConstants -Wl,--wrap=vkCmdDispatch

build/diagnostics/native360_frame_trace: tests/app/native_720_performance_tests.c tools/diagnostics/frame_trace.c $(GPU_SRC) src/world/chunk.c build/pixel_component_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_GPU_CHUNK_SLOTS=90u -DNATIVE_PERF_WIDTH=640 -DNATIVE_PERF_HEIGHT=360 tests/app/native_720_performance_tests.c tools/diagnostics/frame_trace.c $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS) $(FRAME_TRACE_LINK)

build/diagnostics/pixel_properties_probe.spv: tests/vulkan/pixel_properties_probe.comp shaders/sim/rigid_body.glsl shaders/sim/pixel_properties.glsl
	mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.1 -S comp -Ishaders/sim -o $@ $<

build/diagnostics/rigid_prepare_probe.spv: shaders/sim/rigid.comp shaders/sim/rigid_body.glsl shaders/sim/rigid_shape.glsl shaders/sim/rigid_clip.glsl
	mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.1 -S comp -o $@ $<

build/pixel_properties_gpu_tests: tests/vulkan/pixel_properties_gpu_tests.c tools/diagnostics/vulkan_probe.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/pixel_properties_probe.spv build/diagnostics/rigid_prepare_probe.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

test: build/pixel_properties_gpu_tests

.PHONY: test_contact_barrier
test_contact_barrier:
	python3 tests/vulkan/contact_barrier_tests.py

test: test_contact_barrier

build/hydro_head_tests: tests/vulkan/hydro_head_tests.c tools/diagnostics/vulkan_probe.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/hydro_head.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

test: build/hydro_head_tests

build/shaders/fluid_aperture.comp.spv: shaders/sim/fluid_aperture.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@

build/shaders/fluid_aperture_query.comp.spv: tests/vulkan/fluid_aperture_query.comp shaders/sim/fluid_aperture.glsl
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@

build/fluid_aperture_tests: tests/vulkan/fluid_aperture_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/fluid_aperture.comp.spv build/shaders/fluid_aperture_query.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/shaders/fluid_transport.comp.spv: shaders/sim/fluid_transport.comp shaders/sim/fluid_aperture.glsl Makefile
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@

build/fluid_transport_tests: tests/vulkan/fluid_transport_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/fluid_transport.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/water_color_tests: tests/vulkan/water_color_tests.c tests/vulkan/fluid_transport_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/fluid_transport.comp.spv build/shaders/fluid_displacement.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/water_color_tests

$(FLUID_SHADER) build/shaders/fluid_transport.comp.spv build/shaders/fluid_aperture.comp.spv build/diagnostics/fluid_transport_profile.comp.spv: shaders/sim/hydraulic_face.glsl
$(FLUID_SHADER) build/shaders/fluid_transport.comp.spv build/shaders/fluid_displacement.comp.spv build/diagnostics/fluid_transport_profile.comp.spv: shaders/sim/water_color.glsl

build/fluid_transport_long_tests: tests/vulkan/fluid_transport_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/fluid_transport.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -DTRANSPORT_TEST_WIDTH=2113 -DTRANSPORT_TEST_HEIGHT=3 $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/fluid_transport_slow_tests: tests/vulkan/fluid_transport_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/fluid_transport.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -DTRANSPORT_TEST_STEP=0.125f $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/raster_dispatch_tests: tests/vulkan/raster_dispatch_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) $(RIGID_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/raster_equivalence_tests: tests/vulkan/raster_equivalence_tests.c tests/vulkan/pixel_component_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/pixel_component_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/shaders/fluid_displacement.comp.spv: shaders/sim/fluid_displacement.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@

build/fluid_displacement_tests: tests/vulkan/fluid_displacement_tests.c tests/vulkan/fluid_transport_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/fluid_displacement.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/diagnostics/fluid_displacement_profile.comp.spv: shaders/sim/fluid_displacement.comp shaders/sim/water_color.glsl
	@mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_DISPLACEMENT_PROFILE=1 $< -o $@

build/fluid_displacement_work_tests: tests/vulkan/fluid_displacement_tests.c tests/vulkan/fluid_transport_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/fluid_displacement_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_DISPLACEMENT_PROFILE=1 $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

test: build/fluid_displacement_work_tests

build/fluid_transport_work_tests: tests/vulkan/fluid_transport_work_tests.c tests/vulkan/fluid_transport_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/fluid_transport.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

.PHONY: test_fluid_transport_work
test_fluid_transport_work: build/fluid_transport_work_tests
	./build/fluid_transport_work_tests

build/fluid_displacement_wide_tests: tests/vulkan/fluid_displacement_tests.c tests/vulkan/fluid_transport_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/fluid_displacement.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -DTRANSPORT_TEST_WIDTH=2113 -DTRANSPORT_TEST_HEIGHT=3 $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/fluid_displacement_tall_tests: tests/vulkan/fluid_displacement_tests.c tests/vulkan/fluid_transport_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/fluid_displacement.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -DTRANSPORT_TEST_WIDTH=3 -DTRANSPORT_TEST_HEIGHT=2113 $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

test: build/fluid_aperture_tests build/diagnostics/fluid_case
test: build/fluid_transport_tests build/fluid_transport_long_tests build/fluid_transport_slow_tests
test: build/raster_dispatch_tests build/raster_equivalence_tests
test: build/contact_dispatch_tests

build/contact_dispatch_tests: tests/vulkan/contact_dispatch_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) $(CONTACT_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/fluid_displacement_tests build/fluid_displacement_wide_tests build/fluid_displacement_tall_tests

build/diagnostics/fluid_case: tools/diagnostics/fluid_case.c tests/vulkan/fluid_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=vkCmdBindPipeline -Wl,--wrap=vkCmdPushConstants -Wl,--wrap=vkCmdDispatch

build/diagnostics/shader_compile_probe: tools/diagnostics/shader_compile_probe.c tools/diagnostics/vulkan_probe.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ -lvulkan -Wl,--wrap=vkQueueSubmit -Wl,--wrap=vkQueueSubmit2

.PHONY: diagnostic_fluid_variants
.PHONY: test_gpu_tools
test: test_gpu_tools
test_gpu_tools: build/contact_shader_contract_tests
	python3 tests/vulkan/shader_variant_tests.py
	python3 tests/vulkan/gpu_dependency_tests.py
	./build/contact_shader_contract_tests
	./build/fluid_transport_limits_tests
	./build/contact_recording_tests
	./build/mpm_layout_tests
	./build/probe_feature_tests
	./build/fluid_recording_tests
	./build/native_demo_controls_tests
	./build/extraction_stats_tests
	./build/body_address_feature_tests
	./build/body_address_allocation_tests
	./build/body_storage_layout_tests
	./build/body_storage_growth_tests
	./build/body_features_tests
	./build/body_scan_plan_tests
	./build/body_scan_recording_tests

build/contact_shader_contract_tests: tests/vulkan/contact_shader_contract_tests.c src/vulkan/contact_shader_contract.h $(CONTACT_SHADER)
	@mkdir -p build/diagnostics
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

MPM_VARIANT_WRAPS = -Wl,--wrap=dc_gpu_record_mpm -Wl,--wrap=dc_gpu_destroy -Wl,--wrap=vkCmdBindPipeline -Wl,--wrap=vkCmdPushConstants
build/diagnostics/mpm_case: tests/vulkan/mpm_specialization_tests.c tools/diagnostics/mpm_variants.c tests/vulkan/particle_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ) $(STREAM_OBJ) diagnostic_fluid_variants
	@mkdir -p build/diagnostics
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/vulkan/mpm_specialization_tests.c tools/diagnostics/mpm_variants.c $(GPU_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ) $(STREAM_OBJ) -o $@ $(LDLIBS) $(MPM_VARIANT_WRAPS)

build/diagnostics/native360_mpm_variants: tests/app/native_720_performance_tests.c tests/vulkan/pixel_component_tests.c tests/vulkan/native_scene.h tools/diagnostics/mpm_variants.c $(GPU_SRC) src/world/chunk.c diagnostic_fluid_variants
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_GPU_CHUNK_SLOTS=90u -DNATIVE_PERF_WIDTH=640 -DNATIVE_PERF_HEIGHT=360 tests/app/native_720_performance_tests.c tools/diagnostics/mpm_variants.c $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS) $(MPM_VARIANT_WRAPS)

diagnostic_fluid_variants: build/diagnostics/shader_compile_probe
	python3 tools/diagnostics/fluid_variants.py build/diagnostics/fluid-variants
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -Ishaders/sim -o build/diagnostics/fluid-variants/fluid.spv build/diagnostics/fluid-variants/fluid_specialized.comp
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o build/diagnostics/fluid-variants/projection.spv build/diagnostics/fluid-variants/projection_specialized.comp
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -Ishaders/sim -o build/diagnostics/fluid-variants/mpm.spv build/diagnostics/fluid-variants/mpm_specialized.comp
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -Ishaders/sim -o build/diagnostics/fluid-variants/pattern.spv build/diagnostics/fluid-variants/pattern_specialized.comp
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -Ishaders/sim -o build/diagnostics/fluid-variants/contacts_pixel.spv build/diagnostics/fluid-variants/contacts_pixel.comp
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -Ishaders/sim -o build/diagnostics/fluid-variants/rigid_pixel.spv build/diagnostics/fluid-variants/rigid_pixel.comp
	spirv-val --target-env vulkan1.3 build/diagnostics/fluid-variants/fluid.spv
	spirv-val --target-env vulkan1.3 build/diagnostics/fluid-variants/projection.spv
	spirv-val --target-env vulkan1.3 build/diagnostics/fluid-variants/mpm.spv
	spirv-val --target-env vulkan1.3 build/diagnostics/fluid-variants/pattern.spv
	spirv-val --target-env vulkan1.3 build/diagnostics/fluid-variants/contacts_pixel.spv
	spirv-val --target-env vulkan1.3 build/diagnostics/fluid-variants/rigid_pixel.spv

GPU_SOURCE_BINARIES = $(addprefix build/, \
    component_world_tests diagnostics/native360_fluid_trace diagnostics/native360_frame_trace \
    diagnostics/native360_mpm_variants dungeoncraft_half_native \
    dungeoncraft_native dungeoncraft_quarter_native marker_slot_tests \
    material_cut_buoyancy_tests dynamic_body_capacity_tests \
    native_360_performance_tests native_720_component_tests \
    native_720_performance_tests native_bench pixel_dynamics_visual_tests \
    pixel_performance_tests structural_bench structural_cadence_tests \
    structural_material_visual_tests structural_visual_scene_tests \
    structural_world_tests)
GPU_SOURCE_BINARIES += $(addprefix build/quarter_native_, \
    bench buoyancy_tests component_acceptance_tests component_extraction_tests \
    component_material_tests fluid_tests rigid_solver_tests rigid_tests solver_tests)
GPU_SOURCE_BINARIES += $(addprefix build/quarter_native_structural_, \
    buckling_tests equilibrium_tests failure_tests fracture_tests impact_tests \
    particle_identity_tests placement_tests root_identity_tests store_tests \
    streaming_tests submerged_load_tests)
GPU_SOURCE_BINARIES += build/material_cut_buoyancy_tests build/renderer_visual_tests build/renderer_performance_tests
$(GPU_SOURCE_BINARIES): $(GPU_SRC) src/vulkan/gpu_internal.h src/vulkan/contact_shader_contract.h src/vulkan/fluid_transport_limits.h src/vulkan/mpm_activity_layout.h include/dungeoncraft/mpm_activity_layout.h $(CONTACT_SHADER) $(MPM_COMPACTION_SHADER) $(MPM_SHADER) $(MPM_COMPONENT_SHADER) $(MPM_ACTIVITY_SHADER)
$(GPU_SOURCE_BINARIES): $(RENDERER_SHADERS) build/shaders/prop_place.comp.spv build/shaders/authored.comp.spv include/ferrum/assets/prop.h include/ferrum/assets/prop_gpu.h src/vulkan/renderer_internal.h
$(GPU_SOURCE_BINARIES) $(GPU_OBJ) $(STREAM_OBJ): include/dungeoncraft/color.h
$(GPU_OBJ): src/vulkan/mpm_activity_layout.h include/dungeoncraft/mpm_activity_layout.h
build/contact_gpu.o: src/vulkan/contact_shader_contract.h $(CONTACT_SHADER)
build/fluid_gpu.o: src/vulkan/fluid_transport_limits.h

build/fluid_transport_limits_tests: tests/vulkan/fluid_transport_limits_tests.c src/vulkan/fluid_transport_limits.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
test_gpu_tools: build/fluid_transport_limits_tests
build/contact_recording_tests: tests/vulkan/contact_recording_tests.c build/contact_gpu.o build/shader.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $< build/contact_gpu.o build/shader.o -o $@ -lvulkan
test_gpu_tools: build/contact_recording_tests
build/mpm_compaction_tests: tests/vulkan/mpm_compaction_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) $(MPM_ACTIVITY_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/diagnostics/mpm_control_schedule.spv: shaders/sim/mpm_component.comp tools/diagnostics/mpm_control_schedule.py
	mkdir -p build/diagnostics
	python3 tools/diagnostics/mpm_control_schedule.py $< build/diagnostics/mpm_control_schedule.comp
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ build/diagnostics/mpm_control_schedule.comp
	spirv-val --target-env vulkan1.3 $@
build/mpm_control_tests: tests/vulkan/mpm_control_tests.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) $(MPM_COMPONENT_SHADER) build/diagnostics/mpm_control_schedule.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/diagnostics/particle_case: tools/diagnostics/particle_case.c tests/vulkan/particle_tests.c build/particle_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) $(GENERATE_OBJ) $(STREAM_OBJ) -o $@ $(LDLIBS)
test: build/mpm_control_tests
build/mpm_layout_tests: tests/vulkan/mpm_layout_tests.c src/vulkan/mpm_activity_layout.h include/dungeoncraft/mpm_activity_layout.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
test_gpu_tools: build/mpm_layout_tests

build/probe_feature_tests: tests/vulkan/probe_feature_tests.c tools/diagnostics/vulkan_probe.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ -lvulkan
.PHONY: test_probe_features
test_probe_features: build/probe_feature_tests
	./build/probe_feature_tests
test_gpu_tools: build/probe_feature_tests

MINIMAL_VK_TESTS = $(addprefix build/,component_dispatch_tests pixel_properties_gpu_tests \
    fluid_transport_work_tests raster_dispatch_tests fluid_transport_tests fluid_transport_long_tests water_color_tests \
    fluid_transport_slow_tests fluid_transport_partial_tests fluid_color_sync_tests mpm_control_tests mpm_compaction_tests fluid_displacement_tests \
    fluid_displacement_wide_tests fluid_displacement_tall_tests fluid_displacement_work_tests hydro_head_tests fluid_aperture_tests \
    contact_dispatch_tests)
$(MINIMAL_VK_TESTS): tools/diagnostics/vulkan_probe.c tools/diagnostics/shader_fixture.h
test: build/mpm_compaction_tests
test: build/fluid_transport_partial_tests
build/diagnostics/fluid_transport_color_reference.comp.spv: shaders/sim/fluid_transport.comp shaders/sim/fluid_aperture.glsl shaders/sim/water_color.glsl
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_TRANSPORT_PROFILE -DDC_TRANSPORT_CONSERVATIVE_COLOR_SYNC -o $@ $<
	spirv-val --target-env vulkan1.3 $@
build/fluid_color_sync_tests: tests/vulkan/fluid_transport_partial_tests.c tests/vulkan/fluid_transport_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/fluid_transport_profile.comp.spv build/diagnostics/fluid_transport_color_reference.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_FIXTURE_WATER_COLOR -DDC_COLOR_SYNC_PROFILE $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/fluid_color_sync_tests
build/fluid_recording_tests: tests/vulkan/fluid_recording_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=vkCmdBindPipeline -Wl,--wrap=vkCmdPushConstants -Wl,--wrap=vkCmdDispatch -Wl,--wrap=vkCmdPipelineBarrier2 -Wl,--wrap=vkCmdBindDescriptorSets
test_gpu_tools: build/fluid_recording_tests
build/native_demo_controls_tests: tests/app/native_demo_controls_tests.c tests/app/native_demo_controls.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
test_gpu_tools: build/native_demo_controls_tests
build/pixel_residency_tests: tests/vulkan/pixel_residency_tests.c tests/vulkan/pixel_component_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
build/material_cut_buoyancy_tests: tests/app/material_cut_buoyancy_tests.c src/app/level.h src/app/view_config.h $(filter-out src/app/main.c,$(NATIVE_SRC))
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(filter-out src/app/main.c,$(NATIVE_SRC)) -o $@ $(LDLIBS)
build/extraction_stats_tests: tests/vulkan/extraction_stats_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=vkQueueWaitIdle
build/dynamic_body_capacity_tests: tests/app/dynamic_body_capacity_tests.c tests/app/material_cut_buoyancy_tests.c src/app/level.h src/app/view_config.h $(filter-out src/app/main.c,$(NATIVE_SRC))
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(filter-out src/app/main.c,$(NATIVE_SRC)) -o $@ $(LDLIBS)
build/body_address_feature_tests: tests/vulkan/probe_feature_tests.c tools/diagnostics/vulkan_probe.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_PROBE_BUFFER_DEVICE_ADDRESS $< -o $@ -lvulkan
build/body_address_allocation_tests: tests/vulkan/body_address_allocation_tests.c src/vulkan/body_address_gpu.c src/vulkan/body_address_gpu.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/body_address_gpu.c -o $@ -lvulkan -Wl,--wrap=vkCreateBuffer -Wl,--wrap=vkGetBufferMemoryRequirements -Wl,--wrap=vkGetPhysicalDeviceMemoryProperties -Wl,--wrap=vkAllocateMemory -Wl,--wrap=vkBindBufferMemory -Wl,--wrap=vkGetBufferDeviceAddress -Wl,--wrap=vkDestroyBuffer -Wl,--wrap=vkFreeMemory
build/diagnostics/body_address_probe.spv: tests/vulkan/body_address_probe.comp shaders/sim/rigid_body.glsl shaders/sim/rigid_storage.glsl include/dungeoncraft/body_storage_layout.h | build
	mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<
	spirv-val --target-env vulkan1.3 $@
build/body_address_gpu_tests: tests/vulkan/body_address_gpu_tests.c tests/vulkan/body_probe_fixture.h tools/diagnostics/vulkan_probe.c src/vulkan/body_address_gpu.c src/vulkan/body_address_gpu.h src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h include/dungeoncraft/body_storage_layout.h build/diagnostics/body_address_probe.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/body_address_gpu.c -o $@ -lvulkan
build/body_storage_layout_tests: tests/vulkan/body_storage_layout_tests.c include/dungeoncraft/body_storage_layout.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
build/body_features_tests: tests/vulkan/body_features_tests.c src/vulkan/body_features_gpu.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ -lvulkan -Wl,--wrap=vkGetPhysicalDeviceFeatures2
build/body_scan_plan_tests: tests/vulkan/body_scan_plan_tests.c src/vulkan/body_scan_gpu.c src/vulkan/body_scan_gpu.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/body_scan_gpu.c -o $@
build/body_scan_recording_tests: tests/vulkan/body_scan_recording_tests.c src/vulkan/body_scan_gpu.c src/vulkan/body_scan_record_gpu.c src/vulkan/body_scan_gpu.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/body_scan_gpu.c src/vulkan/body_scan_record_gpu.c -o $@ -lvulkan -Wl,--wrap=vkCmdBindPipeline -Wl,--wrap=vkCmdPushConstants -Wl,--wrap=vkCmdDispatch -Wl,--wrap=vkCmdDispatchIndirect -Wl,--wrap=vkCmdPipelineBarrier2
test_gpu_tools: build/body_scan_recording_tests
test_gpu_tools: build/body_scan_plan_tests
build/gpu.o: src/vulkan/body_features_gpu.h
$(GPU_SOURCE_BINARIES): src/vulkan/body_features_gpu.h
test_gpu_tools: build/body_features_tests
build/body_storage_growth_tests: tests/vulkan/body_storage_growth_tests.c src/vulkan/body_storage_gpu.c src/vulkan/body_storage_gpu.h include/dungeoncraft/body_storage_layout.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/body_storage_gpu.c -o $@ -lvulkan -Wl,--wrap=dc_gpu_address_buffer_create -Wl,--wrap=dc_gpu_address_buffer_destroy -Wl,--wrap=vkCmdCopyBuffer -Wl,--wrap=vkCmdFillBuffer -Wl,--wrap=vkCmdUpdateBuffer -Wl,--wrap=vkCmdPipelineBarrier2 -Wl,--wrap=vkCmdBindPipeline -Wl,--wrap=vkCmdPushConstants -Wl,--wrap=vkCmdDispatch
test_gpu_tools: build/extraction_stats_tests build/body_address_feature_tests build/body_address_allocation_tests build/body_storage_layout_tests build/body_storage_growth_tests
build/diagnostics/body_growth_probe.spv: tests/vulkan/body_growth_probe.comp shaders/sim/rigid_body.glsl shaders/sim/rigid_storage.glsl include/dungeoncraft/body_storage_layout.h | build
	mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<
	spirv-val --target-env vulkan1.3 $@
build/body_storage_gpu_tests: tests/vulkan/body_storage_gpu_tests.c tests/vulkan/body_probe_fixture.h tools/diagnostics/vulkan_probe.c src/vulkan/body_storage_gpu.c src/vulkan/body_storage_gpu.h src/vulkan/body_address_gpu.c src/vulkan/body_address_gpu.h src/vulkan/shader.c include/dungeoncraft/body_storage_layout.h build/diagnostics/body_growth_probe.spv build/diagnostics/body_storage_init.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/body_storage_gpu.c src/vulkan/body_address_gpu.c src/vulkan/shader.c -o $@ -lvulkan
build/diagnostics/body_scan.spv: shaders/sim/body_scan.comp | build
	mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<
	spirv-val --target-env vulkan1.3 $@
build/body_scan_gpu_tests: tests/vulkan/body_scan_gpu_tests.c tests/vulkan/body_probe_fixture.h tools/diagnostics/vulkan_probe.c src/vulkan/body_scan_gpu.h src/vulkan/body_scan_gpu.c src/vulkan/body_scan_record_gpu.c src/vulkan/body_address_gpu.h src/vulkan/body_address_gpu.c build/diagnostics/body_scan.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/body_scan_gpu.c src/vulkan/body_scan_record_gpu.c src/vulkan/body_address_gpu.c -o $@ -lvulkan
build/diagnostics/body_allocator.spv: shaders/sim/body_allocator.comp shaders/sim/rigid_body.glsl shaders/sim/rigid_storage.glsl include/dungeoncraft/body_storage_layout.h | build
	mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -Ishaders/sim -o $@ $<
	spirv-val --target-env vulkan1.3 $@
build/body_allocator_gpu_tests: tests/vulkan/body_allocator_gpu_tests.c tests/vulkan/body_probe_fixture.h tools/diagnostics/vulkan_probe.c src/vulkan/body_scan_gpu.h src/vulkan/body_scan_gpu.c src/vulkan/body_scan_record_gpu.c src/vulkan/body_storage_gpu.c src/vulkan/body_storage_gpu.h src/vulkan/body_address_gpu.c src/vulkan/body_address_gpu.h src/vulkan/shader.c include/dungeoncraft/body_storage_layout.h build/diagnostics/body_allocator.spv build/diagnostics/body_scan.spv build/diagnostics/body_storage_init.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/body_scan_gpu.c src/vulkan/body_scan_record_gpu.c src/vulkan/body_storage_gpu.c src/vulkan/body_address_gpu.c src/vulkan/shader.c -o $@ -lvulkan
test: build/body_address_gpu_tests build/body_storage_gpu_tests build/body_scan_gpu_tests build/body_page_init_gpu_tests
BODY_STORAGE_TEST_BINARIES = $(addprefix build/,body_address_gpu_tests body_storage_gpu_tests body_allocator_gpu_tests body_scan_gpu_tests body_address_allocation_tests body_storage_growth_tests)
$(BODY_STORAGE_TEST_BINARIES): src/vulkan/gpu_internal.h include/dungeoncraft/gpu.h
build/diagnostics/body_storage_init.spv: shaders/sim/body_storage_init.comp include/dungeoncraft/body_storage_layout.h | build
	mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<
	spirv-val --target-env vulkan1.3 $@
$(GPU_OBJ): build/diagnostics/body_storage_init.spv
build/shaders/runtime_body_index.comp.spv: shaders/sim/runtime_body_index.comp $(wildcard shaders/sim/*.glsl) include/dungeoncraft/body_storage_layout.h
	@mkdir -p $(dir $@)
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
$(GPU_OBJ): build/shaders/runtime_body_index.comp.spv
build/shaders/runtime_body_scan.comp.spv: shaders/sim/runtime_body_scan.comp $(wildcard shaders/sim/*.glsl) include/dungeoncraft/body_storage_layout.h
	@mkdir -p $(dir $@)
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
$(GPU_OBJ): build/shaders/runtime_body_scan.comp.spv

RUNTIME_BODY_NAMES = pattern rigid rigid_solver contacts broadphase component_extract component_local structural pixel_hydro mpm mpm_active mpm_compact tick_probe scene
RUNTIME_BODY_SHADERS = $(addprefix build/shaders/runtime/,$(addsuffix .comp.spv,$(RUNTIME_BODY_NAMES)))
$(GPU_OBJ): $(RUNTIME_BODY_SHADERS)
$(GPU_SOURCE_BINARIES) build/dungeoncraft_360: $(RUNTIME_BODY_SHADERS) build/shaders/runtime_body_index.comp.spv build/shaders/runtime_body_scan.comp.spv build/diagnostics/body_storage_init.spv src/vulkan/runtime_body_gpu.h src/vulkan/runtime_body_internal.h
$(GPU_OBJ): src/vulkan/runtime_body_gpu.h src/vulkan/runtime_body_internal.h
build/shaders/runtime/%.comp.spv: shaders/sim/%.comp $(wildcard shaders/sim/*.glsl) include/dungeoncraft/body_storage_layout.h
	@mkdir -p $(dir $@)
	$(GLSLANG) -DDC_RUNTIME_BODY_STORAGE=1 -V --target-env vulkan1.3 $< -o $@
build/shaders/runtime/scene.comp.spv: shaders/renderer/scene.comp $(wildcard shaders/renderer/*.glsl) $(wildcard shaders/sim/*.glsl) include/dungeoncraft/body_storage_layout.h
	@mkdir -p $(dir $@)
	$(GLSLANG) -DDC_RUNTIME_BODY_STORAGE=1 -V --target-env vulkan1.3 $< -o $@
build/body_page_init_gpu_tests: tests/vulkan/body_page_init_gpu_tests.c tests/vulkan/body_probe_fixture.h tools/diagnostics/vulkan_probe.c src/vulkan/body_address_gpu.c src/vulkan/body_address_gpu.h src/vulkan/gpu_internal.h include/dungeoncraft/body_storage_layout.h build/diagnostics/body_storage_init.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/body_address_gpu.c -o $@ -lvulkan
build/diagnostics/body_work_gate.spv: shaders/sim/body_work_gate.comp | build
	mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<
	spirv-val --target-env vulkan1.3 $@
build/body_work_gate_gpu_tests: tests/vulkan/body_work_gate_gpu_tests.c tests/vulkan/body_probe_fixture.h tools/diagnostics/vulkan_probe.c src/vulkan/body_address_gpu.c src/vulkan/body_address_gpu.h src/vulkan/gpu_internal.h include/dungeoncraft/body_storage_layout.h build/diagnostics/body_work_gate.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/body_address_gpu.c -o $@ -lvulkan
shaders: $(MPM_COMPACTION_SHADER)
.PHONY: test_mpm_compaction_work
test_mpm_compaction_work: build/mpm_compaction_tests
	./build/mpm_compaction_tests performance

build/mpm_descriptor_tests: tests/vulkan/mpm_descriptor_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/gpu_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=vkUpdateDescriptorSets
test: build/mpm_descriptor_tests
build/diagnostics/mpm_scalar_reference_tests: tools/diagnostics/mpm_scalar_reference.py tools/diagnostics/vulkan_probe.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ)
	python3 tools/diagnostics/mpm_scalar_reference.py
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -Ishaders/sim -o build/diagnostics/mpm_scalar_reference.spv build/diagnostics/mpm_scalar_reference.comp
	spirv-val --target-env vulkan1.3 build/diagnostics/mpm_scalar_reference.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) build/diagnostics/mpm_scalar_reference_tests.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)

build/shaders/prop_place.comp.spv: shaders/renderer/prop_place.comp shaders/renderer/texture.glsl include/dungeoncraft/material_density.h include/dungeoncraft/terrain_flags.h
	mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
build/terrain_prop_tests: tests/vulkan/terrain_prop_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/prop_place.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
.PHONY: test_terrain_prop test_scene_placement
test_terrain_prop: build/terrain_prop_tests
	./build/terrain_prop_tests
test_scene_placement: build/dungeoncraft_quarter_native
	./build/dungeoncraft_quarter_native --smoke-scene assets/props/level_piece01.prop/asset.cprop
test: build/terrain_prop_tests
build/body_color_tests: tests/vulkan/body_color_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/body_color_tests
build/body_carve_tests: tests/vulkan/body_carve_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=vkAllocateMemory
test: build/body_carve_tests
build/sequential_detachment_tests: tests/vulkan/sequential_detachment_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/sequential_detachment_tests
build/dense_support_tests: tests/vulkan/dense_support_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(MPM_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/dense_support_tests
build/diagnostics/body_support: tools/diagnostics/body_support.c tools/diagnostics/terrain_support.h Makefile src/vulkan/gpu_internal.h src/vulkan/contact_shader_contract.h $(CONTACT_SHADER) $(GPU_SRC) $(GPU_OBJ) $(CHUNK_OBJ) src/world/chunk.c $(RENDERER_SHADERS) build/shaders/prop_place.comp.spv
	@mkdir -p build/diagnostics
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(GPU_SRC) src/world/chunk.c -o $@ $(LDLIBS) -Wl,--wrap=dc_gpu_record_contacts
build/scene_ground_tests: tests/world/scene_ground_tests.c $(GENERATE_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GENERATE_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/scene_ground_tests
build/dungeoncraft: src/app/prop_scene.c src/app/prop_scene.h

# Offline authoring and preloaded prop reader checks (no Vulkan device needed).
build/prop_tests: tests/assets/prop_tests.c src/assets/prop.c include/ferrum/assets/prop.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/assets/prop_tests.c src/assets/prop.c -o $@ -lm
.PHONY: test_prop_assets test_libresprite_host
test: test_prop_assets
test_prop_assets: build/prop_tests build/prop_channel_contract_tests
	python3 -m unittest discover -s tests/assets -p '*_test.py' -k 'SchemaTests'
	python3 tests/assets/package_test.py
	python3 tests/assets/reader_test.py
	python3 tests/assets/profile_test.py
	python3 tests/assets/png_pair_test.py
	python3 tests/assets/compiler_fixture.py
	./build/prop_tests build/prop_fixture/asset.cprop
	./build/prop_channel_contract_tests
build/prop_channel_contract_tests: tests/integration/prop_channel_contract_tests.c src/assets/prop.c include/ferrum/assets/prop.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/integration/prop_channel_contract_tests.c src/assets/prop.c -o $@ -lm
test_libresprite_host:
	python3 tests/assets/host_probe_test.py
	python3 tests/assets/export_test.py
	python3 tests/assets/host_example_test.py

build/dry_fluid_tests: tests/vulkan/dry_fluid_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=dc_gpu_record_fluid_phase
test: build/dry_fluid_tests

build/fluid_specialization_tests: tests/vulkan/fluid_specialization_tests.c tests/vulkan/fluid_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/fluid_specialization_tests

build/pressure_work_tests: tests/vulkan/pressure_work_tests.c tests/vulkan/fluid_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/pressure_work_tests

build/grain_bed_work_tests: tests/vulkan/grain_bed_work_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/grain_bed_work_tests
$(SOLVER_SHADER): include/dungeoncraft/mpm_activity_layout.h

build/dynamic_runtime_tests: tests/vulkan/dynamic_runtime_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/dynamic_runtime_tests
build/component_cache_tests: tests/vulkan/component_cache_tests.c tests/vulkan/component_extraction_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/component_cache_tests
build/albedo_work_tests: tests/renderer/albedo_work_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=fr_renderer_record -Wl,--wrap=vkCmdDispatch
test: build/albedo_work_tests
build/fluid_one_cell_scan_tests: tests/vulkan/fluid_one_cell_scan_tests.c tests/vulkan/fluid_transport_tests.c tools/diagnostics/vulkan_probe.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/fluid_transport.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/fluid_one_cell_scan_tests
build/shaders/fluid_transport.comp.spv build/diagnostics/fluid_transport_profile.comp.spv build/diagnostics/fluid_transport_color_reference.comp.spv: shaders/sim/fluid_one_cell.glsl Makefile

# A literal group size keeps older Vulkan feature configurations supported.
build/shaders/fluid_transport_16.comp.spv: shaders/sim/fluid_transport.comp shaders/sim/fluid_aperture.glsl shaders/sim/fluid_one_cell.glsl shaders/sim/hydraulic_face.glsl shaders/sim/water_color.glsl Makefile
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_TRANSPORT_GROUP_SIZE=16 $< -o $@
$(GPU_OBJ) build/diagnostics/quarter_app_frame_trace build/fluid_one_cell_scan_tests: build/shaders/fluid_transport_16.comp.spv

build/diagnostics/pixel_boundary_probe.spv: tests/vulkan/pixel_boundary_probe.comp shaders/sim/rigid_body.glsl
	mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.1 -S comp -Ishaders/sim -o $@ $<
build/pixel_boundary_tests: tests/vulkan/pixel_boundary_tests.c tests/vulkan/pixel_properties_gpu_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/pixel_properties_probe.spv build/diagnostics/pixel_boundary_probe.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/pixel_boundary_tests

build/contact_piece_work_tests: tests/vulkan/contact_piece_work_tests.c tools/diagnostics/contact_scaling.c tests/vulkan/pixel_component_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/pixel_component_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=dc_gpu_load_shader_module
test: build/contact_piece_work_tests

build/mpm_support_dirty_tests: tests/vulkan/mpm_support_dirty_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(MPM_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/mpm_support_dirty_tests

build/mpm_gather_tests: tests/vulkan/mpm_gather_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(MPM_SHADER)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/mpm_gather_tests

# Profiled literal-size transport variants exercise the exact cooperative path.
build/diagnostics/fluid_transport_profile_16.comp.spv: shaders/sim/fluid_transport.comp shaders/sim/fluid_one_cell.glsl shaders/sim/fluid_aperture.glsl shaders/sim/hydraulic_face.glsl shaders/sim/water_color.glsl Makefile
	@mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_TRANSPORT_PROFILE=1 -DDC_TRANSPORT_GROUP_SIZE=16 $< -o $@
build/fluid_one_cell_scan_tests: build/diagnostics/fluid_transport_profile.comp.spv build/diagnostics/fluid_transport_profile_16.comp.spv

build/shaders/fluid_transport.comp.spv build/shaders/fluid_transport_16.comp.spv build/diagnostics/fluid_transport_profile.comp.spv build/diagnostics/fluid_transport_profile_16.comp.spv build/diagnostics/fluid_transport_color_reference.comp.spv: shaders/sim/fluid_short_run.glsl

build/shaders/mpm_copy.comp.spv: shaders/sim/mpm_copy.comp
	@mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 -S comp -o $@ $<
build/mpm_copy_tests: tests/vulkan/mpm_copy_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/shaders/mpm_copy.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/mpm_copy_tests

build/fluid_gpu.o: src/vulkan/subgroup_gpu.h
build/fluid_one_cell_scan_tests: src/vulkan/subgroup_gpu.h tools/diagnostics/vulkan_probe.c

build/diagnostics/solver_motion_profile.comp.spv: shaders/sim/rigid_solver.comp shaders/sim/rigid_body.glsl
	@mkdir -p build/diagnostics
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_SOLVER_MOTION_PROFILE=1 -S comp -o $@ $<
build/solver_motion_store_tests: tests/vulkan/solver_motion_store_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/solver_motion_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/solver_motion_store_tests

build/solver_recording_tests: tests/vulkan/solver_recording_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/solver_recording_tests

build/solver_commands_gpu.o: src/vulkan/solver_commands_gpu.c src/vulkan/gpu_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/solver_commands_tests: tests/vulkan/solver_commands_tests.c $(GPU_OBJ) $(CHUNK_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/solver_commands_tests

build/diagnostics/mpm_gather_profile.comp.spv: shaders/sim/mpm.comp $(wildcard shaders/sim/*.glsl)
	@mkdir -p build/diagnostics
	$(GLSLANG) -DDC_MPM_SHARED_GATHER=1 -DDC_RUNTIME_BODY_STORAGE=1 -DDC_MPM_GATHER_PROFILE=1 -V --target-env vulkan1.3 -S comp -o $@ $<
build/mpm_gather_tests: build/diagnostics/mpm_gather_profile.comp.spv

build/mpm_gather_partial_tile_tests: tests/vulkan/mpm_gather_tests.c $(GPU_OBJ) $(CHUNK_OBJ) $(MPM_SHADER) build/diagnostics/mpm_gather_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDC_GATHER_TEST_WIDTH=137u -DDC_GATHER_TEST_HEIGHT=83u $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/mpm_gather_partial_tile_tests

$(MPM_SHADER) build/shaders/runtime/mpm.comp.spv: shaders/sim/mpm_gather_cache.glsl

build/mpm_shader_storage_tests: tests/vulkan/mpm_shader_storage_tests.c $(MPM_SHADER) build/diagnostics/mpm_gather_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
test: build/mpm_shader_storage_tests

build/shaders/mpm_stencil.comp.spv: shaders/sim/mpm.comp $(wildcard shaders/sim/*.glsl)
	@mkdir -p build/shaders
	$(GLSLANG) -DDC_MPM_SHARED_GATHER=1 -V --target-env vulkan1.3 -S comp -o $@ $<
build/shaders/runtime/mpm_stencil.comp.spv: shaders/sim/mpm.comp $(wildcard shaders/sim/*.glsl)
	@mkdir -p build/shaders/runtime
	$(GLSLANG) -DDC_RUNTIME_BODY_STORAGE=1 -DDC_MPM_SHARED_GATHER=1 -V --target-env vulkan1.3 -S comp -o $@ $<
$(GPU_OBJ) $(GPU_SOURCE_BINARIES): build/shaders/mpm_stencil.comp.spv build/shaders/runtime/mpm_stencil.comp.spv

# Exact hydrostatic recurrence and duplicate-transform work budget.
build/diagnostics/hydro_head_profile.comp.spv: shaders/sim/hydro_head.comp
	@mkdir -p $(@D)
	glslangValidator -V --target-env vulkan1.1 -DDC_HYDRO_HEAD_PROFILE=1 -o $@ $<
build/hydro_head_work_tests: tests/vulkan/hydro_head_tests.c tools/diagnostics/vulkan_probe.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/hydro_head_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHYDRO_TEST_PROFILE=1 -DHYDRO_TEST_SHADER='"build/diagnostics/hydro_head_profile.comp.spv"' -o $@ $< $(GPU_OBJ) $(CHUNK_OBJ) $(LDLIBS)

build/hydro_head_tall_work_tests: tests/vulkan/hydro_head_tests.c tools/diagnostics/vulkan_probe.c tools/diagnostics/shader_fixture.h $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/hydro_head_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHYDRO_TEST_HEIGHT=1153 -DHYDRO_TEST_PROFILE=1 -DHYDRO_TEST_SHADER='"build/diagnostics/hydro_head_profile.comp.spv"' -o $@ $< $(GPU_OBJ) $(CHUNK_OBJ) $(LDLIBS)
test: build/hydro_head_work_tests build/hydro_head_tall_work_tests

build/diagnostics/pixel_boundary_probe.spv: shaders/sim/contact_geometry.glsl

build/solver_contact_start_tests: tests/vulkan/solver_contact_start_tests.c tests/vulkan/solver_motion_store_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/solver_motion_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/solver_contact_start_tests

build/solver_grain_fusion_tests: tests/vulkan/solver_grain_fusion_tests.c tests/vulkan/solver_motion_store_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/solver_motion_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/solver_grain_fusion_tests

build/shaders/fluid_transport_recompute_16.comp.spv: shaders/sim/fluid_transport.comp shaders/sim/fluid_short_run.glsl shaders/sim/fluid_one_cell.glsl Makefile
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_RECOMPUTE_REQUESTS=1 -DDC_TRANSPORT_GROUP_SIZE=16 $< -o $@
build/shaders/fluid_transport_recompute.comp.spv: shaders/sim/fluid_transport.comp shaders/sim/fluid_short_run.glsl shaders/sim/fluid_one_cell.glsl Makefile
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_RECOMPUTE_REQUESTS=1 $< -o $@
build/diagnostics/fluid_transport_recompute_profile.comp.spv: shaders/sim/fluid_transport.comp shaders/sim/fluid_short_run.glsl shaders/sim/fluid_one_cell.glsl Makefile
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_RECOMPUTE_REQUESTS=1 -DDC_TRANSPORT_GROUP_SIZE=16 -DDC_TRANSPORT_PROFILE=1 $< -o $@
build/fluid_recompute_requests_tests: tests/vulkan/fluid_recompute_requests_tests.c tests/vulkan/fluid_transport_partial_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/fluid_transport_profile_16.comp.spv build/diagnostics/fluid_transport_recompute_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/fluid_recompute_requests_tests build/shaders/fluid_transport_recompute_16.comp.spv
build/diagnostics/fluid_dry_line_profile.comp.spv: shaders/sim/fluid_transport.comp shaders/sim/fluid_short_run.glsl shaders/sim/fluid_one_cell.glsl Makefile
	$(GLSLANG) -V --target-env vulkan1.3 -DDC_TRANSPORT_PROFILE=1 -DDC_DRY_LINE_PROFILE=1 -DDC_TRANSPORT_GROUP_SIZE=16 $< -o $@
build/fluid_dry_line_tests: tests/vulkan/fluid_dry_line_tests.c tests/vulkan/fluid_transport_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/fluid_dry_line_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/fluid_dry_line_tests
build/shaders/fluid_transport_recompute_16.comp.spv build/shaders/fluid_transport_recompute.comp.spv build/diagnostics/fluid_transport_recompute_profile.comp.spv: shaders/sim/fluid_aperture.glsl shaders/sim/hydraulic_face.glsl shaders/sim/water_color.glsl
$(GPU_OBJ) $(PERFORMANCE_OBJ): build/shaders/fluid_transport_recompute_16.comp.spv build/shaders/fluid_transport_recompute.comp.spv

build/shaders/mpm_commit.comp.spv: shaders/sim/mpm.comp $(wildcard shaders/sim/*.glsl)
	@mkdir -p $(@D)
	glslangValidator -V --target-env vulkan1.1 -DDC_MPM_COALESCED_COMMIT=1 -o $@ $<
build/shaders/runtime/mpm_commit.comp.spv: shaders/sim/mpm.comp $(wildcard shaders/sim/*.glsl)
	@mkdir -p $(@D)
	glslangValidator -V --target-env vulkan1.1 -DDC_MPM_COALESCED_COMMIT=1 -DDC_RUNTIME_BODY_STORAGE=1 -o $@ $<
$(GPU_OBJ) $(GPU_SOURCE_BINARIES): build/shaders/mpm_commit.comp.spv build/shaders/runtime/mpm_commit.comp.spv
build/mpm_gather_tests build/mpm_gather_partial_tile_tests: build/shaders/mpm_commit.comp.spv

build/mpm_transfer_load_tests: tests/vulkan/mpm_transfer_load_tests.c tests/vulkan/mpm_gather_tests.c $(GPU_OBJ) $(CHUNK_OBJ) build/diagnostics/mpm_gather_profile.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
test: build/mpm_transfer_load_tests

build/diagnostics/quarter_app_frame_trace build/diagnostics/native360_frame_trace: tools/diagnostics/frame_api_trace.h
