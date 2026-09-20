/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_CONFIG_H_
#define TC001_CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

/* Every screen the clock can show. The order is the order they rotate in. */
enum app_id {
	APP_CLOCK,
	APP_BRIDGES_TOTAL,
	APP_BRIDGES_TRENDS,
	APP_SCREENSAVER,
	APP_BATTERY,
	APP_SETTINGS,
	APP_COUNT,
};

enum time_format {
	TIME_24H,
	TIME_12H,
	TIME_FORMAT_COUNT,
};

enum date_format {
	DATE_DAY_MONTH, /* 20.09. */
	DATE_MONTH_DAY, /* 09/20 */
	DATE_FORMAT_COUNT,
};

enum screensaver_mode {
	SAVER_GREEN,
	SAVER_WHITE,
	SAVER_COLORS,
	SAVER_MODE_COUNT,
};

/* User settings, kept in flash. Change fields, then call config_save(). */
struct config {
	uint8_t version;
	uint8_t apps;         /* bit n set: enum app_id n is in the rotation */
	uint8_t brightness;   /* percent, 1 to 100 */
	uint8_t auto_brightness;
	uint8_t screensaver;  /* enum screensaver_mode */
	uint8_t time_format;  /* enum time_format */
	uint8_t date_format;  /* enum date_format */
	int8_t brightness_bias; /* percentage points added to the automatic brightness, -50 to 50 */
};

/** Load the saved settings, falling back to defaults. Call once, early. */
int config_init(void);

/** The live settings. */
struct config *config_get(void);

/** Write the settings to flash. */
void config_save(void);

/* Apps that cannot be switched off, because the web settings do not offer them */
#define APPS_ALWAYS_ON ((1U << APP_BATTERY) | (1U << APP_SETTINGS))

static inline bool config_app_enabled(const struct config *c, enum app_id id)
{
	return (c->apps | APPS_ALWAYS_ON) & (1U << id);
}

#endif /* TC001_CONFIG_H_ */
