/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "color.h"

uint32_t color_hue(uint8_t hue)
{
	/* Six sectors of 42.67 steps; `rise` climbs 0..255 across one sector */
	unsigned int sector = hue * 6 / 256;
	unsigned int rise = (hue * 6 - sector * 256) & 0xff;
	unsigned int fall = 255 - rise;
	unsigned int r, g, b;

	switch (sector) {
	case 0:
		r = 255, g = rise, b = 0;
		break;
	case 1:
		r = fall, g = 255, b = 0;
		break;
	case 2:
		r = 0, g = 255, b = rise;
		break;
	case 3:
		r = 0, g = fall, b = 255;
		break;
	case 4:
		r = rise, g = 0, b = 255;
		break;
	default:
		r = 255, g = 0, b = fall;
		break;
	}

	return r << 16 | g << 8 | b;
}
