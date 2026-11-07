TARGET := pd_spr_sink_rebuild
ADDITIONAL_C_FILES := src/pd_policy.c src/pd_phy_ch32x035.c src/adc_voltage.c src/adc_calibration.c src/ina226.c src/pd_sensors.c src/pd_vbus_history.c src/pd_vbus_monitor.c src/pd_hr_vbus.c src/pd_hr_recovery.c src/pd_stability.c
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6
CH32FUN := ../ch32fun/ch32fun
CFLAGS += -Isrc
-include $(CH32FUN)/ch32fun.mk
all: build
flash: cv_flash
clean: cv_clean
.PHONY: test
test:
	cc -std=c11 -Wall -Wextra -Werror -pedantic -Isrc src/pd_policy.c tests/test_policy.c -o tests/test_policy
	./tests/test_policy
	$(RM) tests/test_policy
