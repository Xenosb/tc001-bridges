/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_HISTORY_H_
#define TC001_HISTORY_H_

#include <stdbool.h>
#include <stdint.h>

#include "stats.h"

/** Load the saved daily record from flash. Call once, early. */
void history_init(void);

/**
 * Record today's numbers. Does nothing until the clock knows the date, since a day number is
 * needed. Saves to flash only when something changed.
 */
void history_record_items(const struct stat_item *items, size_t count);

/**
 * How much a number changed over the last @p days_back days, given its @p current value.
 *
 * @return false if the record does not reach back that far (yet)
 */
bool history_change_over(enum stat_bridge bridge, enum stat_metric metric, int days_back,
			 uint32_t current, int64_t *change);

#endif /* TC001_HISTORY_H_ */
