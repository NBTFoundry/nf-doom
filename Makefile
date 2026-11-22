# SPDX-License-Identifier: GPL-2.0-or-later
SHELL := /bin/bash

BUILD_DIR ?= build
OBJ_DIR := $(BUILD_DIR)/obj
PRBOOM_SRC := third_party/prboom-plus/prboom2/src
PRBOOM_DATA := third_party/prboom-plus/prboom2/data

DOOM1_WAD ?= $(or $(wildcard assets/doom1.wad),$(wildcard doom1.wad),assets/doom1.wad)
HAS_DOOM1_WAD := $(wildcard $(DOOM1_WAD))

APP_VERSION ?= 1.0.19
APP_SEQ ?= 20
APP_KEY ?=
# Sibling NBTFoundry checkouts; override as needed. The SDK is built into
# $(BUILD_DIR)/sdk; the manager is only needed for packaging.
NBT_SDK_DIR ?= ../sdk
NBT_MGR_DIR ?= ../manager
# QNX 6.5 toolchain: the environment script to source, and the folder holding
# the head unit's libscreen.so.
QNX_ENV ?=
QNX_SCREEN_LIBDIR ?=

CC ?= gcc
CXX ?= g++
QCC ?= qcc
QNX_STRIP ?= ntoarmv7-strip
QNX_READELF ?= ntoarmv7-readelf
QNX_NM ?= ntoarmv7-nm

NBT_SDK_BUILD_DIR := $(abspath $(BUILD_DIR))/sdk
include $(NBT_SDK_DIR)/nbt-sdk.mk
# Each kind of object rebuilds when its compile command changes (the SDK's
# nbt-stamp.mk; the stamps are written at the end of this file).
stamp = $(OBJ_DIR)/$(1).stamp
ifeq ($(value nbt_stamp),)
$(error $(NBT_SDK_ROOT) has no nbt-stamp.mk: update that SDK checkout)
endif

COMMON_INC := $(NBT_SDK_INC) -Isrc -Isrc/prboom_compat -I$(PRBOOM_SRC)
COMMON_DEFS := -DHAVE_CONFIG_H $(if $(HAS_DOOM1_WAD),,-DNBT_NO_EMBEDDED_DOOM1)

HOST_CFLAGS := -std=gnu99 -O2 -Wall -Wextra -Wno-unused-parameter $(CFLAGS)
HOST_CXXFLAGS := -std=c++11 -O2 -Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti $(CXXFLAGS)
PRBOOM_C_WARN := -std=gnu99 -O2 -w $(CFLAGS)
PRBOOM_CXX_WARN := -std=c++11 -O2 -w -fno-exceptions -fno-rtti $(CXXFLAGS)

HOST_LDLIBS := -lX11 -lEGL -lGLESv2 -lm -ldl -lpthread

WAD_ASM_FLAGS := -DNBT_PRBOOM_WAD_PATH='"$(abspath $(BUILD_DIR))/prboom-plus.wad"'
ifneq ($(HAS_DOOM1_WAD),)
WAD_ASM_FLAGS += -DNBT_DOOM1_WAD_PATH='"$(abspath $(DOOM1_WAD))"'
else
WAD_ASM_FLAGS += -DNBT_NO_EMBEDDED_DOOM1
endif

# Upstream PrBoom+ core sources (w_wad.c compiled via src/prboom_compat/w_wad_wrap.c)
PRBOOM_CORE_C := \
	am_map.c doomdef.c doomstat.c dstrings.c d_deh.c d_items.c d_main.c d_client.c \
	e6y.c f_finale.c f_finale2.c f_wipe.c g_game.c g_overflow.c hu_lib.c hu_stuff.c \
	hu_tracers.c info.c i_capture.c i_glob.c lprintf.c md5.c m_argv.c m_bbox.c \
	m_cheat.c m_menu.c m_misc.c m_random.c p_ceilng.c p_checksum.c p_doors.c \
	p_enemy.c p_floor.c p_genlin.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c \
	p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c \
	p_tick.c p_user.c r_bsp.c r_data.c r_demo.c r_draw.c r_filter.c r_fps.c r_main.c \
	r_patch.c r_plane.c r_segs.c r_sky.c r_things.c sc_man.c sounds.c statdump.c \
	st_lib.c st_stuff.c s_advsound.c s_sound.c tables.c version.c v_video.c wi_stuff.c \
	w_memcache.c z_bmalloc.c z_zone.c memio.c mus2mid.c \
	MUSIC/dbopl.c MUSIC/midifile.c MUSIC/opl.c MUSIC/oplplayer.c MUSIC/opl_queue.c \
	gl_clipper.c gl_detail.c gl_drawinfo.c gl_fbo.c gl_gamma.c gl_hires.c gl_hqresize.c \
	gl_light.c gl_main.c gl_map.c gl_missingtexture.c gl_opengl.c gl_preprocess.c \
	gl_shader.c gl_shadow.c gl_sky.c gl_texture.c gl_vertex.c gl_wipe.c

PRBOOM_CORE_CXX := umapinfo.cpp scanner.cpp

OUR_C := \
	src/prboom_compat/sdl_compat.c \
	src/prboom_compat/m_io_wrap.c \
	src/prboom_compat/w_wad_wrap.c

OUR_CXX := \
	src/wad_vfs.cpp \
	src/wad_embed_reg.cpp \
	src/doom_settings.cpp \
	src/doom_log.cpp \
	src/doom_video.cpp \
	src/doom_controls.cpp \
	src/input_idrive.cpp \
	src/input_gamepad.cpp \
	src/i_system_nbt.cpp \
	src/i_sound_nbt.cpp \
	src/i_video_nbt.cpp \
	src/i_input_nbt.cpp \
	src/main.cpp

HOST_PRBOOM_OBJ := \
	$(patsubst %.c,$(OBJ_DIR)/host/prboom/%.o,$(PRBOOM_CORE_C)) \
	$(patsubst %.cpp,$(OBJ_DIR)/host/prboom/%.o,$(PRBOOM_CORE_CXX))

HOST_OUR_OBJ := \
	$(patsubst %.c,$(OBJ_DIR)/host/%.o,$(OUR_C)) \
	$(patsubst %.cpp,$(OBJ_DIR)/host/%.o,$(OUR_CXX)) \
	$(OBJ_DIR)/host/wad_embed.o

RDATAWAD_SRC := \
	$(PRBOOM_DATA)/rd_main.c \
	$(PRBOOM_DATA)/rd_util.c \
	$(PRBOOM_DATA)/rd_output.c \
	$(PRBOOM_DATA)/rd_sound.c \
	$(PRBOOM_DATA)/rd_palette.c \
	$(PRBOOM_DATA)/rd_graphic.c

WAD_PALETTE := palette.rgb
WAD_LUMPS := \
	lumps/switches.lmp lumps/animated.lmp lumps/crbrick.lmp lumps/crtan.lmp \
	lumps/crgray.lmp lumps/crgreen.lmp lumps/crbrown.lmp lumps/crgold.lmp \
	lumps/crred.lmp lumps/crblue.lmp lumps/crblue2.lmp lumps/crorange.lmp \
	lumps/cryellow.lmp lumps/crblack.lmp lumps/crpurple.lmp lumps/crwhite.lmp
WAD_COLORMAPS := lumps/watermap.lmp
WAD_TABLES := \
	lumps/sinetabl.lmp lumps/tangtabl.lmp lumps/tantoang.lmp lumps/gammatbl.lmp \
	lumps/chexdeh.lmp lumps/bfgbex.lmp lumps/nervebex.lmp lumps/glshadow.lmp \
	lumps/glfp.lmp lumps/glvp.lmp lumps/-prbhud-.lmp lumps/m_ammo.lmp \
	lumps/m_armour.lmp lumps/m_arrow.lmp lumps/m_health.lmp lumps/m_key.lmp \
	lumps/m_normal.lmp lumps/m_shadow.lmp lumps/m_power.lmp lumps/m_weap.lmp \
	lumps/m_mark.lmp
WAD_SOUNDS := \
	sounds/dsdgsit.wav sounds/dsdgatk.wav sounds/dsdgact.wav sounds/dsdgdth.wav sounds/dsdgpain.wav
WAD_GRAPHICS := \
	graphics/dig0.ppm graphics/dig1.ppm graphics/dig2.ppm graphics/dig3.ppm graphics/dig4.ppm \
	graphics/dig5.ppm graphics/dig6.ppm graphics/dig7.ppm graphics/dig8.ppm graphics/dig9.ppm \
	graphics/diga.ppm graphics/digb.ppm graphics/digc.ppm graphics/digd.ppm graphics/dige.ppm \
	graphics/digf.ppm graphics/digg.ppm graphics/digh.ppm graphics/digi.ppm graphics/digj.ppm \
	graphics/digk.ppm graphics/digl.ppm graphics/digm.ppm graphics/dign.ppm graphics/digo.ppm \
	graphics/digp.ppm graphics/digq.ppm graphics/digr.ppm graphics/digs.ppm graphics/digt.ppm \
	graphics/digu.ppm graphics/digv.ppm graphics/digw.ppm graphics/digx.ppm graphics/digy.ppm \
	graphics/digz.ppm graphics/dig45.ppm graphics/dig46.ppm graphics/dig47.ppm graphics/dig58.ppm \
	graphics/dig91.ppm graphics/dig93.ppm graphics/stbr123.ppm graphics/stbr124.ppm \
	graphics/stbr125.ppm graphics/stbr126.ppm graphics/stbr127.ppm graphics/boxul.ppm \
	graphics/boxuc.ppm graphics/boxur.ppm graphics/boxcl.ppm graphics/boxcc.ppm graphics/boxcr.ppm \
	graphics/boxll.ppm graphics/boxlc.ppm graphics/boxlr.ppm graphics/stkeys6.ppm \
	graphics/stkeys7.ppm graphics/stkeys8.ppm graphics/stcfn096.ppm graphics/m_butt1.ppm \
	graphics/m_butt2.ppm graphics/m_colors.ppm graphics/m_palno.ppm graphics/m_palsel.ppm \
	graphics/m_vbox.ppm graphics/cross1.ppm graphics/cross2.ppm graphics/cross3.ppm
WAD_FLATS := flats/-n0_tex-.ppm
WAD_SPRITEP := \
	0,0,sprites/tnt1a0.ppm \
	$(foreach f,1 2 3 4 5 6 7 8,33,66,sprites/dogsa$(f).ppm) \
	$(foreach f,1 2 3 4 5 6 7 8,33,66,sprites/dogsb$(f).ppm) \
	$(foreach f,1 2 3 4 5 6 7 8,33,66,sprites/dogsc$(f).ppm) \
	$(foreach f,1 2 3 4 5 6 7 8,33,66,sprites/dogsd$(f).ppm) \
	$(foreach f,1 2 3 4 5 6 7 8,33,66,sprites/dogse$(f).ppm) \
	$(foreach f,1 2 3 4 5 6 7 8,33,66,sprites/dogsf$(f).ppm) \
	$(foreach f,1 2 3 4 5 6 7 8,33,66,sprites/dogsg$(f).ppm) \
	$(foreach f,1 2 3 4 5 6 7 8,33,66,sprites/dogsh$(f).ppm) \
	33,67,sprites/dogsi0.ppm 33,67,sprites/dogsj0.ppm 33,67,sprites/dogsk0.ppm \
	33,67,sprites/dogsl0.ppm 33,68,sprites/dogsm0.ppm 33,69,sprites/dogsn0.ppm \
	9,11,sprites/pls1a0.ppm 8,11,sprites/pls1b0.ppm 9,11,sprites/pls1c0.ppm \
	8,11,sprites/pls1d0.ppm 16,27,sprites/pls1e0.ppm 16,27,sprites/pls1f0.ppm \
	18,27,sprites/pls1g0.ppm 9,11,sprites/pls2a0.ppm 8,13,sprites/pls2b0.ppm \
	11,18,sprites/pls2c0.ppm 16,27,sprites/pls2d0.ppm 18,27,sprites/pls2e0.ppm

.PHONY: all host doom-host run-doom-host test test-asan qnx qnx-check doom-package format check-static-init clean FORCE

all: doom-host test

$(BUILD_DIR):
	mkdir -p $@

# --- Internal resource WAD generator ---
$(BUILD_DIR)/rdatawad: $(RDATAWAD_SRC) | $(BUILD_DIR)
	$(CC) -O2 -Isrc/prboom_compat $(COMMON_DEFS) $(RDATAWAD_SRC) -o $@

$(BUILD_DIR)/prboom-plus.wad: $(BUILD_DIR)/rdatawad
	$(BUILD_DIR)/rdatawad -I $(PRBOOM_DATA) \
		-palette $(WAD_PALETTE) \
		-lumps $(WAD_LUMPS) \
		-marker C_START -lumps $(WAD_COLORMAPS) -marker C_END \
		-marker B_START -lumps $(WAD_TABLES) -marker B_END \
		-sounds $(WAD_SOUNDS) \
		-graphics $(WAD_GRAPHICS) \
		-marker FF_START -flats $(WAD_FLATS) -marker FF_END \
		-marker SS_START -sprites $(WAD_SPRITEP) -marker SS_END \
		-o $@

# --- Host compilation rules ---
$(OBJ_DIR)/host/prboom/%.o: $(PRBOOM_SRC)/%.c $(call stamp,host)
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_INC) $(COMMON_DEFS) $(PRBOOM_C_WARN) -MMD -MP -c $< -o $@

$(OBJ_DIR)/host/prboom/%.o: $(PRBOOM_SRC)/%.cpp $(call stamp,host)
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON_INC) $(COMMON_DEFS) $(PRBOOM_CXX_WARN) -MMD -MP -c $< -o $@

$(OBJ_DIR)/host/src/prboom_compat/w_wad_wrap.o: src/prboom_compat/w_wad_wrap.c $(call stamp,host)
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_INC) $(COMMON_DEFS) $(PRBOOM_C_WARN) -MMD -MP -c $< -o $@

$(OBJ_DIR)/host/src/prboom_compat/m_io_wrap.o: src/prboom_compat/m_io_wrap.c $(call stamp,host)
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_INC) $(COMMON_DEFS) $(PRBOOM_C_WARN) -MMD -MP -c $< -o $@

$(OBJ_DIR)/host/src/%.o: src/%.c $(call stamp,host)
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_INC) $(COMMON_DEFS) $(HOST_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/host/src/%.o: src/%.cpp $(call stamp,host)
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON_INC) $(COMMON_DEFS) $(HOST_CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/host/wad_embed.o: src/wad_embed.S $(BUILD_DIR)/prboom-plus.wad $(HAS_DOOM1_WAD) $(call stamp,host)
	@mkdir -p $(dir $@)
	$(CC) $(WAD_ASM_FLAGS) -c src/wad_embed.S -o $@

doom-host: $(BUILD_DIR)/nbt-doom-host

$(BUILD_DIR)/nbt-doom-host: $(HOST_PRBOOM_OBJ) $(HOST_OUR_OBJ) $(NBT_SDK_FRONT_HOST_LIBS)
	$(CXX) $^ $(LDFLAGS) $(NBT_SDK_HOST_LDLIBS) -o $@

run-doom-host: $(BUILD_DIR)/nbt-doom-host
	$(BUILD_DIR)/nbt-doom-host

# --- Host unit & integration tests ---
UNIT_CXX_SRC := \
	src/wad_vfs.cpp \
	src/wad_embed_reg.cpp \
	src/doom_settings.cpp \
	src/doom_log.cpp \
	src/doom_video.cpp \
	src/doom_controls.cpp \
	src/input_idrive.cpp \
	src/input_gamepad.cpp

UNIT_C_SRC := \
	src/prboom_compat/sdl_compat.c

$(OBJ_DIR)/host/tests/doom_test.o: tests/doom_test.cpp $(call stamp,host)
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON_INC) $(COMMON_DEFS) $(HOST_CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/doom-unit-test: $(OBJ_DIR)/host/tests/doom_test.o \
		$(patsubst %.cpp,$(OBJ_DIR)/host/%.o,$(UNIT_CXX_SRC)) \
		$(patsubst %.c,$(OBJ_DIR)/host/%.o,$(UNIT_C_SRC)) \
		$(OBJ_DIR)/host/wad_embed.o $(NBT_SDK_FRONT_HOST_LIBS)
	$(CXX) $^ $(LDFLAGS) $(NBT_SDK_HOST_LDLIBS) -o $@

$(BUILD_DIR)/doom-unit-asan: $(wildcard src/*.hpp) tests/doom_test.cpp $(UNIT_CXX_SRC) $(UNIT_C_SRC) src/wad_embed.S $(BUILD_DIR)/prboom-plus.wad $(HAS_DOOM1_WAD) $(NBT_SDK_FRONT_ASAN_LIBS)
	$(CXX) $(COMMON_INC) $(COMMON_DEFS) -std=c++11 -O1 -g -fno-exceptions -fno-rtti \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		$(WAD_ASM_FLAGS) \
		tests/doom_test.cpp $(UNIT_CXX_SRC) $(UNIT_C_SRC) src/wad_embed.S \
		$(NBT_SDK_FRONT_ASAN_LIBS) $(NBT_SDK_HOST_LDLIBS) -o $@

test: doom-host $(BUILD_DIR)/doom-unit-test
	$(BUILD_DIR)/doom-unit-test
	NBT_BUILD_DIR=$(abspath $(BUILD_DIR)) NBT_MGR_DIR=$(abspath $(NBT_MGR_DIR)) \
		python3 -m unittest discover -s tests -v

test-asan: $(BUILD_DIR)/doom-unit-asan
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		$(BUILD_DIR)/doom-unit-asan

host: doom-host $(BUILD_DIR)/doom-unit-test

check-static-init: doom-host
	@set -e; \
	for obj in $(patsubst %.cpp,$(OBJ_DIR)/host/%.o,$(OUR_CXX)); do \
		ctors="$$(readelf -S "$$obj" | awk '/\.init_array|\.ctors/ {print $$2}')"; \
		if test -n "$$ctors"; then \
			echo "ERROR: static constructor section in $$obj: $$ctors"; exit 1; \
		fi; \
	done; \
	echo "OK: no unexpected .init_array/.ctors in Doom's platform objects"
	+@$(NBT_SDK_MAKE) check-static-init

format:
	clang-format -i $(shell find src tests -maxdepth 2 \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \))

# --- QNX 6.5 ARMv7 cross-compilation ---
.PHONY: qnx-env
qnx-env:
	@test -n "$(QNX_ENV)" || { echo "QNX_ENV is not set: make QNX_ENV=/path/to/qnx-env.sh ..."; exit 1; }
	@test -r "$(QNX_ENV)" || { echo "QNX_ENV=$(QNX_ENV) is not readable"; exit 1; }

QNX_C_VARIANT := -Vgcc_ntoarmv7le
QNX_CPP_VARIANT := -Vgcc_ntoarmv7le_gpp
QNX_CFLAGS := $(QNX_C_VARIANT) -D_QNX_SOURCE -O2 -Wc,-std=gnu99 -w $(COMMON_INC) $(COMMON_DEFS)
QNX_OUR_CFLAGS := $(QNX_C_VARIANT) -D_QNX_SOURCE -O2 -Wc,-std=gnu99 -Wc,-Wall -Wc,-Wextra $(COMMON_INC) $(COMMON_DEFS)
QNX_PRBOOM_CXXFLAGS := $(QNX_CPP_VARIANT) -D_QNX_SOURCE -O2 -Wc,-std=c++11 -Wc,-fno-exceptions -Wc,-fno-rtti -w $(COMMON_INC) $(COMMON_DEFS)
QNX_OUR_CXXFLAGS := $(QNX_CPP_VARIANT) -D_QNX_SOURCE -O2 -Wc,-std=c++11 -Wc,-fno-exceptions -Wc,-fno-rtti -Wc,-Wall -Wc,-Wextra $(COMMON_INC) $(COMMON_DEFS)
QNX_DEPFLAGS = -Wp,-MMD,$(@:.o=.d) -Wp,-MP -Wp,-MT,$@

QNX_PRBOOM_OBJ := \
	$(patsubst %.c,$(OBJ_DIR)/qnx/prboom/%.o,$(PRBOOM_CORE_C)) \
	$(patsubst %.cpp,$(OBJ_DIR)/qnx/prboom/%.o,$(PRBOOM_CORE_CXX))

QNX_OUR_OBJ := \
	$(patsubst %.c,$(OBJ_DIR)/qnx/%.o,$(OUR_C)) \
	$(patsubst %.cpp,$(OBJ_DIR)/qnx/%.o,$(OUR_CXX)) \
	$(OBJ_DIR)/qnx/wad_embed.o

$(OBJ_DIR)/qnx/prboom/%.o: $(PRBOOM_SRC)/%.c $(call stamp,qnx) | qnx-env
	@mkdir -p $(dir $@)
	. "$(QNX_ENV)" && $(QCC) $(QNX_CFLAGS) $(QNX_DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/qnx/prboom/%.o: $(PRBOOM_SRC)/%.cpp $(call stamp,qnx) | qnx-env
	@mkdir -p $(dir $@)
	. "$(QNX_ENV)" && $(QCC) $(QNX_PRBOOM_CXXFLAGS) $(QNX_DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/qnx/src/prboom_compat/w_wad_wrap.o: src/prboom_compat/w_wad_wrap.c $(call stamp,qnx) | qnx-env
	@mkdir -p $(dir $@)
	. "$(QNX_ENV)" && $(QCC) $(QNX_CFLAGS) $(QNX_DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/qnx/src/prboom_compat/m_io_wrap.o: src/prboom_compat/m_io_wrap.c $(call stamp,qnx) | qnx-env
	@mkdir -p $(dir $@)
	. "$(QNX_ENV)" && $(QCC) $(QNX_CFLAGS) $(QNX_DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/qnx/src/%.o: src/%.c $(call stamp,qnx) | qnx-env
	@mkdir -p $(dir $@)
	. "$(QNX_ENV)" && $(QCC) $(QNX_OUR_CFLAGS) $(QNX_DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/qnx/src/%.o: src/%.cpp $(call stamp,qnx) | qnx-env
	@mkdir -p $(dir $@)
	. "$(QNX_ENV)" && $(QCC) $(QNX_OUR_CXXFLAGS) $(QNX_DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/qnx/wad_embed.o: src/wad_embed.S $(BUILD_DIR)/prboom-plus.wad $(HAS_DOOM1_WAD) $(call stamp,qnx) | qnx-env
	@mkdir -p $(dir $@)
	. "$(QNX_ENV)" && $(QCC) $(QNX_C_VARIANT) $(WAD_ASM_FLAGS) -c src/wad_embed.S -o $@

$(BUILD_DIR)/nbt-doom-qnx-unstripped: $(QNX_PRBOOM_OBJ) $(QNX_OUR_OBJ) $(NBT_SDK_FRONT_QNX_LIBS) | qnx-env
	@test -n "$(QNX_SCREEN_LIBDIR)" || { echo "QNX_SCREEN_LIBDIR is not set (directory with libscreen.so)"; exit 1; }
	. "$(QNX_ENV)" && $(QCC) $(QNX_CPP_VARIANT) -lang-c++ $(QNX_PRBOOM_OBJ) $(QNX_OUR_OBJ) $(NBT_SDK_FRONT_QNX_LIBS) -L$(QNX_SCREEN_LIBDIR) $(NBT_SDK_QNX_LDFLAGS) $(NBT_SDK_QNX_LDLIBS) -o $@

$(BUILD_DIR)/nbt-doom-qnx: $(BUILD_DIR)/nbt-doom-qnx-unstripped
	. "$(QNX_ENV)" && $(QNX_STRIP) -o $@ $<

qnx: $(BUILD_DIR)/nbt-doom-qnx

# --- A guest: built without the QNX SDP, run by nbt-runner as its own
# process (sdk/guest). Doom is GPL: the QNX licence forbids linking it with
# QNX code, so it must be a guest, never a module (the licence gate checks). ---
GUEST_APP := $(BUILD_DIR)/nbt-doom.guest
GUEST_APP_LICENSE := GPL-2.0-or-later
MOD_PRBOOM_CFLAGS := $(NBT_MOD_CFLAGS) -O2 -std=gnu99 -w $(COMMON_INC) $(COMMON_DEFS)
MOD_PRBOOM_CXXFLAGS := $(NBT_MOD_CXXFLAGS) -O2 -std=c++11 -w -fno-exceptions -fno-rtti $(COMMON_INC) $(COMMON_DEFS)
MOD_OUR_CFLAGS := $(NBT_MOD_CFLAGS) -O2 -std=gnu99 -Wall -Wextra $(COMMON_INC) $(COMMON_DEFS)
MOD_OUR_CXXFLAGS := $(NBT_MOD_CXXFLAGS) -O2 -std=c++11 -Wall -Wextra -fno-exceptions -fno-rtti $(COMMON_INC) \
	$(COMMON_DEFS)
MOD_PRBOOM_OBJ := $(patsubst %.c,$(OBJ_DIR)/mod/prboom/%.o,$(PRBOOM_CORE_C)) \
	$(patsubst %.cpp,$(OBJ_DIR)/mod/prboom/%.o,$(PRBOOM_CORE_CXX))
MOD_OUR_OBJ := $(patsubst %.c,$(OBJ_DIR)/mod/%.o,$(OUR_C)) $(patsubst %.cpp,$(OBJ_DIR)/mod/%.o,$(OUR_CXX)) \
	$(OBJ_DIR)/mod/wad_embed.o

$(OBJ_DIR)/mod/prboom/%.o: $(PRBOOM_SRC)/%.c $(call stamp,mod) | nbt-sdk-mod
	@mkdir -p $(dir $@)
	$(NBT_MOD_CC) $(MOD_PRBOOM_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/mod/prboom/%.o: $(PRBOOM_SRC)/%.cpp $(call stamp,mod) | nbt-sdk-mod
	@mkdir -p $(dir $@)
	$(NBT_MOD_CXX) $(MOD_PRBOOM_CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/mod/src/prboom_compat/%.o: src/prboom_compat/%.c $(call stamp,mod) | nbt-sdk-mod
	@mkdir -p $(dir $@)
	$(NBT_MOD_CC) $(MOD_PRBOOM_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/mod/src/%.o: src/%.c $(call stamp,mod) | nbt-sdk-mod
	@mkdir -p $(dir $@)
	$(NBT_MOD_CC) $(MOD_OUR_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/mod/src/%.o: src/%.cpp $(call stamp,mod) | nbt-sdk-mod
	@mkdir -p $(dir $@)
	$(NBT_MOD_CXX) $(MOD_OUR_CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/mod/wad_embed.o: src/wad_embed.S $(BUILD_DIR)/prboom-plus.wad $(HAS_DOOM1_WAD) $(call stamp,mod) | nbt-sdk-mod
	@mkdir -p $(dir $@)
	$(NBT_MOD_CC) $(NBT_MOD_ARCH) $(WAD_ASM_FLAGS) -c src/wad_embed.S -o $@

$(GUEST_APP): $(MOD_PRBOOM_OBJ) $(MOD_OUR_OBJ) $(NBT_SDK_MOD_DEPS)
	$(call nbt_guest_link,$@,$(MOD_PRBOOM_OBJ) $(MOD_OUR_OBJ),$(GUEST_APP_LICENSE))

.PHONY: guest vm-test
guest: $(GUEST_APP)

# --- The guest in QNX 6.5 SP1 under QEMU (the SDK's tools/qnx65-vm; no GPU,
# no sound: both faked): a health check, then the demo loop, which must draw
# frames and stop cleanly. ---
VM_PLAY := NBT_LAUNCH=user NBT_SURFACE=5 NBT_RUNNER_FAKE_GL=1 NBT_RUNNER_FAKE_AUDIO=1 NBT_DATA_DIR=/dev/shmem

vm-test: $(NBT_SDK_RUNNER) $(GUEST_APP)
	QEMU_RUN_SECS=150 $(NBT_SDK_ROOT)/tools/qnx65-vm/run.sh \
		-c "NBT_LAUNCH=check NBT_DATA_DIR=/dev/shmem nbt-runner --nbt-payload /proc/boot/$(notdir $(GUEST_APP)) &" \
		-c "waitfor /none 10" -c "slay -s TERM nbt-runner" -c "waitfor /none 3" \
		-c "$(VM_PLAY) nbt-runner --nbt-payload /proc/boot/$(notdir $(GUEST_APP)) &" \
		-c "waitfor /none 60" -c "slay -s TERM nbt-runner" -c "waitfor /none 3" \
		-x 'stage: check' -x 'fake-gl: closed: [1-9][0-9]* frames' -x 'runner: guest exited status=0' \
		$^

qnx-check: qnx
	. "$(QNX_ENV)" && $(QNX_READELF) -h -d $(BUILD_DIR)/nbt-doom-qnx
	. "$(QNX_ENV)" && set -e; \
		bad="$$($(QNX_NM) -u $(BUILD_DIR)/nbt-doom-qnx-unstripped | grep -E 'GLIBCXX_3\.4\.(1[4-9]|[2-9][0-9])|__cxa_throw' || true)"; \
		if test -n "$$bad"; then echo "Disallowed QNX ABI symbols:"; echo "$$bad"; exit 1; fi; \
		$(QNX_NM) -D $(BUILD_DIR)/nbt-doom-qnx-unstripped | grep -qw kdMain || { echo "kdMain is not exported"; exit 1; }; \
		echo "OK: QNX binary ELF/ABI symbols verified, kdMain exported"

# The package's executable: the guest, which NBT Manager's nbt-runner runs
# (bridge 1.7, runner). The QNX build stays for comparisons on the car.
PACKAGE_INPUT ?= $(GUEST_APP)

doom-package: $(PACKAGE_INPUT)
	@test -f "$(APP_KEY)" || { echo "APP_KEY must name the RSA signing key"; exit 1; }
	@test -f "$(NBT_MGR_DIR)/tools/package.py" || { echo "NBT_MGR_DIR=$(NBT_MGR_DIR) has no tools/package.py"; exit 1; }
	rm -f $(BUILD_DIR)/doom-$(APP_VERSION).nbt
	python3 $(NBT_MGR_DIR)/tools/package.py @release/package.args \
		--version "$(APP_VERSION)" \
		--sequence "$(APP_SEQ)" \
		--key "$(APP_KEY)" \
		--output $(BUILD_DIR)/doom-$(APP_VERSION).nbt \
		$(PACKAGE_INPUT)

clean:
	rm -rf $(BUILD_DIR)

DEPS := $(shell find $(OBJ_DIR) -name '*.d' 2>/dev/null)
-include $(DEPS)

$(call nbt_stamp,$(call stamp,host),$(CC) $(CXX) $(COMMON_INC) $(COMMON_DEFS) | $(PRBOOM_C_WARN) | $(PRBOOM_CXX_WARN) \
	| $(HOST_CFLAGS) | $(HOST_CXXFLAGS) | $(WAD_ASM_FLAGS),$(CC) $(CXX))
$(call nbt_stamp,$(call stamp,qnx),$(if $(QNX_ENV),$(QNX_ENV) $(QCC) $(QNX_CFLAGS) | $(QNX_OUR_CFLAGS) \
	| $(QNX_PRBOOM_CXXFLAGS) | $(QNX_OUR_CXXFLAGS) | $(WAD_ASM_FLAGS)))
$(call nbt_stamp,$(call stamp,mod),$(NBT_MOD_CC) $(MOD_PRBOOM_CFLAGS) | $(MOD_PRBOOM_CXXFLAGS) | $(MOD_OUR_CFLAGS) \
	| $(MOD_OUR_CXXFLAGS) | $(NBT_MOD_ARCH) $(WAD_ASM_FLAGS),,$(NBT_TOOLCHAIN)/VERSION)
