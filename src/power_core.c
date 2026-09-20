/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include "power_core.h"

void power_estimator_init(struct power_estimator *e)
{
	memset(e, 0, sizeof(*e));
	e->on_power = true;
}

void power_estimator_load_changed(struct power_estimator *e)
{
	e->count = 0;
}

/* Average of @p n readings starting at index @p from */
static int average(const struct power_estimator *e, int from, int n)
{
	long sum = 0;

	for (int i = from; i < from + n; i++) {
		sum += e->millivolts[i];
	}
	return sum / n;
}

bool power_estimator_feed(struct power_estimator *e, int millivolts)
{
	bool was = e->on_power;

	if (e->count == POWER_WINDOW) {
		memmove(&e->millivolts[0], &e->millivolts[1], (POWER_WINDOW - 1) * sizeof(int));
		e->count--;
	}
	e->millivolts[e->count++] = millivolts;

	/* A jump within the last half minute or so: the reading now against two readings before that */
	if (e->count >= 4) {
		int jump = millivolts - average(e, e->count - 4, 2);

		if (e->on_power && jump <= -POWER_DROP_MV) {
			e->on_power = false;
		} else if (!e->on_power && jump >= POWER_RISE_MV) {
			e->on_power = true;
		}
	}

	/* Slow drift: the latest few readings against the first few in the window */
	if (e->count >= POWER_TREND_SAMPLES) {
		int drift = average(e, e->count - 3, 3) - average(e, 0, 3);

		if (e->on_power && drift <= -POWER_TREND_MV) {
			e->on_power = false;
		} else if (!e->on_power && drift >= POWER_TREND_MV) {
			e->on_power = true;
		}
	}

	return e->on_power != was;
}
