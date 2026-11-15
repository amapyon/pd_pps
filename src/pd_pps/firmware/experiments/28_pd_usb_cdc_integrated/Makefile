# M28 independent firmware: PD17 copied by assembler; M5 session + USBFS.
.DEFAULT_GOAL := all
TARGET := m28_pd_usb_cdc
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
M28_C := src/m28_transport.c src/m28_runtime.c src/m28_usb.c m28_fsusb.c
ADDITIONAL_C_FILES := $(PD17_C) $(M5_C) $(M28_C)
EXTRA_CFLAGS += -Isrc -I. -I$(FUSB_ROOT)
CFLAGS += -Isrc
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
