# WeAct CH32X035F8U6 - PPS 5.00 <-> 11.00 V sweep, 0.50 V steps
all: build

TARGET := pps_sweep
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6

CH32FUN := ../ch32fun/ch32fun
include $(CH32FUN)/ch32fun.mk

flash: cv_flash
clean: cv_clean
