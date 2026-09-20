/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_BATTERY_H_
#define TC001_BATTERY_H_

#include <stdbool.h>
#include <stdint.h>

/** Set up the battery voltage input. Returns 0 on success. */
int battery_init(void);

/**
 * Measure the battery voltage in millivolts, as an average of several samples. This is an
 * estimate (see CONFIG_TC001_BATTERY_UV_PER_COUNT).
 *
 * @param raw Receives the ADC count it was worked out from; may be NULL
 * @return the voltage in millivolts, or a negative errno on failure
 */
int battery_millivolts(uint16_t *raw);

/** Charge left, 0 to 100, for a lithium cell at @p millivolts. Pure function of the voltage. */
int battery_percent(int millivolts);

#endif /* TC001_BATTERY_H_ */
