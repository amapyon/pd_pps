# v3-derived: manual PPS voltage AND current, with reconnect support.
all: build
TARGET := pps_oled_encoder_pps_control
ADDITIONAL_C_FILES := adc_voltage.c adc_calibration.c ina226.c m2_bus_adapter.c m2_ssd1306.c m2_dashboard.c m3_input.c m3_gpio.c m4_ui.c m4_screen.c m4_apply_guard.c
TARGET_MCU := CH32X035
TARGET_MCU_PACKAGE := CH32X035F8U6
CH32FUN := ../ch32fun/ch32fun
-include $(CH32FUN)/ch32fun.mk
flash: cv_flash
clean: cv_clean
.PHONY: test

test: test_caps
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

	cc -std=c11 -Wall -Wextra -Werror -I. -o /tmp/test_ina226_units tests/test_ina226_units.c
	/tmp/test_ina226_units
	$(RM) /tmp/test_ina226_units

	cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 -I. -o /tmp/test_m2_oled_host tests/test_oled_host.c m2_bus_adapter.c m2_ssd1306.c m2_dashboard.c
	/tmp/test_m2_oled_host
	$(RM) /tmp/test_m2_oled_host
	cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 -I. -o /tmp/test_m2_oled_bus tests/test_oled_bus.c m2_bus_adapter.c
	/tmp/test_m2_oled_bus
	$(RM) /tmp/test_m2_oled_bus

	cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 -I. -o /tmp/test_m3_input tests/test_m3_input.c m3_input.c
	/tmp/test_m3_input
	$(RM) /tmp/test_m3_input

	cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 -I. -o /tmp/test_m4_ui tests/test_m4_ui.c m4_ui.c
	/tmp/test_m4_ui
	$(RM) /tmp/test_m4_ui

	cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 -I. -o /tmp/test_m4_screen tests/test_m4_screen.c m4_screen.c m4_ui.c m2_ssd1306.c
	/tmp/test_m4_screen
	$(RM) /tmp/test_m4_screen

	cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 -I. -o /tmp/test_m4b_apply_guard tests/test_m4b_apply_guard.c m4_apply_guard.c
	/tmp/test_m4b_apply_guard
	$(RM) /tmp/test_m4b_apply_guard

# M4-C: Source_Capabilities defensive validation (host-only).
test_caps:
	cc -std=c11 -Wall -Wextra -Werror -pedantic -O2 -I. -o /tmp/test_pd_caps_integrity tests/test_pd_caps_integrity.c
	/tmp/test_pd_caps_integrity
	$(RM) /tmp/test_pd_caps_integrity

.PHONY: test_caps
