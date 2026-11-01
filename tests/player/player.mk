BEAVER_SRC = src/player/beaver_art.c src/player/beaver_asset.c
src/player/beaver_imported.h: assets/player/beaver/beaver_chomp.ase assets/player/beaver/beaver_chomp.mapping.json scripts/props/beaver_import.py scripts/props/ase_reader.py
	python3 scripts/props/beaver_import.py
build/beaver_asset_tests build/beaver_atlas: src/player/beaver_imported.h
build/beaver_asset_tests: tests/player/beaver_tests.c $(BEAVER_SRC) $(PLAYER_SRC) $(REACTIVE_STATE_SRC) src/assets/prop.c src/renderer/appearance.c include/ferrum/player/beaver.h src/player/beaver_internal.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/player/beaver_tests.c $(BEAVER_SRC) $(PLAYER_SRC) $(REACTIVE_STATE_SRC) src/assets/prop.c src/renderer/appearance.c -o $@ -lm
.PHONY: test_beaver_assets
test_beaver_assets: build/beaver_asset_tests
	./build/beaver_asset_tests
test_player: test_beaver_assets

build/beaver_atlas: tools/diagnostics/beaver_atlas.c $(BEAVER_SRC) $(PLAYER_SRC) src/assets/prop.c include/ferrum/player/beaver.h src/player/beaver_internal.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BEAVER_SRC) $(PLAYER_SRC) src/assets/prop.c -o $@ $(shell pkg-config --libs sdl2) -lm
