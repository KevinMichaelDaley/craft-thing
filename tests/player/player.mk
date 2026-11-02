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
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/player/input_sdl.c src/player/input.c -o $@ -lm
.PHONY: test_player_input_sdl
test_player_input_sdl: build/player_input_sdl_tests
	./build/player_input_sdl_tests
test_player: test_player_input_sdl
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
