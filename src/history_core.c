/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include "history_core.h"

bool history_record(struct history *h, uint32_t day, int metric, uint32_t value)
{
	struct history_entry *e;

	if (metric < 0 || metric >= HISTORY_METRICS) {
		return false;
	}

	if (h->count > 0 && day < h->entry[h->count - 1].day) {
		return false; /* the clock went backwards: do not disturb what is recorded */
	}

	if (h->count > 0 && day == h->entry[h->count - 1].day) {
		e = &h->entry[h->count - 1];
		if ((e->valid & BIT(metric)) && e->value[metric] == value) {
			return false;
		}
	} else {
		if (h->count == HISTORY_DAYS) {
			memmove(&h->entry[0], &h->entry[1], (HISTORY_DAYS - 1) * sizeof(h->entry[0]));
			h->count--;
		}
		e = &h->entry[h->count++];
		memset(e, 0, sizeof(*e));
		e->day = day;
	}

	e->value[metric] = value;
	e->valid |= BIT(metric);
	return true;
}

bool history_change(const struct history *h, uint32_t today, int metric, int days_back,
		    uint32_t current, int64_t *change)
{
	uint32_t wanted;

	if (metric < 0 || metric >= HISTORY_METRICS || days_back < 0 || today < (uint32_t)days_back) {
		return false;
	}
	wanted = today - days_back;

	/* Newest first: the latest record on or before the wanted day */
	for (int i = h->count - 1; i >= 0; i--) {
		const struct history_entry *e = &h->entry[i];

		if (e->day > wanted || !(e->valid & BIT(metric))) {
			continue;
		}
		if (wanted - e->day > HISTORY_TOLERANCE_DAYS) {
			return false;
		}
		*change = (int64_t)current - (int64_t)e->value[metric];
		return true;
	}

	return false;
}
