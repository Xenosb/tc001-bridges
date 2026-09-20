/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <zephyr/logging/log.h>

#include "battery.h"
#include "power.h"
#include "power_core.h"

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

#define SAMPLE_MS 10000

static struct power_estimator estimator;
static bool initialised;
static bool have_reading;
static int millivolts;
static int percent;
static int64_t next_sample;

static void init_once(void)
{
	if (!initialised) {
		power_estimator_init(&estimator);
		initialised = true;
	}
}

void power_tick(int64_t now_ms)
{
	int mv;

	init_once();
	if (now_ms < next_sample) {
		return;
	}
	next_sample = now_ms + SAMPLE_MS;

	mv = battery_millivolts(NULL);
	if (mv < 0) {
		return;
	}

	millivolts = mv;
	percent = battery_percent(mv);
	have_reading = true;

	if (power_estimator_feed(&estimator, mv)) {
		LOG_INF("Power: %s (battery at %d mV)",
			estimator.on_power ? "external power" : "running on battery", mv);
	}
}

bool power_have_reading(void)
{
	return have_reading;
}

int power_millivolts(void)
{
	return millivolts;
}

int power_percent(void)
{
	return percent;
}

bool power_external(void)
{
	init_once();
	return estimator.on_power;
}

void power_load_changed(void)
{
	init_once();
	power_estimator_load_changed(&estimator);
}
