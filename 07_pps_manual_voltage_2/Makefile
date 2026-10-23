# 07 PPS interactive voltage console
all: build
TARGET := pps_manual_voltage
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6
CH32FUN := ../ch32fun/ch32fun
include $(CH32FUN)/ch32fun.mk
flash: cv_flash
clean: cv_clean
.PHONY: test

test:
	cc -std=c11 -Wall -Wextra -Werror -o /tmp/test_manual_parse tests/test_parse.c
	/tmp/test_manual_parse
	$(RM) /tmp/test_manual_parse
