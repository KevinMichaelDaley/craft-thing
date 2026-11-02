BEAVER_SRC = src/player/beaver_art.c src/player/beaver_asset.c
build/player_input_tests: tests/player/input_tests.c src/player/input.c src/player/control.c $(PLAYER_SRC) include/ferrum/player/input.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/player/input.c src/player/control.c $(PLAYER_SRC) -o $@ -lm
.PHONY: test_player_input
test_player_input: build/player_input_tests
	./build/player_input_tests
test_player: test_player_input
build/player_input_sdl_tests: tests/player/input_sdl_tests.c src/player/input_sdl.c src/player/input.c include/ferrum/player/input_sdl.h include/ferrum/player/input.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/player/input_sdl.c src/player/input.c -o $@ $(shell pkg-config --libs sdl2) -lm -Wl,--wrap=SDL_WasInit -Wl,--wrap=SDL_GetKeyboardState
.PHONY: test_player_input_sdl
test_player_input_sdl: build/player_input_sdl_tests
	./build/player_input_sdl_tests
test_player: test_player_input_sdl
build/player_sprite_gpu_tests: tests/player/sprite_gpu_tests.c src/vulkan/sprite_gpu.c include/ferrum/renderer/sprite_gpu.h build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
.PHONY: test_player_sprite_gpu
test_player_sprite_gpu: build/player_sprite_gpu_tests
	./build/player_sprite_gpu_tests
test: test_player_sprite_gpu
build/player_body_gpu_tests: tests/player/body_gpu_tests.c src/player/body_gpu.c include/ferrum/player/body_gpu.h $(BEAVER_SRC) $(PLAYER_SRC) src/player/beaver_imported.h build/fluid_tests
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/player/body_gpu.c $(BEAVER_SRC) $(PLAYER_SRC) $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
.PHONY: test_player_body_gpu
test_player_body_gpu: build/player_body_gpu_tests
	./build/player_body_gpu_tests
test: test_player_body_gpu
build/player_runtime_tests: tests/app/player_runtime_tests.c tests/player/runtime_visual_check.h build/dungeoncraft_quarter_native
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(filter-out build/performance/src/app/main.o,$(QUARTER_APP_OBJ)) $(PERFORMANCE_OBJ) -o $@ $(LDLIBS)
.PHONY: test_player_runtime
test_player_runtime: build/player_runtime_tests
	./build/player_runtime_tests
test: test_player_runtime
build/player_fire_ray_gpu_tests: tests/player/fire_ray_gpu_tests.c src/player/fire_ray_gpu.c include/ferrum/player/fire_ray_gpu.h build/fluid_tests build/shaders/player_fire_ray.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/player/fire_ray_gpu.c $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS) -Wl,--wrap=fr_renderer_submit
.PHONY: test_player_fire_ray_gpu
test_player_fire_ray_gpu: build/player_fire_ray_gpu_tests
	./build/player_fire_ray_gpu_tests
test: test_player_fire_ray_gpu
build/shaders/player_fire_ray.comp.spv: shaders/reactive/player_fire_ray.comp include/dungeoncraft/fire_ray_layout.h $(wildcard shaders/reactive/*.glsl) $(wildcard shaders/sim/*.glsl)
	mkdir -p build/shaders
	$(GLSLANG) -DDC_RUNTIME_BODY_STORAGE=1 -V --target-env vulkan1.3 $< -o $@
build/shaders/scene.comp.spv build/shaders/runtime/scene.comp.spv: include/dungeoncraft/fire_ray_layout.h
build/beaver_fire_plan_tests: tests/app/beaver_fire_plan_tests.c src/app/beaver_fire_plan.c src/app/beaver_fire_demo.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/app/beaver_fire_plan.c -o $@
.PHONY: test_beaver_fire_plan
test_beaver_fire_plan: build/beaver_fire_plan_tests
	./build/beaver_fire_plan_tests
test: test_beaver_fire_plan
build/beaver_fire_runtime_tests: tests/app/beaver_fire_runtime_tests.c src/app/beaver_fire_scene.c src/app/beaver_fire_plan.c build/dungeoncraft_quarter_native
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) $< $(filter-out build/performance/src/app/main.o,$(QUARTER_APP_OBJ)) $(PERFORMANCE_OBJ) -o $@ $(LDLIBS)
.PHONY: test_beaver_fire_runtime
test_beaver_fire_runtime: build/beaver_fire_runtime_tests
	./build/beaver_fire_runtime_tests
test: test_beaver_fire_runtime
build/beaver_fire_demo: build/beaver_fire_main.o build/dungeoncraft_quarter_native build/shaders/player_fire_ray.comp.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) build/beaver_fire_main.o $(filter-out build/performance/src/app/main.o,$(QUARTER_APP_OBJ)) $(PERFORMANCE_OBJ) -o $@ $(LDLIBS)
build/beaver_fire_main.o: src/app/main.c src/app/beaver_fire_demo.h src/app/player_session.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUARTER_FLAGS) -DDC_BEAVER_FIRE_DEMO=1 -c $< -o $@
.PHONY: beaver-fire-demo
beaver-fire-demo: build/beaver_fire_demo
build/beaver_fire_runtime_tests: build/shaders/player_fire_ray.comp.spv
build/player_clock_tests: tests/app/player_clock_tests.c src/app/player_clock.c src/app/player_clock.h src/player/control.c src/player/input.c $(PLAYER_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/app/player_clock.c src/player/control.c src/player/input.c $(PLAYER_SRC) -o $@ -lm
.PHONY: test_player_clock
test_player_clock: build/player_clock_tests
	./build/player_clock_tests
test: test_player_clock
build/smoke_profile_probe: tests/app/smoke_profile_probe.c src/app/smoke_profile.c src/app/smoke_profile.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/app/smoke_profile.c -o $@
.PHONY: test_smoke_profile
test_smoke_profile: build/smoke_profile_probe
	python3 tests/app/smoke_profile_tests.py
test: test_smoke_profile
src/player/beaver_imported.h: assets/player/beaver/beaver_cast.ase assets/player/beaver/beaver_cast.mapping.json assets/player/beaver/beaver_chomp.ase assets/player/beaver/beaver_chomp.mapping.json scripts/props/beaver_import.py scripts/props/ase_reader.py
	python3 scripts/props/beaver_import.py
build/beaver_asset_tests build/beaver_atlas: src/player/beaver_imported.h
build/beaver_asset_tests: src/fire/material_profile_data.inc include/dungeoncraft/coating_layout.h
build/beaver_asset_tests: tests/player/beaver_tests.c $(BEAVER_SRC) $(PLAYER_SRC) $(REACTIVE_STATE_SRC) src/assets/prop.c src/renderer/appearance.c include/ferrum/player/beaver.h src/player/beaver_internal.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/player/beaver_tests.c $(BEAVER_SRC) $(PLAYER_SRC) $(REACTIVE_STATE_SRC) src/assets/prop.c src/renderer/appearance.c -o $@ -lm
.PHONY: test_beaver_assets
test_beaver_assets: build/beaver_asset_tests
	./build/beaver_asset_tests
test_player: test_beaver_assets

build/player_drive_gpu_tests: tests/player/drive_gpu_tests.c src/vulkan/player_drive_gpu.c include/ferrum/player/drive_gpu.h build/fluid_tests $(BEAVER_SRC) $(PLAYER_SRC) src/player/beaver_imported.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/vulkan/player_drive_gpu.c $(BEAVER_SRC) $(PLAYER_SRC) $(GPU_OBJ) $(CHUNK_OBJ) -o $@ $(LDLIBS)
.PHONY: test_player_drive_gpu
test_player_drive_gpu: build/player_drive_gpu_tests
	./build/player_drive_gpu_tests
test: test_player_drive_gpu
build/shaders/player_drive.comp.spv: shaders/sim/player_drive.comp shaders/sim/rigid_body.glsl shaders/sim/runtime_body_storage.glsl shaders/sim/solver_body_state.glsl
	mkdir -p build/shaders
	$(GLSLANG) -V --target-env vulkan1.3 $< -o $@
build/player_drive_gpu_tests: build/shaders/player_drive.comp.spv
build/shaders/player_drive.comp.spv: shaders/sim/rigid_storage.glsl include/dungeoncraft/body_storage_layout.h include/dungeoncraft/material_density.h include/dungeoncraft/player_drive_layout.h
build/player_drive_gpu_tests: include/dungeoncraft/player_drive_layout.h src/vulkan/runtime_body_internal.h src/vulkan/runtime_body_gpu.h src/vulkan/gpu_internal.h src/vulkan/renderer_internal.h
build/player_drive_gpu_tests: include/ferrum/player/beaver.h src/player/beaver_internal.h include/ferrum/player/player.h include/ferrum/fire/gpu.h include/ferrum/fire/material_profile.h

build/beaver_atlas: tools/diagnostics/beaver_atlas.c $(BEAVER_SRC) $(PLAYER_SRC) src/assets/prop.c include/ferrum/player/beaver.h src/player/beaver_internal.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BEAVER_SRC) $(PLAYER_SRC) src/assets/prop.c -o $@ $(shell pkg-config --libs sdl2) -lm
