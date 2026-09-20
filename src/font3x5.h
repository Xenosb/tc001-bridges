/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_FONT3X5_H_
#define TC001_FONT3X5_H_

#include <stdint.h>

#define FONT3X5_W       3
#define FONT3X5_H       5
/* Distance from one character to the next: 3 pixels plus 1 of spacing */
#define FONT3X5_ADVANCE 4
#define FONT3X5_FIRST   0x20
#define FONT3X5_GLYPHS  95 /* 0x20 (space) to 0x7e (~) */

/** Glyph bitmaps, see tools/gen_font.py. Bit (row * 3 + col) is a lit pixel. */
extern const uint16_t font3x5[FONT3X5_GLYPHS];

#endif /* TC001_FONT3X5_H_ */
