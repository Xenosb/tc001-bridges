/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "battery.h"
#include "brightness.h"
#include "clock.h"
#include "config.h"
#include "history.h"
#include "https.h"
#include "light.h"
#include "location.h"
#include "portal.h"
#include "setup.h"
#include "stats.h"
#include "ui.h"
#include "weather.h"
#include "wifi.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define RETRY_DELAY_S 30

#define SECOND_MS 1000

/* Everything the clock fetches over the network. One request at a time: TLS memory allows no more. */
struct task {
	const char *name;
	int (*run)(void);
	int interval_s; /* until the next run after a success */
	int retry_s;    /* until the next run after a failure */
	int64_t next_ms;
};

static int run_time(void)
{
	return clock_sync();
}

static int run_location(void)
{
	return location_refresh();
}

static int run_weather(void)
{
	return weather_refresh();
}

static int run_stats(void)
{
	struct stat_item items[STATS_MAX_ITEMS];
	size_t count;
	int ret = stats_refresh(items, &count);

	if (ret == 0) {
		history_record_items(items, count);
		ui_set_items(items, count);
	}
	return ret;
}

/* In the order they run at start: the time first, and the weather needs the location */
static struct task tasks[] = {
	{"time", run_time, 60 * 60, 60},
	{"location", run_location, 12 * 60 * 60, 5 * 60},
	{"weather", run_weather, 30 * 60, 5 * 60},
	{"stats", run_stats, CONFIG_TC001_STATS_REFRESH_S, RETRY_DELAY_S},
};

/* Run whatever is due, then sleep until the next thing is */
static void run_tasks_forever(void)
{
	while (1) {
		int64_t soonest = INT64_MAX;

		for (size_t i = 0; i < ARRAY_SIZE(tasks); i++) {
			struct task *t = &tasks[i];

			if (k_uptime_get() >= t->next_ms) {
				int ret = t->run();

				if (ret) {
					LOG_WRN("Task %s failed (%d), trying again in %d s", t->name, ret,
						t->retry_s);
				}
				t->next_ms = k_uptime_get() +
					     (int64_t)(ret ? t->retry_s : t->interval_s) * SECOND_MS;
			}
			soonest = MIN(soonest, t->next_ms);
		}

		k_sleep(K_MSEC(CLAMP(soonest - k_uptime_get(), 100, 60 * SECOND_MS)));
	}
}

/*
 * Left/right switch between apps, the middle button does whatever the current app defines. While
 * the setup access point is up it ends the setup instead.
 */
static void button_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type != INPUT_EV_KEY || !evt->value) {
		return;
	}
	if (ui_activity()) {
		return; /* the display was off: this press only turned it on */
	}

	switch (evt->code) {
	case INPUT_KEY_LEFT:
		ui_step(-1);
		break;
	case INPUT_KEY_RIGHT:
		ui_step(1);
		break;
	case INPUT_KEY_ENTER:
		if (setup_active()) {
			setup_cancel();
		} else {
			ui_middle();
		}
		break;
	default:
		break;
	}
}
INPUT_CALLBACK_DEFINE(NULL, button_cb, NULL);

int main(void)
{
	int ret;

	config_init();
	location_init();
	clock_init();
	history_init();
	battery_init();
	light_init();
	brightness_init();

	ret = ui_init();
	if (ret) {
		LOG_ERR("UI init failed: %d", ret);
		return ret;
	}

	ui_set_status("SCAN");
	ret = wifi_connect_saved();
	if (ret) {
		LOG_WRN("No Wi-Fi connection (%d), starting the setup access point", ret);
		setup_boot();
		return 0;
	}

	ui_start_apps();

	ret = https_init();
	if (ret) {
		LOG_ERR("HTTPS init failed: %d", ret);
		ui_set_status("FAIL");
		return ret;
	}

	/* The settings page: browse to the clock's address, which the device menu will show */
	portal_start();

	run_tasks_forever();

	return 0;
}
