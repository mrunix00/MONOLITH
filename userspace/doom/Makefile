TARGET := doom

APP_OPTFLAGS := -O0 --std=c99
APP_LIB_DEPS := libdesktop libgfx libc
LIBS := $(addprefix $(BUILD_DIR)/libs/,$(addsuffix .a,$(APP_LIB_DEPS)))

# PureDOOM — downloaded and cached, not a submodule
PUREDOOM_RAW_BASE := https://raw.githubusercontent.com/Daivuk/PureDOOM/master
PUREDOOM_CACHE_DIR := $(PROJECT_ROOT)/.cache/puredoom
PUREDOOM_H   := $(PUREDOOM_CACHE_DIR)/PureDOOM.h
PUREDOOM_WAD := $(PUREDOOM_CACHE_DIR)/doom1.wad
PUREDOOM_READY := $(PUREDOOM_CACHE_DIR)/.ready

EXTRA_INCLUDE_DIRS := $(PUREDOOM_CACHE_DIR)
POST_INSTALL_CMD := @mkdir -p $(INITRD_DIR)/assets && cp $(PUREDOOM_CACHE_DIR)/*.wad $(INITRD_DIR)/assets/ 2>/dev/null || true

include ../common-app.mk

# Download rules
$(PUREDOOM_CACHE_DIR):
	mkdir -p $@

$(PUREDOOM_H): | $(PUREDOOM_CACHE_DIR)
	wget -q --show-progress -O $@.tmp "$(PUREDOOM_RAW_BASE)/PureDOOM.h"
	mv $@.tmp $@

$(PUREDOOM_WAD): | $(PUREDOOM_CACHE_DIR)
	wget -q --show-progress -O $@.tmp "$(PUREDOOM_RAW_BASE)/doom1.wad"
	mv $@.tmp $@

$(PUREDOOM_READY): $(PUREDOOM_H) $(PUREDOOM_WAD)
	touch $@

# Ensure PureDOOM is fetched before any object files are compiled
# (include after common-app.mk so OBJ is defined)
$(OBJ): | $(PUREDOOM_READY)
