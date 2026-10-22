# LAS Recomp build: the shared runtime (nxrt/, vendored: nxrt.mk) + this game's own sources (runtime/).
# macOS: Homebrew clang/SDL3/FFmpeg, Metal renderer.
# Windows: MSYS2 UCRT64 shell (see docs/BUILD-windows.md), clang + Vulkan renderer.
# Linux: clang + Vulkan renderer, x86-64 or aarch64 (see docs/linux.md).
# the library root is the submodule root
NXRT     := nxrt
export NXRT  # (the packaging scripts below take it too)
GAME_ID  := las
GAME_EXE := lasrecomp
GAME_SRC := $(wildcard runtime/*.c) $(wildcard runtime/hle/*.c) $(wildcard runtime/nvn/*.c) $(wildcard runtime/ui/*.mm)
GAMELIB_EXTRA := $(wildcard tools/rando/patch_variants.json)  # (gen.py's variants, through tools/patches.py)
WINDOWS_ICON := docs/icon/icon.ico

include $(NXRT)/nxrt.mk

VERSION  := $(shell sed -n 's/^\#define LAS_VERSION "\(.*\)"/\1/p' runtime/las_game.c)

# macOS: build/LAS Recomp.app, runs on macOS $(APP_MIN)+: the game rebuilt for that in build/mac15 against build/deps
# (`make deps`: SDL3, LGPL FFmpeg, keystone, Python, from pinned sources), the generator helper when built
# (`make rando-helper` after `make deps`), packaged by tools/bundle_macos.sh. No game files or fonts.
APP_MIN  := 15.0
APP_B    := build/mac15
deps:
	MACOS_MIN=$(APP_MIN) $(NXRT)/tools/build_deps_macos.sh
app:
	@[ -f build/deps/lib/libSDL3.0.dylib ] || { echo "make deps first"; exit 1; }
	$(MAKE) B=$(APP_B) MACOS_MIN=$(APP_MIN) DEPS=$(CURDIR)/build/deps $(APP_B)/$(GAME_EXE)
	MACOS_MIN=$(APP_MIN) tools/bundle_macos.sh $(APP_B)/$(GAME_EXE)

# macOS release disk image (the .app + an Applications shortcut to drag it onto): build/dist/LAS Recomp-<v>-macOS.dmg
dmg: app
	tools/make_dmg.sh

# Windows: build/dist/LAS Recomp-<version>-windows.zip, top folder "LAS Recomp/": the exe, the seed generator
# helper (make rando-helper), what the start screen builds the game code with on the player's machine (make gen-helper,
# $(NXRT)/tools/fetch_zig.sh slimmed by $(NXRT)/tools/zig_slim.sh: its runtime built ahead of time, gamelib-src/, the
# exe's import library) and the DLLs they load (runs without MSYS2). No game code. The exe is a release build in
# $(WIN_B): linked against the LGPL FFmpeg from `make deps-windows` and without a console window (RELEASE=1: the log
# goes to lasrecomp.log next to the exe). vulkan-1.dll is left out: every Vulkan driver installs the loader in System32.
# Needs: Windows 10+ x64, a Vulkan 1.3 driver, an AVX2 CPU (GENARCH x86-64-v3; checked at startup).
DIST     := build/dist/LAS Recomp
WIN_B    := build/win-release
deps-windows:
	$(NXRT)/tools/build_deps_windows.sh
dist:
	@[ -f build/deps-win/lib/libavcodec.dll.a ] || { echo "make deps-windows first (LGPL FFmpeg)"; exit 1; }
	@[ -d build/helper/rando-gen ] || { echo "make rando-helper first (the seed generator)"; exit 1; }
	@[ -d build/helper/nxgen ] || { echo "make gen-helper first (the game code generator)"; exit 1; }
	@[ -x build/zig/zig.exe ] || { echo "$(NXRT)/tools/fetch_zig.sh first (the compiler)"; exit 1; }
	$(MAKE) B=$(WIN_B) DEPS=$(CURDIR)/build/deps-win RELEASE=1 $(WIN_B)/$(GAME_EXE).exe
	! nm $(WIN_B)/$(GAME_EXE).exe | grep ' f_70' >/dev/null  # (no game code inside)
	rm -rf build/dist && mkdir -p "$(DIST)/gamelib-src"
	cp $(WIN_B)/$(GAME_EXE).exe $(WIN_B)/$(GAME_EXE).dll.a "$(DIST)/"
	$(NXRT)/tools/zig_slim.sh $(strip $(filter-out -I$(RT),$(GENFLAGS)))  # (zig with its runtime built ahead of time)
	cp -R build/helper/nxgen "$(DIST)/"
	cp -R build/zig-slim "$(DIST)/zig"
	cp $(RT)/cpu.h $(RT)/simd.h $(RT)/simd_fp.h $(RT)/gamelib.h $(NXRT)/tools/gamelib_entry.c $(RT)/hle/hle_symbols.txt \
	    "$(DIST)/gamelib-src/"
	cp docs/icon/icon_1024.png "$(DIST)/icon.png"
	cp $(NXRT)/third_party/fonts/Baloo2/Baloo2-SemiBold.ttf "$(DIST)/"  # (start screen font, OFL)
	cp $(NXRT)/third_party/fonts/Baloo2/OFL.txt "$(DIST)/Baloo2-OFL.txt"
	cp README.md LICENSE "$(DIST)/"  # (licenses and source links of the bundled components: README "License")
	cp -R build/helper/rando-gen "$(DIST)/"
	PATH="$(CURDIR)/build/deps-win/bin:$$PATH" ldd $(WIN_B)/$(GAME_EXE).exe | awk '(/ucrt64/ || /deps-win/) && !/vulkan-1/ {print $$3}' | sort -u | \
	    xargs -I{} cp -u {} "$(DIST)/"
	! ls "$(DIST)" | grep -iE 'x264|x265|xvid|postproc'  # (no GPL codec libraries in the release)
	cd build/dist && if command -v zip >/dev/null; then zip -qr "LAS Recomp-$(VERSION)-windows.zip" "LAS Recomp"; \
	    else /c/Windows/System32/tar.exe -a -cf "LAS Recomp-$(VERSION)-windows.zip" "LAS Recomp"; fi
	@echo "build/dist/LAS Recomp-$(VERSION)-windows.zip"

# Seed generator as a standalone helper, so players need no Python: build/helper/rando-gen/ (rando-gen[.exe] +
# _internal/), frozen by PyInstaller from tools/rando/rando_gen.spec in its own venv (build/rando-venv). Ship the
# rando-gen folder next to the exe (Windows) or as "LAS Recomp.app"/Contents/Resources/rando-gen (runtime/ui/rando_tab.mm
# generator_cmd). PyInstaller only builds for the OS it runs on.
ifeq ($(PLATFORM),macos)
RANDO_PY  ?= $(firstword $(wildcard build/deps/python/bin/python3.12) python3.12)
export KEYSTONE_LIB ?= $(wildcard $(CURDIR)/build/deps/lib/libkeystone.dylib)
RANDO_VPY := build/rando-venv/bin/python
else
RANDO_PY  ?= py -3.12
RANDO_VPY := build/rando-venv/Scripts/python.exe
endif

rando-helper: build/rando-venv/.ok
	$(if $(filter windows,$(PLATFORM)),USERPROFILE="$$(cygpath -w ~ 2>/dev/null || echo $$HOME)") \
	$(RANDO_VPY) -m PyInstaller --noconfirm --clean --log-level WARN --distpath build/helper --workpath build/pyinstaller \
	    tools/rando/rando_gen.spec
	@du -sh build/helper/rando-gen

# The game code generator, the same way (tools/nxgen.spec: nxrt's gen.py with this game's patches.py): build/helper/nxgen/,
# shipped as nxgen/ next to the exe or in Contents/Resources. The start screen's setup runs it on the player's own game
# files (runtime/gamebuild.c).
gen-helper: build/rando-venv/.ok build/rando-venv/.gen-ok
	$(if $(filter windows,$(PLATFORM)),USERPROFILE="$$(cygpath -w ~ 2>/dev/null || echo $$HOME)") \
	$(RANDO_VPY) -m PyInstaller --noconfirm --clean --log-level WARN --distpath build/helper --workpath build/pyinstaller \
	    tools/nxgen.spec
	$(if $(filter windows,$(PLATFORM)),$(RANDO_VPY) $(NXRT)/tools/pe_no_cfg.py build/helper/nxgen/nxgen.exe)
	@du -sh build/helper/nxgen

build/rando-venv/.gen-ok: build/rando-venv/.ok
	$(RANDO_VPY) -m pip install -q capstone==5.0.9 unicorn==2.1.4 lz4==4.4.5
	touch $@

build/rando-venv/.ok: third_party/LAS-Randomizer/requirements.txt
	$(RANDO_PY) -m venv build/rando-venv
	$(RANDO_VPY) -m pip install -q -r third_party/LAS-Randomizer/requirements.txt pyinstaller==6.22.3
	touch $@

# Linux: build/dist/LAS Recomp-<version>-linux-<arch>.AppImage, built in Ubuntu 22.04 containers (podman). See docs/linux.md.
appimage:
	tools/linux/appimage.sh $(ARCH)
appimages:
	tools/linux/appimage.sh x86_64
	tools/linux/appimage.sh aarch64
# release check of the x86-64 AppImage: start screen, into the game, warps to the overworld and every dungeon
smoketest:
	tools/linux/smoketest.sh

.PHONY: dmg deps deps-windows app dist rando-helper gen-helper appimage appimages smoketest
