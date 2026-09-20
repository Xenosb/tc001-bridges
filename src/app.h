/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_APP_H_
#define TC001_APP_H_

#include <stdbool.h>
#include <stdint.h>

#include "gfx.h"
#include "stats.h"

/*
 * A screen of the clock. The display thread calls update() every tick, with the UI lock held,
 * and enter() whenever the user switches to the app, so an app needs no locking of its own for
 * state that only these callbacks touch.
 */

enum app_result {
	APP_IDLE,     /* nothing changed, keep showing the last frame */
	APP_REDRAW,   /* the frame in @p fb replaces what is shown, immediately */
	APP_SLIDE_UP, /* the frame in @p fb slides up into view, as one page replaces another */
};

struct app {
	/**
	 * A word shown once when the user switches to the app, before its pages start. NULL for none.
	 * Upper case, at most eight characters.
	 */
	const char *title;

	/** Called when the app becomes the visible one. The next update() must return a frame. */
	void (*enter)(int64_t now);

	/** Called every tick. Draw into @p fb (already cleared) and say what to do with it. */
	enum app_result (*update)(struct gfx_fb *fb, int64_t now);

	/** The middle button was pressed. May be NULL. */
	void (*middle)(int64_t now);

	/**
	 * Left (-1) or right (+1) was pressed. Return true if the app used it, for example to move
	 * through a menu, and false to let it switch to the next or previous app. May be NULL.
	 */
	bool (*nav)(int direction, int64_t now);
};

/** New bridge numbers for the "bridges total" app. Call with the UI lock held (see ui_set_items). */
void app_bridges_set_items(const struct stat_item *items, size_t count);

/** New bridge numbers for the "bridges trends" app. Call with the UI lock held. */
void app_trends_set_items(const struct stat_item *items, size_t count);

extern const struct app app_clock;
extern const struct app app_battery;
extern const struct app app_settings;
extern const struct app app_bridges_trends;
extern const struct app app_bridges_total;
extern const struct app app_screensaver;

#endif /* TC001_APP_H_ */
