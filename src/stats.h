/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_STATS_H_
#define TC001_STATS_H_

#include <stddef.h>
#include <stdint.h>

enum stat_bridge {
	STAT_BRIDGE_RUST,
	STAT_BRIDGE_CSHARP,
	STAT_BRIDGE_COUNT,
};

enum stat_metric {
	STAT_METRIC_STARS,
	STAT_METRIC_DOWNLOADS,
	STAT_METRIC_COUNT,
};

struct stat_item {
	enum stat_bridge bridge;
	enum stat_metric metric;
	uint32_t value;
};

#define STATS_MAX_ITEMS (STAT_BRIDGE_COUNT * STAT_METRIC_COUNT)

/**
 * Query GitHub, crates.io and NuGet directly and update the stats.
 *
 * A source that fails keeps the value it had before, so a flaky request never
 * blanks the display. `items` always receives every value known so far.
 *
 * @param items     Output array of at least STATS_MAX_ITEMS entries
 * @param count     Number of valid entries written to @p items
 * @return 0 if at least one source answered, -EIO if none did
 */
int stats_refresh(struct stat_item *items, size_t *count);

#endif /* TC001_STATS_H_ */
