/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>
#include <zephyr/spinlock.h>

#include "clock.h"

LOG_MODULE_REGISTER(clock, LOG_LEVEL_INF);

#define NTP_SERVER     "pool.ntp.org"
#define NTP_TIMEOUT_MS 4000

/* An RTC that lost its battery power comes up at some default date: only trust a recent one */
#define RTC_MIN_YEAR 2025
#define RTC_MAX_YEAR 2099

static const struct device *const rtc = DEVICE_DT_GET_OR_NULL(DT_ALIAS(rtc));

/* The time is kept as one known Unix time and the uptime at which it was known */
static struct k_spinlock lock;
static bool is_set;
static int64_t known_unix;
static int64_t known_uptime_ms;
static int32_t utc_offset;

static void set_unix(int64_t unix)
{
	k_spinlock_key_t key = k_spin_lock(&lock);

	known_unix = unix;
	known_uptime_ms = k_uptime_get();
	is_set = true;
	k_spin_unlock(&lock, key);
}

static int64_t now_unix(void)
{
	return known_unix + (k_uptime_get() - known_uptime_ms) / 1000;
}

static void rtc_write(int64_t unix)
{
	struct civil_time c;
	struct rtc_time t = {0};

	if (rtc == NULL || !device_is_ready(rtc)) {
		return;
	}

	timeconv_from_unix(unix, &c);
	t.tm_year = c.year - 1900;
	t.tm_mon = c.month - 1;
	t.tm_mday = c.day;
	t.tm_hour = c.hour;
	t.tm_min = c.minute;
	t.tm_sec = c.second;
	t.tm_wday = -1;
	t.tm_yday = -1;
	t.tm_isdst = -1;

	if (rtc_set_time(rtc, &t)) {
		LOG_WRN("Could not set the RTC");
	}
}

void clock_init(void)
{
	struct rtc_time t;
	struct civil_time c;

	if (rtc == NULL || !device_is_ready(rtc)) {
		LOG_WRN("No RTC available");
		return;
	}
	if (rtc_get_time(rtc, &t)) {
		LOG_WRN("Could not read the RTC");
		return;
	}

	c = (struct civil_time){
		.year = t.tm_year + 1900,
		.month = t.tm_mon + 1,
		.day = t.tm_mday,
		.hour = t.tm_hour,
		.minute = t.tm_min,
		.second = t.tm_sec,
	};
	if (c.year < RTC_MIN_YEAR || c.year > RTC_MAX_YEAR || c.month < 1 || c.month > 12 ||
	    c.day < 1 || c.day > 31) {
		LOG_WRN("RTC holds %04d-%02d-%02d, ignoring it", c.year, c.month, c.day);
		return;
	}

	set_unix(timeconv_to_unix(&c));
	LOG_INF("Time from RTC: %04d-%02d-%02d %02d:%02d UTC", c.year, c.month, c.day, c.hour,
		c.minute);
}

int clock_sync(void)
{
	struct sntp_time ts;
	int ret = sntp_simple(NTP_SERVER, NTP_TIMEOUT_MS, &ts);

	if (ret) {
		LOG_WRN("NTP request failed: %d", ret);
		return ret;
	}

	set_unix(ts.seconds);
	rtc_write(ts.seconds);
	LOG_INF("Time from NTP: %lld", (long long)ts.seconds);

	return 0;
}

bool clock_is_set(void)
{
	return is_set;
}

void clock_set_utc_offset(int32_t seconds)
{
	k_spinlock_key_t key = k_spin_lock(&lock);

	utc_offset = seconds;
	k_spin_unlock(&lock, key);
}

bool clock_local(struct civil_time *t)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	bool set = is_set;
	int64_t local = set ? now_unix() + utc_offset : 0;

	k_spin_unlock(&lock, key);

	if (set) {
		timeconv_from_unix(local, t);
	}
	return set;
}
