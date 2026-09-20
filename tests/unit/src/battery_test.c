/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <zephyr/ztest.h>

int battery_percent(int millivolts);

ZTEST(battery, test_percent_landmarks)
{
	zassert_equal(battery_percent(4200), 100);
	zassert_equal(battery_percent(4350), 100, "above the curve stays at 100");
	zassert_equal(battery_percent(3300), 0);
	zassert_equal(battery_percent(3000), 0, "below the curve stays at 0");
	zassert_equal(battery_percent(3700), 25);
	zassert_equal(battery_percent(4000), 80);
}

ZTEST(battery, test_percent_rises_with_voltage_and_stays_in_range)
{
	int last = -1;

	for (int mv = 2800; mv <= 4400; mv += 5) {
		int p = battery_percent(mv);

		zassert_true(p >= 0 && p <= 100, "%d mV -> %d", mv, p);
		zassert_true(p >= last, "%d mV went down: %d after %d", mv, p, last);
		last = p;
	}
}

ZTEST(battery, test_steps_between_points_are_linear)
{
	/* Halfway between 3700 (25%) and 3800 (42%) */
	int p = battery_percent(3750);

	zassert_true(p == 33 || p == 34, "%d", p);
}

ZTEST_SUITE(battery, NULL, NULL, NULL, NULL, NULL);
