/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_COLOR_H_
#define TC001_COLOR_H_

#include <stdint.h>

/**
 * Fully saturated, full value colour for a position on the colour wheel.
 * @p hue runs 0 to 255 and wraps: red, yellow, green, cyan, blue, magenta.
 * @return 0xRRGGBB
 */
uint32_t color_hue(uint8_t hue);

#endif /* TC001_COLOR_H_ */
