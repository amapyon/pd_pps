# WeAct CH32X035F8U6 CoreBoard -- default USB-C VBUS 5 V test
all: build

TARGET := pps_5v
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6

CH32FUN := ../ch32fun/ch32fun
include $(CH32FUN)/ch32fun.mk

flash: cv_flash
clean: cv_clean
