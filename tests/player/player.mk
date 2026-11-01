BEAVER_SRC = tests/player/beaver_red_stubs.c
build/beaver_asset_tests: tests/player/beaver_tests.c $(BEAVER_SRC) $(PLAYER_SRC) $(REACTIVE_STATE_SRC) src/assets/prop.c src/renderer/appearance.c include/ferrum/player/beaver.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/player/beaver_tests.c $(BEAVER_SRC) $(PLAYER_SRC) $(REACTIVE_STATE_SRC) src/assets/prop.c src/renderer/appearance.c -o $@ -lm
.PHONY: test_beaver_assets
test_beaver_assets: build/beaver_asset_tests
	./build/beaver_asset_tests
test_player: test_beaver_assets
