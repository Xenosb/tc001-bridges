/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

#include "battery.h"

LOG_MODULE_REGISTER(battery, LOG_LEVEL_INF);

/* The board's devicetree describes channel 6 of ADC1 (GPIO34) as the battery */
static const struct adc_dt_spec adc = ADC_DT_SPEC_STRUCT(DT_NODELABEL(adc0), 6);

/* The ADC is noisy: average this many samples */
#define SAMPLES 16

int battery_init(void)
{
	int ret;

	if (!adc_is_ready_dt(&adc)) {
		LOG_ERR("ADC not ready");
		return -ENODEV;
	}

	ret = adc_channel_setup_dt(&adc);
	if (ret) {
		LOG_ERR("ADC channel setup failed: %d", ret);
	}
	return ret;
}

int battery_millivolts(uint16_t *raw_out)
{
	uint16_t sample;
	struct adc_sequence sequence = {
		.buffer = &sample,
		.buffer_size = sizeof(sample),
	};
	uint32_t sum = 0;
	int ret;

	ret = adc_sequence_init_dt(&adc, &sequence);
	if (ret) {
		return ret;
	}

	for (int i = 0; i < SAMPLES; i++) {
		ret = adc_read_dt(&adc, &sequence);
		if (ret) {
			return ret;
		}
		sum += sample;
	}

	if (raw_out != NULL) {
		*raw_out = sum / SAMPLES;
	}

	return (int)((uint64_t)(sum / SAMPLES) * CONFIG_TC001_BATTERY_UV_PER_COUNT / 1000);
}
