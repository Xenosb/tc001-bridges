/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "brightness.h"

/* The sensor counts at which the display should be at its dimmest and at its brightest */
#define DARK_COUNT   CONFIG_TC001_LDR_DARK_COUNT
#define BRIGHT_COUNT CONFIG_TC001_LDR_BRIGHT_COUNT

/* Integer natural logarithm times 1000, good enough to shape a brightness curve */
static int ln_milli(int x)
{
	/* ln(x) = k*ln(2) + ln(m) for x = m * 2^k, m in [1, 2); ln(m) from a short series */
	int k = 0;
	long m;
	long y;
	long term;
	long sum;

	if (x < 1) {
		x = 1;
	}
	m = x * 1000L;
	while (m >= 2000) {
		m /= 2;
		k++;
	}
	/* ln(m) = 2 * (z + z^3/3 + z^5/5) with z = (m - 1) / (m + 1) */
	y = (m - 1000) * 1000 / (m + 1000);
	term = y;
	sum = y;
	term = term * y / 1000 * y / 1000;
	sum += term / 3;
	term = term * y / 1000 * y / 1000;
	sum += term / 5;

	return (int)(k * 693 + 2 * sum);
}

int brightness_from_light(int light_raw)
{
	/* The sensor's counts run one way or the other depending on the wiring, see Kconfig */
	int dark = DARK_COUNT;
	int bright = BRIGHT_COUNT;
	int span_dark = ln_milli(dark) - ln_milli(bright);
	int position;

	if (light_raw < 1) {
		light_raw = 1;
	}
	/* How far from dark towards bright, 0 to 1000, on a logarithmic scale like the eye's */
	position = (ln_milli(dark) - ln_milli(light_raw)) * 1000 / span_dark;
	if (position < 0) {
		position = 0;
	} else if (position > 1000) {
		position = 1000;
	}

	return BRIGHTNESS_MIN + (BRIGHTNESS_MAX - BRIGHTNESS_MIN) * position / 1000;
}

int brightness_target(const struct config *cfg, int light_raw)
{
	int target = cfg->auto_brightness ? brightness_from_light(light_raw) + cfg->brightness_bias
					  : cfg->brightness;

	if (target < BRIGHTNESS_MIN) {
		return BRIGHTNESS_MIN;
	}
	return target > BRIGHTNESS_MAX ? BRIGHTNESS_MAX : target;
}
