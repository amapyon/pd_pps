# M31A diagnostic firmware: PD17 copied by assembler; M5 session + USBFS.
.DEFAULT_GOAL := all
TARGET := m34_pd_usbfs_phase_trace
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6
CH32FUN := ../../../ch32fun/ch32fun
FUSB_ROOT := $(CH32FUN)/../extralibs

PD17_C := src/pd_policy.c src/pd_phy_ch32x035.c src/adc_voltage.c \
          src/adc_calibration.c src/ina226.c src/pd_sensors.c \
          src/pd_vbus_history.c src/pd_vbus_monitor.c src/pd_hr_vbus.c \
          src/pd_hr_recovery.c src/pd_stability.c src/pd_stop_diag.c
M5_C := src/m5_bridge_runtime.c src/m5_pd_snapshot.c src/m5_readonly.c \
        src/m5_session.c src/m5_recovery_nb.c src/m5_usb_io.c
M28_C := src/m28_transport.c src/m28_runtime.c src/m28_usb.c m28_fsusb.c src/m30_diag.c src/m32_diag.c src/m33_diag.c src/m34_diag.c
ADDITIONAL_C_FILES := $(PD17_C) $(M5_C) $(M28_C)
EXTRA_CFLAGS += -Isrc -I. -I$(FUSB_ROOT)
CFLAGS += -Isrc
# Restore ch32fun size optimizations: CFLAGS += above suppresses its ?= defaults.
CFLAGS += -Os -flto -ffunction-sections -fdata-sections -fno-tree-loop-distribute-patterns
ifeq ($(wildcard $(CH32FUN)/ch32fun.mk),)
  $(error Pinned ch32fun submodule missing)
endif
ifeq ($(wildcard src/main.c),)
  $(error Assembled PD17 main missing: run Install-M28-Only.ps1)
endif
include $(CH32FUN)/ch32fun.mk
all: build
build: $(TARGET).bin
flash: cv_flash
clean: cv_clean
