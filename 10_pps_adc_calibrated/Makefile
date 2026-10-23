# v3-derived: manual PPS voltage AND current, with reconnect support.
all: build
TARGET := pps_manual_voltage
ADDITIONAL_C_FILES := adc_voltage.c adc_calibration.c
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6
CH32FUN := ../ch32fun/ch32fun
-include $(CH32FUN)/ch32fun.mk
flash: cv_flash
clean: cv_clean
.PHONY: test

test:
	cc -std=c11 -Wall -Wextra -Werror -o /tmp/test_manual_parse_reconnect tests/test_parse.c
	/tmp/test_manual_parse_reconnect
	$(RM) /tmp/test_manual_parse_reconnect
	cc -std=c11 -Wall -Wextra -Werror -o /tmp/test_manual_reconnect tests/test_reconnect.c
	/tmp/test_manual_reconnect
	$(RM) /tmp/test_manual_reconnect

	cc -std=c11 -Wall -Wextra -Werror -o /tmp/test_manual_vi_commands tests/test_commands.c
	/tmp/test_manual_vi_commands
	$(RM) /tmp/test_manual_vi_commands
	cc -std=c11 -Wall -Wextra -Werror -o /tmp/test_manual_pps_rdo tests/test_rdo.c
	/tmp/test_manual_pps_rdo
	$(RM) /tmp/test_manual_pps_rdo

	cc -std=c11 -Wall -Wextra -Werror -o /tmp/test_manual_apdo tests/test_apdo.c
	/tmp/test_manual_apdo
	$(RM) /tmp/test_manual_apdo

	cc -std=c11 -Wall -Wextra -Werror -I. -o /tmp/test_adc_calibration tests/test_adc_calibration.c adc_calibration.c
	/tmp/test_adc_calibration
	$(RM) /tmp/test_adc_calibration
