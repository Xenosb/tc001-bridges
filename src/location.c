/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "clock.h"
#include "https.h"
#include "location.h"

LOG_MODULE_REGISTER(location, LOG_LEVEL_INF);

/* Bump when struct saved changes; older saved data is then ignored */
#define SAVED_VERSION 1

/* Roughly 11 metres: moves smaller than this are not worth a flash write */
#define MOVE_THRESHOLD 1

struct saved {
	uint8_t version;
	uint8_t valid;
	int32_t latitude;
	int32_t longitude;
	int32_t utc_offset;
};

static K_MUTEX_DEFINE(lock);
static struct location here;

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	struct saved saved;

	if (strcmp(name, "v") != 0 || len != sizeof(saved) ||
	    read_cb(cb_arg, &saved, sizeof(saved)) != sizeof(saved) || saved.version != SAVED_VERSION) {
		return 0;
	}

	here = (struct location){
		.valid = saved.valid,
		.latitude = saved.latitude,
		.longitude = saved.longitude,
		.utc_offset = saved.utc_offset,
	};
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(tc001_loc, "tc001_loc", NULL, settings_set, NULL, NULL);

/* Caller holds the lock */
static void save(void)
{
	struct saved saved = {
		.version = SAVED_VERSION,
		.valid = here.valid,
		.latitude = here.latitude,
		.longitude = here.longitude,
		.utc_offset = here.utc_offset,
	};
	int ret = settings_save_one("tc001_loc/v", &saved, sizeof(saved));

	if (ret) {
		LOG_ERR("Saving the location failed: %d", ret);
	}
}

void location_init(void)
{
	int ret = settings_subsys_init();

	if (ret == 0) {
		ret = settings_load_subtree("tc001_loc");
	}
	if (ret) {
		LOG_WRN("Loading the saved location failed: %d", ret);
		return;
	}

	if (here.valid) {
		clock_set_utc_offset(here.utc_offset);
		LOG_INF("Saved location, UTC offset %d s", (int)here.utc_offset);
	}
}

int location_refresh(void)
{
	struct resp_field fields[] = {
		{.key = "latitude", .decimals = 4},
		{.key = "longitude", .decimals = 4},
	};
	int ret = https_get("ipwho.is", HTTPS_ROOT_GTS_R4, "/", fields, ARRAY_SIZE(fields));

	if (ret) {
		LOG_WRN("Location lookup failed: %d", ret);
		return ret;
	}

	k_mutex_lock(&lock, K_FOREVER);
	if (!here.valid || labs(fields[0].value - here.latitude) > MOVE_THRESHOLD ||
	    labs(fields[1].value - here.longitude) > MOVE_THRESHOLD) {
		here.valid = true;
		here.latitude = fields[0].value;
		here.longitude = fields[1].value;
		save();
		LOG_INF("Location: %d, %d (ten-thousandths of a degree)", (int)here.latitude,
			(int)here.longitude);
	}
	k_mutex_unlock(&lock);

	return 0;
}

bool location_get(struct location *out)
{
	bool valid;

	k_mutex_lock(&lock, K_FOREVER);
	*out = here;
	valid = here.valid;
	k_mutex_unlock(&lock);

	return valid;
}

void location_set_utc_offset(int32_t seconds)
{
	clock_set_utc_offset(seconds);

	k_mutex_lock(&lock, K_FOREVER);
	if (here.utc_offset != seconds) {
		here.utc_offset = seconds;
		if (here.valid) {
			save();
		}
	}
	k_mutex_unlock(&lock);
}
