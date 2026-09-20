/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <zephyr/sys/util.h>

#include "battery.h"

int battery_percent(int millivolts)
{
	/*
	 * A lithium cell does not discharge linearly: it falls quickly at the top, sits on a long
	 * plateau, and drops away at the bottom. Points on a typical curve, with linear steps between.
	 */
	static const struct {
		int mv;
		int percent;
	} curve[] = {
		{3300, 0}, {3500, 8}, {3700, 25}, {3800, 42},
		{3900, 60}, {4000, 80}, {4100, 93}, {4200, 100},
	};

	if (millivolts <= curve[0].mv) {
		return 0;
	}
	for (size_t i = 1; i < ARRAY_SIZE(curve); i++) {
		if (millivolts <= curve[i].mv) {
			int span = curve[i].mv - curve[i - 1].mv;
			int into = millivolts - curve[i - 1].mv;

			return curve[i - 1].percent +
			       (curve[i].percent - curve[i - 1].percent) * into / span;
		}
	}
	return 100;
}
