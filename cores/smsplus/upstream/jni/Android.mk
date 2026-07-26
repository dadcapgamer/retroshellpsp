LOCAL_PATH := $(call my-dir)

SOUND_ENGINE             := maxim_sn76489
Z80_CORE                 := z80
ZIP_SUPPORT              := 0
DEBUG                    := 0
FLAGS                    :=

CORE_DIR := $(LOCAL_PATH)/..

include $(CORE_DIR)/Makefile.common

COREFLAGS := $(INCFLAGS) $(CFLAGS)
COREFLAGS += -D__LIBRETRO__ -DALIGN_DWORD -DLSB_FIRST

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
  COREFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE       := retro
LOCAL_SRC_FILES    := $(SOURCES_CXX) $(SOURCES_C)
LOCAL_CFLAGS       := $(COREFLAGS)
LOCAL_CXXFLAGS     := $(COREFLAGS)
LOCAL_LDFLAGS      := -Wl,-version-script=$(CORE_DIR)/link.T
include $(BUILD_SHARED_LIBRARY)
