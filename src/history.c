/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "clock.h"
#include "history.h"
#include "history_core.h"

LOG_MODULE_REGISTER(history, LOG_LEVEL_INF);

/* Bump when struct saved or the meaning of the metric numbers changes */
#define SAVED_VERSION 1

BUILD_ASSERT(STATS_MAX_ITEMS <= HISTORY_METRICS, "the record needs a slot per number");

struct saved {
	uint8_t version;
	struct history history;
};

static K_MUTEX_DEFINE(lock);
static struct history record;

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	struct saved saved;

	if (strcmp(name, "v") != 0 || len != sizeof(saved) ||
	    read_cb(cb_arg, &saved, sizeof(saved)) != sizeof(saved) || saved.version != SAVED_VERSION ||
	    saved.history.count > HISTORY_DAYS) {
		return 0;
	}

	record = saved.history;
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(tc001_hist, "tc001_hist", NULL, settings_set, NULL, NULL);

static int metric_index(enum stat_bridge bridge, enum stat_metric metric)
{
	return bridge * STAT_METRIC_COUNT + metric;
}

/* Days since 1970-01-01 in local time, or false if the clock is not set */
static bool today(uint32_t *day)
{
	struct civil_time t;

	if (!clock_local(&t)) {
		return false;
	}
	*day = timeconv_to_unix(&t) / 86400;
	return true;
}

/* Caller holds the lock */
static void save(void)
{
	struct saved saved = {.version = SAVED_VERSION, .history = record};
	int ret = settings_save_one("tc001_hist/v", &saved, sizeof(saved));

	if (ret) {
		LOG_ERR("Saving the history failed: %d", ret);
	}
}

void history_init(void)
{
	int ret = settings_subsys_init();

	if (ret == 0) {
		ret = settings_load_subtree("tc001_hist");
	}
	if (ret) {
		LOG_WRN("Loading the history failed: %d", ret);
		return;
	}
	LOG_INF("History: %u day(s) recorded", record.count);
}

void history_record_items(const struct stat_item *items, size_t count)
{
	uint32_t day;
	bool changed = false;

	if (!today(&day)) {
		return;
	}

	k_mutex_lock(&lock, K_FOREVER);
	for (size_t i = 0; i < count; i++) {
		changed |= history_record(&record, day, metric_index(items[i].bridge, items[i].metric),
					  items[i].value);
	}
	if (changed) {
		save();
	}
	k_mutex_unlock(&lock);
}

bool history_change_over(enum stat_bridge bridge, enum stat_metric metric, int days_back,
			 uint32_t current, int64_t *change)
{
	uint32_t day;
	bool ok;

	if (!today(&day)) {
		return false;
	}

	k_mutex_lock(&lock, K_FOREVER);
	ok = history_change(&record, day, metric_index(bridge, metric), days_back, current, change);
	k_mutex_unlock(&lock);

	return ok;
}
