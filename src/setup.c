/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "portal.h"
#include "setup.h"
#include "ui.h"
#include "wifi.h"

LOG_MODULE_REGISTER(setup, LOG_LEVEL_INF);

#define TIMEOUT_MIN 15

static volatile bool active;
static bool alongside; /* opened next to a working connection, so it can be closed again */

static void end_alongside(void)
{
	wifi_stop_ap();
	active = false;
	ui_hide_text();
	LOG_INF("Setup ended");
}

static void timeout_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (active && alongside) {
		LOG_INF("Nobody set up a network in %d minutes", TIMEOUT_MIN);
		end_alongside();
	}
}
static K_WORK_DELAYABLE_DEFINE(timeout_work, timeout_handler);

/* Given by the middle button to cancel setup_boot(), which is otherwise waiting out the timeout */
static K_SEM_DEFINE(boot_cancel, 0, 1);

/* Open the access point and the web page, and put the details on the display */
static int begin(void)
{
	char ip[16];
	char text[112];
	int ret = wifi_start_ap(ip, sizeof(ip));

	if (ret == 0) {
		ret = portal_start();
	}
	if (ret) {
		return ret;
	}

	/* Enough for someone to join and open the page without looking anything up */
	snprintf(text, sizeof(text), "WIFI %s PW %s   %s   MIDDLE: CANCEL", CONFIG_TC001_AP_SSID,
		 CONFIG_TC001_AP_PSK[0] ? CONFIG_TC001_AP_PSK : "(OPEN)", ip);
	ui_show_text(text);
	active = true;
	LOG_INF("Setup page at http://%s/", ip);

	return 0;
}

void setup_boot(void)
{
	alongside = false;
	if (begin() != 0) {
		ui_set_status("FAIL");
		return;
	}

	/* Wait for a network to be saved (which reboots on its own, see portal.c), the middle
	 * button to cancel, or the timeout: if nobody sets up a network the surroundings may have
	 * changed, so start over.
	 */
	if (k_sem_take(&boot_cancel, K_MINUTES(TIMEOUT_MIN)) == 0) {
		LOG_INF("Setup cancelled from the button, showing the apps instead");
		wifi_stop_ap();
		active = false;
		return;
	}

	LOG_INF("Nobody set up a network in %d minutes, restarting", TIMEOUT_MIN);
	sys_reboot(SYS_REBOOT_COLD);
}

static void start_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (active) {
		return;
	}
	alongside = true;
	if (begin() != 0) {
		LOG_ERR("Could not start the setup access point");
		wifi_stop_ap();
		return;
	}
	k_work_reschedule(&timeout_work, K_MINUTES(TIMEOUT_MIN));
}
static K_WORK_DEFINE(start_work, start_handler);

static void stop_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (active && alongside) {
		k_work_cancel_delayable(&timeout_work);
		end_alongside();
	}
}
static K_WORK_DEFINE(stop_work, stop_handler);

void setup_request(void)
{
	k_work_submit(&start_work);
}

bool setup_active(void)
{
	return active;
}

void setup_cancel(void)
{
	if (!active) {
		return;
	}
	if (alongside) {
		/* On the work queue: stopping the access point takes a moment */
		k_work_submit(&stop_work);
	} else {
		k_sem_give(&boot_cancel);
	}
}
