# M38B USBPD-disabled control, M38A USBFS lower layer unchanged.
.DEFAULT_GOAL := all
TARGET := m38b_pd_off_min_cdc
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6
CH32FUN := ../../../ch32fun/ch32fun
FUSB_ROOT := $(CH32FUN)/../extralibs

# Deliberately NO PD PHY, PD Policy Engine, PD safety or ADC source files.
ADDITIONAL_C_FILES := src/m38b_usb.c m38b_fsusb.c
EXTRA_CFLAGS += -Isrc -I. -I$(FUSB_ROOT)
CFLAGS += -Isrc -Os -flto -ffunction-sections -fdata-sections -fno-tree-loop-distribute-patterns
ifeq ($(wildcard $(CH32FUN)/ch32fun.mk),)
  $(error Pinned ch32fun submodule missing)
endif
include $(CH32FUN)/ch32fun.mk
all: build
build: $(TARGET).bin
flash: cv_flash
clean: cv_clean
