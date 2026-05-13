ifneq (,$(wildcard .env))
    include .env
endif

BUILD_DIR     = build
BUILD_DIR_WEB = build-web
WEB_UI_DIR    = app/web-ui
WEB_PUBLIC    = $(WEB_UI_DIR)/public/ear6

NPROC         := $(shell sysctl -n hw.ncpu 2>/dev/null || nproc)
JOB           := $(shell echo $$(($(NPROC) > 2 ? $(NPROC) - 1 : 1)))

.PHONY: ear6 ear6-web serve clean

ear6:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Release \
		-DEAR6_BUILD_DESKTOP=ON \
		-DEAR6_BUILD_WEB=OFF
	cmake --build $(BUILD_DIR) -j $(JOB)

ear6-web:
ifndef EMSCRIPTEN_CMAKE_TOOLCHAIN
	$(error EMSCRIPTEN_CMAKE_TOOLCHAIN is not set. Define it in .env)
endif
	cmake -B $(BUILD_DIR_WEB) -S . \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=$(EMSCRIPTEN_CMAKE_TOOLCHAIN) \
		-DEAR6_BUILD_DESKTOP=OFF \
		-DEAR6_BUILD_WEB=ON
	cmake --build $(BUILD_DIR_WEB) -j $(JOB)

serve: ear6-web
	@mkdir -p $(WEB_PUBLIC)
	cp $(BUILD_DIR_WEB)/app/web/ear6-web.js $(WEB_PUBLIC)/
	cp $(BUILD_DIR_WEB)/app/web/ear6-web.wasm $(WEB_PUBLIC)/
	cd $(WEB_UI_DIR) && npm i && npm run dev

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR_WEB)
