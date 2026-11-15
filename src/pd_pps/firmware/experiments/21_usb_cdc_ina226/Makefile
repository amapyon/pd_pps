# M8 USBFS CDC-ACM commands for PD PPS workspace.
# The usual dev-workspace/pd-pps.ps1 task builds TARGET.bin and flashes it.
# VS Code: PD PPS: Build (select project) / Build and flash (select project).
.DEFAULT_GOAL := all
TARGET := m8_usb_cdc_measure
TARGET_MCU := CH32X035

# Repo root is ../../.. from firmware/experiments/21_usb_cdc_ina226.
# Git submodule is located in repo/ch32fun/ch32fun.
CH32FUN := ../../../ch32fun/ch32fun
FUSB_ROOT := $(CH32FUN)/../extralibs
FUSB_CDC_DEMO := $(CH32FUN)/../examples_usb/USBFS/usbfs_cdc_tty

ADDITIONAL_C_FILES += $(FUSB_ROOT)/fsusb.c m8_measurement.c m8_ina226.c
EXTRA_CFLAGS += -I$(FUSB_ROOT) -I.

ifeq ($(wildcard $(CH32FUN)/ch32fun.mk),)
$(error ch32fun.mk not found: $(CH32FUN)/ch32fun.mk. Check submodule location)
endif
ifeq ($(wildcard usb_config.h),)
$(error M8 local usb_config.h not found. Install experiment 21 files)
endif

include $(CH32FUN)/ch32fun.mk

all: build
build: $(TARGET).bin
flash: cv_flash
clean: cv_clean
