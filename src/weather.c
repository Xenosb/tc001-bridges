/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "format.h"
#include "https.h"
#include "location.h"
#include "weather.h"

LOG_MODULE_REGISTER(weather, LOG_LEVEL_INF);

static K_MUTEX_DEFINE(lock);
static struct weather latest;
static bool have_weather;

int weather_refresh(void)
{
	struct location here;
	char lat[16], lon[16], path[160];
	struct resp_field fields[] = {
		{.key = "utc_offset_seconds"},
		{.key = "temperature_2m", .decimals = 1},
		{.key = "weather_code"},
		{.key = "is_day"},
	};
	int ret;

	if (!location_get(&here)) {
		return -EAGAIN;
	}

	format_coordinate(lat, sizeof(lat), here.latitude);
	format_coordinate(lon, sizeof(lon), here.longitude);
	snprintf(path, sizeof(path),
		 "/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,weather_code,is_day"
		 "&timezone=auto",
		 lat, lon);

	ret = https_get("api.open-meteo.com", HTTPS_ROOT_ISRG_X1, path, fields, ARRAY_SIZE(fields));
	if (ret) {
		LOG_WRN("Weather request failed: %d", ret);
		return ret;
	}

	location_set_utc_offset(fields[0].value);

	k_mutex_lock(&lock, K_FOREVER);
	latest = (struct weather){
		.temperature = fields[1].value,
		.code = fields[2].value,
		.is_day = fields[3].value != 0,
	};
	have_weather = true;
	k_mutex_unlock(&lock);

	LOG_INF("Weather: %d.%d C, code %d, %s, UTC%+d s", (int)(fields[1].value / 10),
		(int)(abs(fields[1].value) % 10), (int)fields[2].value,
		fields[3].value ? "day" : "night", (int)fields[0].value);

	return 0;
}

bool weather_get(struct weather *out)
{
	bool have;

	k_mutex_lock(&lock, K_FOREVER);
	*out = latest;
	have = have_weather;
	k_mutex_unlock(&lock);

	return have;
}
