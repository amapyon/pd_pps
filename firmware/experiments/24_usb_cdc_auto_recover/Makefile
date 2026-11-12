# M8F USB CDC recovery experiment 24, independent from experiments 21-23.
.DEFAULT_GOAL := all
TARGET := m8f_usb_cdc_auto_recover
TARGET_MCU := CH32X035
CH32FUN := ../../../ch32fun/ch32fun
FUSB_ROOT := $(CH32FUN)/../extralibs
ADDITIONAL_C_FILES += m8e_fsusb.c m8d_trace.c m8e_irq_trace.c m8f_recovery.c m8_measurement.c m8_ina226.c
EXTRA_CFLAGS += -I$(FUSB_ROOT) -I.
ifeq ($(wildcard $(CH32FUN)/ch32fun.mk),)
$(error Missing pinned ch32fun.mk: $(CH32FUN)/ch32fun.mk)
endif
ifeq ($(wildcard m8e_fsusb.c),)
$(error Missing local instrumented USBFS source; run Install-M8F-Only.ps1)
endif
include $(CH32FUN)/ch32fun.mk
all: build
build: $(TARGET).bin
flash: cv_flash
clean: cv_clean
