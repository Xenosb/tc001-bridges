/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_UI_H_
#define TC001_UI_H_

#include <stdbool.h>

#include "stats.h"

/** Start the display thread. Must be called once before any other ui_* call. */
int ui_init(void);

/**
 * Show a short status such as "SCAN" or "ERR" (up to 8 characters).
 * Ignored once the apps are running, so a failed refresh never covers what is shown.
 */
void ui_set_status(const char *text);

/**
 * Scroll @p text across the display until something else is shown. Meant for
 * text longer than eight characters, such as an IP address.
 */
void ui_show_text(const char *text);

/**
 * End ui_show_text() and go back to what was on display before: the app it interrupted, if any.
 * Does nothing unless a text is showing.
 */
void ui_hide_text(void);

/** Hand over from the status word to the apps, starting with the first enabled one. */
void ui_start_apps(void);

/** New bridge numbers. Safe to call before ui_start_apps(). */
void ui_set_items(const struct stat_item *items, size_t count);

/**
 * Note that a button was pressed, which keeps the display on for a while longer on battery.
 *
 * @return true if the display was off and this press only turned it on, so the caller should not act
 *         on it as well
 */
bool ui_activity(void);

/** Switch to the next (+1) or previous (-1) enabled app. */
void ui_step(int direction);

/** The middle button was pressed; what it does depends on the app on display. */
void ui_middle(void);

#endif /* TC001_UI_H_ */
