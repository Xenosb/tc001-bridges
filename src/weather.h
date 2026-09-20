/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_WEATHER_H_
#define TC001_WEATHER_H_

#include <stdbool.h>
#include <stdint.h>

/* The current conditions at the clock's location */
struct weather {
	int temperature; /* tenths of a degree Celsius */
	int code;        /* WMO weather interpretation code, see wmo.h */
	bool is_day;
};

/**
 * Fetch the current conditions from Open-Meteo, and the time zone offset that goes with the
 * location. Needs a location first: returns -EAGAIN without touching the network if there is none.
 *
 * @return 0 on success, negative errno on failure
 */
int weather_refresh(void);

/** Copy out the latest conditions; returns false if none have been fetched yet. */
bool weather_get(struct weather *out);

#endif /* TC001_WEATHER_H_ */
