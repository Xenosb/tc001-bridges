/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdbool.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "https.h"
#include "stats.h"

LOG_MODULE_REGISTER(stats, LOG_LEVEL_INF);

/* crates.io asks clients to make no more than one request per second */
#define CRATES_PACING_MS 1000

struct known_value {
	bool valid;
	uint32_t value;
};

/* Last good value of every metric, so a failed request leaves it untouched */
static struct known_value known[STAT_BRIDGE_COUNT][STAT_METRIC_COUNT];

static void remember(enum stat_bridge bridge, enum stat_metric metric, int64_t value)
{
	known[bridge][metric].valid = true;
	known[bridge][metric].value = (uint32_t)value;
}

static bool fetch_stars(enum stat_bridge bridge, const char *repo_path)
{
	struct resp_field stars = {.key = "stargazers_count"};
	int ret = https_get("api.github.com", HTTPS_ROOT_USERTRUST_ECC, repo_path, &stars, 1);

	if (ret) {
		LOG_WRN("Stars for bridge %d unavailable: %d", bridge, ret);
		return false;
	}
	remember(bridge, STAT_METRIC_STARS, stars.value);
	return true;
}

static bool fetch_nuget_downloads(const char *package, int64_t *downloads)
{
	char path[160];
	struct resp_field total = {.key = "totalDownloads"};
	int ret;

	snprintf(path, sizeof(path), "/query?q=packageid:%s&take=1&prerelease=true", package);
	ret = https_get("azuresearch-usnc.nuget.org", HTTPS_ROOT_DIGICERT_G2, path, &total, 1);
	if (ret) {
		LOG_WRN("NuGet downloads for %s unavailable: %d", package, ret);
		return false;
	}
	*downloads = total.value;
	return true;
}

int stats_refresh(struct stat_item *items, size_t *count)
{
	bool any = false;
	struct resp_field crate = {.key = "downloads"};
	int64_t win;
	int64_t linux_x64;

	any |= fetch_stars(STAT_BRIDGE_RUST, "/repos/qt/qtbridge-rust");
	any |= fetch_stars(STAT_BRIDGE_CSHARP, "/repos/qt/qtbridge-csharp");

	/* `include=` with no value leaves out the version list, keeping the answer small */
	if (https_get("crates.io", HTTPS_ROOT_GLOBALSIGN_R3, "/api/v1/crates/qtbridge?include=",
		      &crate, 1) == 0) {
		remember(STAT_BRIDGE_RUST, STAT_METRIC_DOWNLOADS, crate.value);
		any = true;
	} else {
		LOG_WRN("crates.io downloads unavailable");
	}
	k_msleep(CRATES_PACING_MS);

	/* The C# runtime ships as separate per-platform packages, so both are summed */
	if (fetch_nuget_downloads("QtGroup.Qt.Bridge.CSharp.win-x64", &win) &&
	    fetch_nuget_downloads("QtGroup.Qt.Bridge.CSharp.linux-x64", &linux_x64)) {
		remember(STAT_BRIDGE_CSHARP, STAT_METRIC_DOWNLOADS, win + linux_x64);
		any = true;
	}

	*count = 0;
	for (int b = 0; b < STAT_BRIDGE_COUNT; b++) {
		for (int m = 0; m < STAT_METRIC_COUNT; m++) {
			if (known[b][m].valid) {
				items[(*count)++] = (struct stat_item){
					.bridge = b,
					.metric = m,
					.value = known[b][m].value,
				};
			}
		}
	}

	if (any) {
		LOG_INF("Rust: %u stars, %u downloads | C#: %u stars, %u downloads",
			known[STAT_BRIDGE_RUST][STAT_METRIC_STARS].value,
			known[STAT_BRIDGE_RUST][STAT_METRIC_DOWNLOADS].value,
			known[STAT_BRIDGE_CSHARP][STAT_METRIC_STARS].value,
			known[STAT_BRIDGE_CSHARP][STAT_METRIC_DOWNLOADS].value);
	}

	return any ? 0 : -EIO;
}
