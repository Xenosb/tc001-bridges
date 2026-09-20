/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_BRIGHTNESS_H_
#define TC001_BRIGHTNESS_H_

#include <stdbool.h>

#include "config.h"

#define BRIGHTNESS_MIN 5
#define BRIGHTNESS_MAX 100

/** What the light sensor says the brightness should be for @p light_raw, before any bias. */
int brightness_from_light(int light_raw);

/**
 * The brightness the settings ask for: the manual level, or with automatic brightness what the
 * light sensor reading @p light_raw calls for, moved by the bias.
 */
int brightness_target(const struct config *cfg, int light_raw);

/** Start at the brightness the settings ask for. Call once, after config_init(). */
void brightness_init(void);

/**
 * Move the LED brightness towards what the settings ask for, gently. Call about twice a second.
 *
 * @return true if the brightness changed, so what is on display should be sent again
 */
bool brightness_tick(void);

#endif /* TC001_BRIGHTNESS_H_ */
