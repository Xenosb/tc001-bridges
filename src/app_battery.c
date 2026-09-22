/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Battery level: a battery symbol filled to the charge, and the percentage beside it. While the
 * battery seems to be charging, the rightmost column is a yellow bar with a dark block travelling
 * up it, round and round.
 *
 * The clock has no signal that says it is charging, so this is inferred from the voltage: a battery
 * on the charger sits at or near full voltage, or climbs. It can be wrong for a while, for example
 * just after unplugging a full battery.
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>

#include "app.h"
#include "power.h"

#define COLOR_OUTLINE 0xb0b0b0
#define COLOR_GOOD    0x64dc8c
#define COLOR_LOW     0xffc83c
#define COLOR_EMPTY   0xff5050
#define COLOR_TEXT    0xffffff
#define COLOR_BAR     0xffd23c

/* The battery is centered in the display height, with a 1 px gap top and bottom: a 12 pixel body
 * and a 2 pixel terminal
 */
#define BATTERY_W        16
#define BATTERY_H        6
#define BATTERY_Y        1 /* to center it: GFX_H is 8 */
#define INTERIOR_W       12 /* columns 1 to 12 */
#define TEXT_LEFT        BATTERY_W
#define TEXT_AREA        (GFX_W - 1 - TEXT_LEFT) /* up to the column before the charging bar */

/* The charging bar: the last column, with a dark block moving up it */
#define BAR_X        (GFX_W - 1)
#define BAR_BLOCK    3
#define BAR_STEP_MS  120

static const char *const battery_shape[BATTERY_H] = {
	"XXXXXXXXXXXXXX..",
	"X............X..",
	"X............XXX",
	"X............XXX",
	"X............X..",
	"XXXXXXXXXXXXXX..",
};

static bool redraw;
static int drawn_bar_step = -1;
static int drawn_percent = -1;
static bool drawn_charging;
static bool drawn_have;

/* Which position of the charging animation it is at @p now */
static int bar_step(int64_t now)
{
	return (now / BAR_STEP_MS) % GFX_H;
}

static void draw(struct gfx_fb *fb, int64_t now)
{
	int percent = power_percent();
	bool charging = power_external();
	char text[8];
	int filled = (percent * INTERIOR_W + 50) / 100;
	uint32_t color = percent > 50 ? COLOR_GOOD : (percent > 20 ? COLOR_LOW : COLOR_EMPTY);
	int step = bar_step(now);

	if (percent > 0 && filled == 0) {
		filled = 1; /* almost empty is not the same as empty */
	}

	gfx_mask(fb, 0, BATTERY_Y, battery_shape, BATTERY_H, COLOR_OUTLINE);
	for (int col = 0; col < filled; col++) {
		for (int row = 1; row < BATTERY_H - 1; row++) {
			gfx_pixel(fb, 1 + col, BATTERY_Y + row, color);
		}
	}

	snprintf(text, sizeof(text), "%d", percent);
	gfx_text(fb, TEXT_LEFT + (TEXT_AREA - gfx_text_width(text)) / 2, 1, text, COLOR_TEXT);

	if (charging) {
		/* The dark block's top row moves up by one each step and wraps round the column */
		int top = (GFX_H - 1) - step;

		for (int row = 0; row < GFX_H; row++) {
			if ((row - top + GFX_H) % GFX_H >= BAR_BLOCK) {
				gfx_pixel(fb, BAR_X, row, COLOR_BAR);
			}
		}
	}
	drawn_bar_step = step;
	drawn_percent = percent;
	drawn_charging = charging;
}

static void enter(int64_t now)
{
	ARG_UNUSED(now);
	redraw = true;
}

static enum app_result update(struct gfx_fb *fb, int64_t now)
{
	bool have = power_have_reading();

	/* Redraw when what is shown is out of date: a new reading, or the charging bar moved on */
	if (have != drawn_have || (have && (power_percent() != drawn_percent ||
					   power_external() != drawn_charging)) ||
	    (have && power_external() && bar_step(now) != drawn_bar_step)) {
		redraw = true;
	}

	if (!redraw) {
		return APP_IDLE;
	}
	redraw = false;
	drawn_have = have;

	if (!have) {
		gfx_text(fb, (GFX_W - gfx_text_width("N/A")) / 2, 1, "N/A", 0x505050);
	} else {
		draw(fb, now);
	}
	return APP_REDRAW;
}

const struct app app_battery = {
	.title = "BATTERY",
	.enter = enter,
	.update = update,
};
