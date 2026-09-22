/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Settings menu. In the rotation this is a "SETTINGS" card; the middle button opens the menu, where
 * left and right move between the items and the middle button chooses one:
 *
 *   BRIGHTNESS   AUTO on/off, the level (or with AUTO on, a bias from the automatic level),
 *                SET DEFAULT (freezes the current auto level as the manual one), BACK
 *   NETWORK      the network, the address, NEW NET (sets up a network), BACK
 *   EXIT
 *
 * The menu closes by itself after a minute of no button presses. Which apps are shown and the date
 * and time format are set from a browser instead: see portal.c.
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>

#include "app.h"
#include "brightness.h"
#include "config.h"
#include "light.h"
#include "scroll.h"
#include "setup.h"
#include "wifi.h"

#define IDLE_CLOSE_MS 60000
#define LEVEL_STEP    5

#define COLOR_CARD   0xc8c8c8
#define COLOR_ITEM   0x80c8ff
#define COLOR_VALUE  0xffffff
#define COLOR_BACK   0xa0a0a0
#define COLOR_ACTION 0xffc83c

enum menu {
	MENU_CARD, /* not in the menu: the card shown in the rotation */
	MENU_ROOT,
	MENU_BRIGHTNESS,
	MENU_LEVEL, /* adjusting the level: left and right change it */
	MENU_NETWORK,
};

static enum menu menu;
static int index_in_menu;
static int64_t item_start; /* when the item on display appeared, for scrolling */
static int64_t last_press;
static bool redraw;
static int drawn_x;
static bool level_changed;

static int item_count(void)
{
	switch (menu) {
	case MENU_ROOT:
		return 3;
	case MENU_BRIGHTNESS:
		return 4;
	case MENU_NETWORK:
		return 4;
	default:
		return 1;
	}
}

/* The text of the item on display, and the colour to draw it in */
static uint32_t item_text(char *buf, size_t len)
{
	const struct config *cfg = config_get();

	switch (menu) {
	case MENU_ROOT:
		switch (index_in_menu) {
		case 0:
			snprintf(buf, len, "BRIGHTNESS");
			return COLOR_ITEM;
		case 1:
			snprintf(buf, len, "NETWORK");
			return COLOR_ITEM;
		default:
			snprintf(buf, len, "EXIT");
			return COLOR_BACK;
		}

	case MENU_BRIGHTNESS:
		switch (index_in_menu) {
		case 0:
			snprintf(buf, len, cfg->auto_brightness ? "AUTO ON" : "AUTO OFF");
			return COLOR_ITEM;
		case 1:
			if (cfg->auto_brightness) {
				snprintf(buf, len, "BIAS %+d", cfg->brightness_bias);
			} else {
				snprintf(buf, len, "LEVEL %d", cfg->brightness);
			}
			return COLOR_VALUE;
		case 2:
			snprintf(buf, len, "SET DEFAULT");
			return COLOR_ACTION;
		default:
			snprintf(buf, len, "BACK");
			return COLOR_BACK;
		}

	case MENU_LEVEL:
		if (cfg->auto_brightness) {
			snprintf(buf, len, "- %+d +", cfg->brightness_bias);
		} else {
			snprintf(buf, len, "- %d%% +", cfg->brightness);
		}
		return COLOR_VALUE;

	case MENU_NETWORK:
		switch (index_in_menu) {
		case 0:
			snprintf(buf, len, "SSID %s", wifi_ssid()[0] ? wifi_ssid() : "-");
			return COLOR_VALUE;
		case 1:
			snprintf(buf, len, "IP %s", wifi_address()[0] ? wifi_address() : "-");
			return COLOR_VALUE;
		case 2:
			snprintf(buf, len, "NEW NET");
			return COLOR_ACTION;
		default:
			snprintf(buf, len, "BACK");
			return COLOR_BACK;
		}

	default:
		snprintf(buf, len, "SETTINGS");
		return COLOR_CARD;
	}
}

static void go(enum menu to, int index, int64_t now)
{
	menu = to;
	index_in_menu = index;
	item_start = now;
	redraw = true;
}

static void leave_level(void)
{
	if (level_changed) {
		config_save();
		level_changed = false;
	}
}

/* Read the current ambient light and make what it calls for the manual default from now on */
static void set_current_as_default(struct config *cfg)
{
	int raw = light_read();

	if (raw >= 0) {
		int target = brightness_from_light(raw) + cfg->brightness_bias;

		cfg->brightness = CLAMP(target, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
		cfg->auto_brightness = false;
		config_save();
	}
	redraw = true;
}

static void enter(int64_t now)
{
	/* Coming back after the setup display: the menu is where it was left */
	if (menu == MENU_CARD) {
		go(MENU_CARD, 0, now);
	} else {
		redraw = true;
		item_start = now;
	}
	last_press = now;
}

static enum app_result update(struct gfx_fb *fb, int64_t now)
{
	char text[48];
	uint32_t color;
	int x;

	if (menu != MENU_CARD && now - last_press > IDLE_CLOSE_MS) {
		leave_level();
		go(MENU_CARD, 0, now);
	}

	color = item_text(text, sizeof(text));
	x = scroll_x(gfx_text_width(text), GFX_W, now - item_start);

	if (!redraw && x == drawn_x) {
		return APP_IDLE;
	}
	redraw = false;
	drawn_x = x;

	gfx_text(fb, x, 1, text, color);
	return APP_REDRAW;
}

static void middle(int64_t now)
{
	struct config *cfg = config_get();

	last_press = now;

	switch (menu) {
	case MENU_CARD:
		go(MENU_ROOT, 0, now);
		break;

	case MENU_ROOT:
		if (index_in_menu == 0) {
			go(MENU_BRIGHTNESS, 0, now);
		} else if (index_in_menu == 1) {
			go(MENU_NETWORK, 0, now);
		} else {
			go(MENU_CARD, 0, now);
		}
		break;

	case MENU_BRIGHTNESS:
		if (index_in_menu == 0) {
			cfg->auto_brightness = !cfg->auto_brightness;
			config_save();
			redraw = true;
		} else if (index_in_menu == 1) {
			go(MENU_LEVEL, 0, now);
		} else if (index_in_menu == 2) {
			set_current_as_default(cfg);
		} else {
			go(MENU_ROOT, 0, now);
		}
		break;

	case MENU_LEVEL:
		leave_level();
		go(MENU_BRIGHTNESS, 1, now);
		break;

	case MENU_NETWORK:
		if (index_in_menu == 2) {
			/* Open the setup access point next to the connection; the middle button ends it */
			setup_request();
		} else if (index_in_menu == 3) {
			go(MENU_ROOT, 1, now);
		}
		break;
	}
}

static bool nav(int direction, int64_t now)
{
	struct config *cfg = config_get();

	if (menu == MENU_CARD) {
		return false; /* switch to the next app */
	}
	last_press = now;

	if (menu == MENU_LEVEL) {
		if (cfg->auto_brightness) {
			int bias = cfg->brightness_bias + direction * LEVEL_STEP;

			cfg->brightness_bias = CLAMP(bias, -50, 50);
		} else {
			int level = cfg->brightness + direction * LEVEL_STEP;

			cfg->brightness = CLAMP(level, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
		}
		level_changed = true;
		redraw = true;
		return true;
	}

	go(menu, (index_in_menu + direction + item_count()) % item_count(), now);
	return true;
}

const struct app app_settings = {
	.enter = enter,
	.update = update,
	.middle = middle,
	.nav = nav,
};
