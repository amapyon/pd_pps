# M41: M40 + read-only USBPD register/RX diagnostics (observational IRQ snapshots).
.DEFAULT_GOAL := all
TARGET := m41_pd_rx_register_cdc
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6
CH32FUN := ../../../ch32fun/ch32fun
FUSB_ROOT := $(CH32FUN)/../extralibs

PD17_C := src/pd_policy.c src/pd_phy_ch32x035.c src/adc_voltage.c \
          src/adc_calibration.c src/ina226.c src/pd_sensors.c \
          src/pd_vbus_history.c src/pd_vbus_monitor.c src/pd_hr_vbus.c \
          src/pd_hr_recovery.c src/pd_stability.c src/pd_stop_diag.c
ADDITIONAL_C_FILES := $(PD17_C) src/m38_usb.c m28_fsusb.c
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
