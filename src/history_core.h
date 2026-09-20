/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_HISTORY_CORE_H_
#define TC001_HISTORY_CORE_H_

#include <stdbool.h>
#include <stdint.h>

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

/*
 * A daily record of the numbers, to work out how much they changed. One entry per day holds the
 * last value seen that day for each metric. Pure logic: the day number is passed in, and where
 * the data is kept is up to the caller (see history.c).
 */

#define HISTORY_DAYS    40
#define HISTORY_METRICS 4

/* How far before the wanted day the entry used for a change may be: the clock may have been off */
#define HISTORY_TOLERANCE_DAYS 3

struct history_entry {
	uint32_t day; /* any monotonic day count, e.g. days since 1970-01-01 in local time */
	uint32_t value[HISTORY_METRICS];
	uint8_t valid; /* bit n set: value[n] has been recorded that day */
};

struct history {
	uint8_t count;
	struct history_entry entry[HISTORY_DAYS]; /* oldest first, days strictly increasing */
};

/**
 * Record @p value of @p metric for @p day. Overwrites what was recorded for that day, and drops
 * the oldest day when the record is full. Days earlier than the newest one are ignored.
 *
 * @return true if the stored data changed, so it is worth saving
 */
bool history_record(struct history *h, uint32_t day, int metric, uint32_t value);

/**
 * How much @p metric grew (negative: shrank) between @p days_back days before @p today and now,
 * given its @p current value.
 *
 * @return false if there is no record from close enough to that day
 */
bool history_change(const struct history *h, uint32_t today, int metric, int days_back,
		    uint32_t current, int64_t *change);

#endif /* TC001_HISTORY_CORE_H_ */
