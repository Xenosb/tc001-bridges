/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "config.h"
#include "gfx.h"

LOG_MODULE_REGISTER(config, LOG_LEVEL_INF);

/* Bump when the layout of struct config changes; older saved data is then ignored */
#define CONFIG_VERSION 4

static struct config config = {
	.version = CONFIG_VERSION,
	.apps = BIT(APP_COUNT) - 1,
	.brightness = CONFIG_TC001_BRIGHTNESS,
	.auto_brightness = 0,
	.screensaver = SAVER_GREEN,
	.time_format = TIME_24H,
	.date_format = DATE_DAY_MONTH,
	.brightness_bias = 0,
};

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	struct config saved;

	if (strcmp(name, "cfg") != 0 || len != sizeof(saved)) {
		return 0;
	}
	if (read_cb(cb_arg, &saved, sizeof(saved)) != sizeof(saved) ||
	    saved.version != CONFIG_VERSION || saved.brightness < 1 || saved.brightness > 100 ||
	    saved.screensaver >= SAVER_MODE_COUNT || saved.time_format >= TIME_FORMAT_COUNT ||
	    saved.date_format >= DATE_FORMAT_COUNT || saved.brightness_bias < -50 ||
	    saved.brightness_bias > 50) {
		LOG_WRN("Ignoring saved settings that do not match this firmware");
		return 0;
	}

	config = saved;
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(tc001, "tc001", NULL, settings_set, NULL, NULL);

int config_init(void)
{
	int ret = settings_subsys_init();

	if (ret) {
		LOG_ERR("Settings init failed: %d", ret);
		return ret;
	}
	ret = settings_load_subtree("tc001");
	if (ret) {
		LOG_ERR("Loading settings failed: %d", ret);
	}

	gfx_set_brightness(config.brightness);
	LOG_INF("Settings: apps 0x%02x, brightness %u%%", config.apps, config.brightness);

	return ret;
}

struct config *config_get(void)
{
	return &config;
}

void config_save(void)
{
	int ret = settings_save_one("tc001/cfg", &config, sizeof(config));

	if (ret) {
		LOG_ERR("Saving settings failed: %d", ret);
	}
}
