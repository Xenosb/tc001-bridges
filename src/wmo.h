/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_WMO_H_
#define TC001_WMO_H_

#include <stdbool.h>

/* The weather icons that exist */
enum wx_kind {
	WX_SUN,
	WX_MOON,
	WX_PARTLY,
	WX_CLOUD,
	WX_FOG,
	WX_RAIN,
	WX_SNOW,
	WX_STORM,
};

/**
 * Pick an icon for a WMO weather interpretation code (as returned by Open-Meteo).
 * Clear skies are a sun by day and a moon by night.
 */
enum wx_kind wmo_kind(int code, bool is_day);

#endif /* TC001_WMO_H_ */
