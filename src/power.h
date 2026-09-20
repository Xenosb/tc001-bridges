/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_POWER_H_
#define TC001_POWER_H_

#include <stdbool.h>
#include <stdint.h>

/*
 * The battery and where the clock's power comes from. Measured continuously, whichever app is on
 * display, because the display turns off on battery. Call power_tick() from the display thread: it
 * shares the ADC with the light sensor, so both are read from that one thread.
 */

/** Sample the battery if it is time to. Cheap to call often. */
void power_tick(int64_t now_ms);

/** Whether a reading has been taken yet. */
bool power_have_reading(void);

/** Battery voltage in millivolts, an estimate (see CONFIG_TC001_BATTERY_UV_PER_COUNT). */
int power_millivolts(void);

/** Charge left, 0 to 100. */
int power_percent(void);

/**
 * Whether the clock seems to be on external power, so the battery is charging or full. A guess from
 * how the battery voltage moves (see power_core.h). True until there is evidence of battery.
 */
bool power_external(void);

/** The clock's own load is about to change, for example the display going off: see power_core.h. */
void power_load_changed(void);

#endif /* TC001_POWER_H_ */
