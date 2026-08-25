ifneq (,$(wildcard .env))
    include .env
endif

BUILD_DIR     = build
BUILD_DIR_WEB = build-web
WEB_UI_DIR    = apps/web/ui
WEB_BASE      ?= /

NPROC         := $(shell sysctl -n hw.ncpu 2>/dev/null || nproc)
JOB           := $(shell echo $$(($(NPROC) > 2 ? $(NPROC) - 1 : 1)))

.PHONY: ear6 ear6-wasm ear6-web test serve clean

ear6:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Release \
		-DEAR6_BUILD_DESKTOP=ON \
		-DEAR6_BUILD_WASM=OFF
	cmake --build $(BUILD_DIR) -j $(JOB)

ear6-wasm:
ifndef EMSCRIPTEN_CMAKE_TOOLCHAIN
	$(error EMSCRIPTEN_CMAKE_TOOLCHAIN is not set. Define it in .env)
endif
	cmake -B $(BUILD_DIR_WEB) -S . \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=$(EMSCRIPTEN_CMAKE_TOOLCHAIN) \
		-DEAR6_BUILD_DESKTOP=OFF \
		-DEAR6_BUILD_WASM=ON
	cmake --build $(BUILD_DIR_WEB) --target ear6-wasm -j $(JOB)

ear6-web: ear6-wasm
	cd $(WEB_UI_DIR) && npm ci && npm run build -- --base=$(WEB_BASE)

test:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Release \
		-DEAR6_BUILD_DESKTOP=OFF \
		-DEAR6_BUILD_WASM=OFF \
		-DEAR6_BUILD_TESTS=ON
	cmake --build $(BUILD_DIR) -j $(JOB)
	./$(BUILD_DIR)/ear6-test

serve: ear6-wasm
	cd $(WEB_UI_DIR) && npm i && npm run dev

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR_WEB)
