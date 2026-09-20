/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Screen saver: the Qt badge, either steady in green, steady in white, or with its colour drifting
 * around the colour wheel. The middle button changes the mode, and the choice is remembered.
 */

#include "app.h"
#include "color.h"
#include "config.h"
#include "qt_logo.h"

/*
 * The Qt green is #41cd52, but LEDs are driven with linear PWM, so its small red and blue parts look
 * far stronger than their numbers say and wash the green out towards yellow-white. These are red
 * and blue corrected for the eye (gamma 2.2), with the green kept as it was.
 */
#define QT_GREEN       0x11cd1b
#define WHITE          0xffffff
#define COLOR_TICK_MS  90
#define HUE_STEP       2
#define LEFT           ((GFX_W - QT_LOGO_W) / 2)

static bool redraw;
static uint8_t hue;
static int64_t next_change;

static void draw_logo(struct gfx_fb *fb, int left, uint32_t color)
{
	gfx_mask(fb, left, 0, qt_logo, QT_LOGO_H, color);
}

static void enter(int64_t now)
{
	redraw = true;
	next_change = now + COLOR_TICK_MS;
}

static enum app_result update(struct gfx_fb *fb, int64_t now)
{
	switch (config_get()->screensaver) {
	case SAVER_COLORS:
		if (!redraw && now < next_change) {
			return APP_IDLE;
		}
		if (!redraw) {
			hue += HUE_STEP;
			next_change = now + COLOR_TICK_MS;
		}
		draw_logo(fb, LEFT, color_hue(hue));
		break;

	case SAVER_WHITE:
	case SAVER_GREEN:
	default:
		if (!redraw) {
			return APP_IDLE;
		}
		draw_logo(fb, LEFT, config_get()->screensaver == SAVER_WHITE ? WHITE : QT_GREEN);
		break;
	}

	redraw = false;
	return APP_REDRAW;
}

static void middle(int64_t now)
{
	struct config *cfg = config_get();

	cfg->screensaver = (cfg->screensaver + 1) % SAVER_MODE_COUNT;
	config_save();
	enter(now);
}

const struct app app_screensaver = {
	.title = "LOGO",
	.enter = enter,
	.update = update,
	.middle = middle,
};
