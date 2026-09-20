/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

#include "light.h"

LOG_MODULE_REGISTER(light, LOG_LEVEL_INF);

/* The board's devicetree describes channel 7 of ADC1 (GPIO35, a photoresistor) as the light sensor */
static const struct adc_dt_spec adc = ADC_DT_SPEC_STRUCT(DT_NODELABEL(adc0), 7);

#define SAMPLES 8

int light_init(void)
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

int light_read(void)
{
	uint16_t sample;
	struct adc_sequence sequence = {
		.buffer = &sample,
		.buffer_size = sizeof(sample),
	};
	uint32_t sum = 0;
	int ret = adc_sequence_init_dt(&adc, &sequence);

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

	return sum / SAMPLES;
}
