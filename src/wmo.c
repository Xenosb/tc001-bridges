/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "wmo.h"

enum wx_kind wmo_kind(int code, bool is_day)
{
	switch (code) {
	case 0: /* clear */
	case 1: /* mainly clear */
		return is_day ? WX_SUN : WX_MOON;
	case 2: /* partly cloudy; there is no night version with a moon, a plain cloud does */
		return is_day ? WX_PARTLY : WX_CLOUD;
	case 45: /* fog */
	case 48: /* rime fog */
		return WX_FOG;
	case 51 ... 57: /* drizzle, freezing drizzle */
	case 61 ... 67: /* rain, freezing rain */
	case 80 ... 82: /* rain showers */
		return WX_RAIN;
	case 71 ... 77: /* snow, snow grains */
	case 85 ... 86: /* snow showers */
		return WX_SNOW;
	case 95 ... 99: /* thunderstorm, with or without hail */
		return WX_STORM;
	case 3: /* overcast */
	default:
		return WX_CLOUD;
	}
}
