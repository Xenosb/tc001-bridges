/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <zephyr/ztest.h>

#include "power_core.h"

/* A small deterministic "noise" of a few millivolts, like the real ADC readings have */
static int noise(int i)
{
	static const int pattern[] = {0, 4, -3, 6, -5, 2, -1, 5, -4, 1};

	return pattern[i % 10];
}

/* Feed @p n samples of @p base plus @p slope_mv_per_sample per sample. Returns the last change. */
static bool run(struct power_estimator *e, int base, double slope, int n, int *changes)
{
	bool last = false;

	for (int i = 0; i < n; i++) {
		if (power_estimator_feed(e, base + (int)(slope * i) + noise(i))) {
			last = true;
			if (changes != NULL) {
				(*changes)++;
			}
		}
	}
	return last;
}

ZTEST(power, test_starts_on_power)
{
	struct power_estimator e;

	power_estimator_init(&e);
	zassert_true(e.on_power);
}

ZTEST(power, test_a_steady_charged_battery_on_the_charger_stays_on_power)
{
	struct power_estimator e;
	int changes = 0;

	power_estimator_init(&e);
	run(&e, 4180, 0, 200, &changes);
	zassert_true(e.on_power);
	zassert_equal(changes, 0);
}

ZTEST(power, test_unplugging_while_charging_is_seen_within_half_a_minute)
{
	struct power_estimator e;
	int changes = 0;

	power_estimator_init(&e);
	run(&e, 4100, 0, 20, &changes); /* charging at 4.10 V */
	zassert_true(e.on_power);

	/* Pulled out: the charger stops driving the battery, 4.10 V falls to 3.98 V at once */
	for (int i = 0; i < 4; i++) {
		power_estimator_feed(&e, 3980 + noise(i));
	}
	zassert_false(e.on_power, "should be on battery after four samples (40 s)");
}

ZTEST(power, test_plugging_in_is_seen_quickly)
{
	struct power_estimator e;

	power_estimator_init(&e);
	run(&e, 3900, 0, 12, NULL);
	e.on_power = false; /* it had concluded battery */

	for (int i = 0; i < 4; i++) {
		power_estimator_feed(&e, 4150 + noise(i)); /* the charger raises it by 250 mV */
	}
	zassert_true(e.on_power);
}

ZTEST(power, test_steady_discharge_on_battery_is_seen_after_minutes)
{
	struct power_estimator e;
	bool seen = false;

	power_estimator_init(&e);
	/* Falls 40 mV over ten minutes: 0.67 mV every ten seconds. No sudden edge, only a drift. */
	for (int i = 0; i < 60; i++) {
		power_estimator_feed(&e, 3900 - (int)(0.67 * i) + noise(i));
		if (!e.on_power && !seen) {
			seen = true;
			zassert_true(i >= POWER_TREND_SAMPLES - 1, "not before the drift can be judged");
		}
	}
	zassert_true(seen, "the drift should have shown it is on battery");
}

ZTEST(power, test_noise_alone_never_changes_the_conclusion)
{
	struct power_estimator e;
	int changes = 0;

	power_estimator_init(&e);
	run(&e, 4000, 0, 500, &changes);
	zassert_equal(changes, 0);

	e.on_power = false;
	run(&e, 4000, 0, 500, &changes);
	zassert_equal(changes, 0);
	zassert_false(e.on_power);
}

ZTEST(power, test_switching_the_display_off_is_not_taken_for_plugging_in)
{
	struct power_estimator e;

	power_estimator_init(&e);
	run(&e, 3900, -0.5, 40, NULL);
	e.on_power = false; /* on battery */

	/* Display off: the load drops and the voltage rises by 30 mV. The caller says so first. */
	power_estimator_load_changed(&e);
	run(&e, 3930, -0.5, 60, NULL);
	zassert_false(e.on_power, "a load change must not look like a cable");
}

ZTEST(power, test_without_the_load_change_notice_a_big_enough_step_would_fool_it)
{
	/* This is why power_estimator_load_changed() exists and why the rise threshold is large */
	struct power_estimator e;

	power_estimator_init(&e);
	e.on_power = false;
	run(&e, 3900, 0, 10, NULL);
	for (int i = 0; i < 4; i++) {
		power_estimator_feed(&e, 3900 + 30 + noise(i));
	}
	zassert_false(e.on_power, "a 30 mV step is below the plug-in threshold");
}

ZTEST(power, test_charging_creep_after_battery_is_seen_as_power)
{
	struct power_estimator e;

	power_estimator_init(&e);
	e.on_power = false;
	/* Rises 30 mV over five minutes: charging slowly, no sudden jump */
	for (int i = 0; i < 40; i++) {
		power_estimator_feed(&e, 3800 + (int)(1.0 * i) + noise(i));
	}
	zassert_true(e.on_power);
}

ZTEST_SUITE(power, NULL, NULL, NULL, NULL, NULL);
