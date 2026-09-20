/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "brightness.h"
#include "gfx.h"
#include "light.h"

/* Percentage points per tick: a room light turning on fades in rather than jumping */
#define MAX_STEP 3

static int current;

static int wanted(void)
{
	const struct config *cfg = config_get();
	int raw = 0;

	if (cfg->auto_brightness) {
		raw = light_read();
		if (raw < 0) {
			return current; /* no reading: leave things as they are */
		}
	}
	return brightness_target(cfg, raw);
}

void brightness_init(void)
{
	current = wanted();
	if (current < BRIGHTNESS_MIN) {
		current = config_get()->brightness;
	}
	gfx_set_brightness(current);
}

bool brightness_tick(void)
{
	int target = wanted();
	int step = target - current;

	if (step == 0) {
		return false;
	}
	if (step > MAX_STEP) {
		step = MAX_STEP;
	} else if (step < -MAX_STEP) {
		step = -MAX_STEP;
	}

	current += step;
	gfx_set_brightness(current);
	return true;
}
