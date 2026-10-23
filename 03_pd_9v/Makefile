# Build inside ~/pj/pps/03_pd_9v; reuse the existing sibling ch32fun.
all: build

TARGET := pd_9v
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6

CH32FUN := ../ch32fun/ch32fun
include $(CH32FUN)/ch32fun.mk

flash: cv_flash
clean: cv_clean
