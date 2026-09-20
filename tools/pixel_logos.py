#!/usr/bin/env python3
# Copyright (c) 2026 Bruno Vunderl
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Pixel-art logos for the TC001's 8x32 LED matrix.

The art below is hand drawn, not converted: an automatic downsample of a logo to
8x8 pixels is an unreadable blur, so each one is redrawn on the 8x8 grid using the
shape and colours that make the original recognisable. The ASCII grids are the
source of truth. Running this script regenerates

  src/logos.c / src/logos.h      RGB888 arrays, ready for display_write()
  docs/images/pixel-logos.png    preview, drawn as round LEDs on black

Usage:  python3 tools/pixel_logos.py
The preview needs Pillow (pip install pillow); the C files do not.
"""

import os
import sys

SIZE = 8
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# '.' is an LED that is off. Colours are chosen for LEDs, which wash out dark
# shades: they are brighter and more saturated than the logos' print colours.
LOGOS = {
    # Ferris, the Rust mascot: orange crab, spiky shell, dark eyes, claws
    "rust": {
        "palette": {
            "O": (255, 80, 0),      # shell
            "D": (150, 35, 0),      # underside shadow
            "W": (255, 255, 255),   # eye highlight; the pupil below it is an LED that is off
        },
        "art": [
            ".O.OO.O.",
            ".OOOOOO.",
            "OOWOOWOO",
            "OO.OO.OO",
            "ODDDDDDO",
            "O.O..O.O",
            ".O....O.",
            "........",
        ],
    },
    # C#: pointy-top purple hexagon with a white C. The # is left out: a C and a # do not
    # both fit legibly in 8x8 pixels, and next to the Rust logo the C is unmistakable.
    "csharp": {
        "palette": {
            "V": (55, 18, 100),     # hexagon, kept dim so the C stands out
            "L": (45, 45, 105),     # slightly lighter top, like the logo's gradient
            "W": (255, 255, 255),
        },
        "art": [
            "...LL...",
            ".LLLLLL.",
            "LLWWWLLL",
            "VVWVVVVV",
            "VVWVVVVV",
            "VVWWWVVV",
            ".VVVVVV.",
            "...VV...",
        ],
    },
    # crates.io: isometric cube with a lit top and a lighter left face than right face
    "crate": {
        "palette": {
            "T": (255, 195, 120),   # top face
            "L": (205, 115, 50),    # left face
            "R": (115, 55, 20),     # right face
        },
        "art": [
            "...TT...",
            ".TTTTTT.",
            "LTTTTTTR",
            "LLLTTRRR",
            "LLLLRRRR",
            "LLLLRRRR",
            ".LLLRRR.",
            "...LR...",
        ],
    },
    # NuGet: blue blob with a hole and a small ball at the top left
    "nuget": {
        "palette": {
            "B": (0, 115, 255),
        },
        "art": [
            "BB......",
            "BB..BBB.",
            "...BBBBB",
            "..BBBBBB",
            "..BBB..B",
            "..BBB..B",
            "...BBBBB",
            "....BBB.",
        ],
    },
    # Weather icons, chosen by wmo_kind() in src/wmo.c
    "wx_sun": {
        "palette": {"Y": (255, 190, 0)},
        "art": [
            "...YY...",
            ".Y....Y.",
            "..YYYY..",
            "Y.YYYY.Y",
            "Y.YYYY.Y",
            "..YYYY..",
            ".Y....Y.",
            "...YY...",
        ],
    },
    "wx_moon": {
        "palette": {"M": (230, 230, 150)},
        "art": [
            "..MMM...",
            ".MMM....",
            "MMM.....",
            "MMM.....",
            "MMM.....",
            "MMM...M.",
            ".MMM.MM.",
            "..MMMM..",
        ],
    },
    "wx_partly": {
        "palette": {"Y": (255, 190, 0), "C": (150, 165, 190)},
        "art": [
            "..Y.....",
            ".YYY.CC.",
            "YYYYCCCC",
            ".YYCCCCC",
            "..CCCCCC",
            ".CCCCCCC",
            "..CCCCC.",
            "........",
        ],
    },
    "wx_cloud": {
        "palette": {"C": (150, 165, 190)},
        "art": [
            "........",
            "...CC...",
            "..CCCC..",
            ".CCCCCCC",
            "CCCCCCCC",
            "CCCCCCCC",
            ".CCCCCC.",
            "........",
        ],
    },
    "wx_fog": {
        "palette": {"F": (150, 160, 175)},
        "art": [
            "........",
            ".FFFFFF.",
            "........",
            "FFFFFF..",
            "........",
            "..FFFFFF",
            "........",
            ".FFFFFF.",
        ],
    },
    "wx_rain": {
        "palette": {"C": (150, 165, 190), "B": (0, 120, 255)},
        "art": [
            "...CC...",
            "..CCCC..",
            ".CCCCCCC",
            "CCCCCCCC",
            ".CCCCCC.",
            ".B.B.B..",
            "B.B.B.B.",
            ".B.B.B..",
        ],
    },
    "wx_snow": {
        "palette": {"C": (150, 165, 190), "W": (255, 255, 255)},
        "art": [
            "...CC...",
            "..CCCC..",
            ".CCCCCCC",
            "CCCCCCCC",
            ".CCCCCC.",
            "..W..W..",
            ".W..W..W",
            "..W..W..",
        ],
    },
    "wx_storm": {
        "palette": {"D": (90, 100, 125), "Y": (255, 200, 0)},
        "art": [
            "...DD...",
            "..DDDD..",
            ".DDDDDDD",
            "DDDDDDDD",
            ".DDDDDD.",
            "...YY...",
            "..YY....",
            "...Y....",
        ],
    },
}


def validate():
    for name, logo in LOGOS.items():
        art = logo["art"]
        assert len(art) == SIZE, f"{name}: needs {SIZE} rows, has {len(art)}"
        for y, row in enumerate(art):
            assert len(row) == SIZE, f"{name} row {y}: needs {SIZE} columns, has {len(row)}"
            for ch in row:
                assert ch == "." or ch in logo["palette"], f"{name} row {y}: unknown '{ch}'"


def pixels(logo):
    """Row-major list of (r, g, b) tuples."""
    return [logo["palette"].get(ch, (0, 0, 0)) for row in logo["art"] for ch in row]


def write_c():
    header = f"""/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Generated by tools/pixel_logos.py, do not edit. */

#ifndef TC001_LOGOS_H_
#define TC001_LOGOS_H_

#include <stdint.h>

#define LOGO_SIZE {SIZE}

/*
 * {SIZE}x{SIZE} pixel-art logos as RGB888, row by row, so they can be passed straight to
 * display_write() with a {SIZE}x{SIZE} buffer descriptor.
 */
"""
    for name in LOGOS:
        header += f"extern const uint8_t logo_{name}[LOGO_SIZE * LOGO_SIZE * 3];\n"
    header += "\n#endif /* TC001_LOGOS_H_ */\n"

    body = """/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Generated by tools/pixel_logos.py, do not edit. */

#include "logos.h"
"""
    for name, logo in LOGOS.items():
        body += f"\n/*\n"
        for row in logo["art"]:
            body += f" * {row}\n"
        body += " */\n"
        body += f"const uint8_t logo_{name}[LOGO_SIZE * LOGO_SIZE * 3] = {{\n"
        px = pixels(logo)
        for y in range(SIZE):
            cells = ", ".join(f"0x{r:02x}, 0x{g:02x}, 0x{b:02x}" for r, g, b in px[y * SIZE:(y + 1) * SIZE])
            body += f"\t{cells},\n"
        body += "};\n"

    with open(os.path.join(ROOT, "src", "logos.h"), "w") as f:
        f.write(header)
    with open(os.path.join(ROOT, "src", "logos.c"), "w") as f:
        f.write(body)


def write_preview(path=None):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("Pillow not installed, skipping the preview image")
        return
    cell, pad, per_row = 28, 20, 6
    n = len(LOGOS)
    rows = (n + per_row - 1) // per_row
    w = pad + per_row * (SIZE * cell + pad)
    h = pad + rows * (SIZE * cell + pad)
    img = Image.new("RGB", (w, h), (0, 0, 0))
    d = ImageDraw.Draw(img)
    for i, logo in enumerate(LOGOS.values()):
        ox = pad + (i % per_row) * (SIZE * cell + pad)
        oy = pad + (i // per_row) * (SIZE * cell + pad)
        for j, (r, g, b) in enumerate(pixels(logo)):
            x, y = ox + (j % SIZE) * cell, oy + (j // SIZE) * cell
            # unlit LEDs stay faintly visible so the grid reads as a matrix
            d.ellipse([x + 3, y + 3, x + cell - 4, y + cell - 4],
                      fill=(r, g, b) if (r or g or b) else (18, 18, 18))
    path = path or os.path.join(ROOT, "docs", "images", "pixel-logos.png")
    img.save(path)
    return path


if __name__ == "__main__":
    validate()
    write_c()
    print("wrote src/logos.c, src/logos.h")
    out = write_preview(sys.argv[1] if len(sys.argv) > 1 else None)
    if out:
        print("wrote", out)
