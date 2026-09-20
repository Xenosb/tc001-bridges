/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_POWER_CORE_H_
#define TC001_POWER_CORE_H_

#include <stdbool.h>

/*
 * Guessing whether the clock is on external power or running from its battery.
 *
 * There is no signal for it, only the battery voltage, so this looks at how the voltage moves.
 * Plugging in makes it jump up (the charger drives the battery), unplugging makes it drop, and
 * while charging it creeps up, while discharging it creeps down. Pure logic: the caller feeds it a
 * voltage at a fixed interval.
 *
 * The clock's own load also moves the voltage: switching the display off or on changes it by tens
 * of millivolts. The caller must call power_estimator_load_changed() when that happens, so the
 * estimator does not mistake it for a change of power source.
 */

#define POWER_WINDOW 60 /* samples kept: ten minutes at one sample every ten seconds */

/* Changes this large within about half a minute are a cable being plugged in or pulled out */
#define POWER_DROP_MV 40 /* battery voltage falling: unplugged */
#define POWER_RISE_MV 80 /* rising: plugged in. Larger, so switching the display off is not mistaken for it */

/* Slow drift over the window: discharging or charging */
#define POWER_TREND_MV      20
#define POWER_TREND_SAMPLES 30 /* the drift is only judged with at least this many samples */

struct power_estimator {
	int millivolts[POWER_WINDOW];
	int count;
	bool on_power;
};

/** Start out assuming external power: the display is only turned off on evidence of battery. */
void power_estimator_init(struct power_estimator *e);

/** Forget the voltages seen so far, keeping the current conclusion. */
void power_estimator_load_changed(struct power_estimator *e);

/**
 * Add a voltage reading.
 *
 * @return true if the conclusion changed
 */
bool power_estimator_feed(struct power_estimator *e, int millivolts);

#endif /* TC001_POWER_CORE_H_ */
