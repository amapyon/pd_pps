# WeAct CH32X035F8U6 - PPS current-limit request experiment
all: build

TARGET := pps_current_limit
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6

CH32FUN := ../ch32fun/ch32fun
include $(CH32FUN)/ch32fun.mk

flash: cv_flash
clean: cv_clean

.PHONY: test-rdo
test-rdo:
	cc -std=c11 -Wall -Wextra -Werror -o test_rdo tests/test_rdo.c
	./test_rdo
	$(RM) test_rdo
