/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/net/wifi_mgmt.h>

#include "wifi.h"

LOG_MODULE_REGISTER(wifi, LOG_LEVEL_INF);

#define MAX_ATTEMPTS      2
#define SCAN_ATTEMPTS     3 /* a scan sometimes misses a network that is there */
#define SCAN_RETRIES      5
#define SCAN_TIMEOUT_S    15
#define CONNECT_TIMEOUT_S 20
#define IP_TIMEOUT_S      15
#define RETRY_DELAY_MS    1000
#define MAX_SAVED         CONFIG_WIFI_CREDENTIALS_MAX_ENTRIES

struct scan_entry {
	char ssid[WIFI_SSID_MAX_LEN + 1];
	uint8_t ssid_len;
	int8_t rssi;
	enum wifi_security_type security;
};

/*
 * A scan in a crowded neighbourhood returns dozens of access points, more than are worth keeping.
 * Only the ones that belong to a saved network matter, so only those are kept: at most one per
 * saved network, the strongest.
 */
static struct scan_entry scan_results[MAX_SAVED];
static size_t scan_count;

static char saved_ssids[MAX_SAVED][WIFI_SSID_MAX_LEN + 1];
static uint8_t saved_lens[MAX_SAVED];
static size_t saved_count;

static void collect_saved(void *cb_arg, const char *ssid, size_t len)
{
	ARG_UNUSED(cb_arg);

	if (saved_count < MAX_SAVED && len <= WIFI_SSID_MAX_LEN) {
		memcpy(saved_ssids[saved_count], ssid, len);
		saved_lens[saved_count] = len;
		saved_count++;
	}
}

static bool is_saved(const uint8_t *ssid, size_t len)
{
	for (size_t i = 0; i < saved_count; i++) {
		if (saved_lens[i] == len && memcmp(saved_ssids[i], ssid, len) == 0) {
			return true;
		}
	}
	return false;
}

/* Remember an access point of a saved network, or improve on the one already kept for it */
static void keep_scan_result(const struct wifi_scan_result *r)
{
	struct scan_entry *e = NULL;

	for (size_t i = 0; i < scan_count; i++) {
		if (scan_results[i].ssid_len == r->ssid_length &&
		    memcmp(scan_results[i].ssid, r->ssid, r->ssid_length) == 0) {
			e = &scan_results[i];
			break;
		}
	}
	if (e != NULL && e->rssi >= r->rssi) {
		return;
	}
	if (e == NULL) {
		if (scan_count >= MAX_SAVED) {
			return;
		}
		e = &scan_results[scan_count++];
	}

	memcpy(e->ssid, r->ssid, r->ssid_length);
	e->ssid[r->ssid_length] = '\0';
	e->ssid_len = r->ssid_length;
	e->rssi = r->rssi;
	e->security = r->security;
}

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;
static K_SEM_DEFINE(scan_done, 0, 1);
static K_SEM_DEFINE(connect_done, 0, 1);

static char connected_ssid[WIFI_SSID_MAX_LEN + 1];
static char connected_address[NET_IPV4_ADDR_LEN];
static K_SEM_DEFINE(got_ip, 0, 1);
static int connect_status;

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t event,
			       struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (event) {
	case NET_EVENT_WIFI_SCAN_RESULT: {
		const struct wifi_scan_result *r = cb->info;

		if (r->ssid_length > 0 && is_saved(r->ssid, r->ssid_length)) {
			keep_scan_result(r);
		}
		break;
	}
	case NET_EVENT_WIFI_SCAN_DONE:
		k_sem_give(&scan_done);
		break;
	case NET_EVENT_WIFI_CONNECT_RESULT: {
		const struct wifi_status *s = cb->info;

		connect_status = s->status;
		k_sem_give(&connect_done);
		break;
	}
	default:
		break;
	}
}

static void ipv4_event_handler(struct net_mgmt_event_callback *cb, uint64_t event,
			       struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	if (event == NET_EVENT_IPV4_ADDR_ADD) {
		k_sem_give(&got_ip);
	}
}

static void register_callbacks(void)
{
	static bool done;

	if (done) {
		return;
	}
	done = true;

	net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
				     NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE |
					     NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	net_mgmt_init_event_callback(&ipv4_cb, ipv4_event_handler, NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ipv4_cb);
}

static int scan(struct net_if *iface)
{
	struct wifi_scan_params params = {0};
	int ret = -EAGAIN;

	scan_count = 0;
	saved_count = 0;
	wifi_credentials_for_each_ssid(collect_saved, NULL);
	k_sem_reset(&scan_done);

	/* The Wi-Fi stack may still be starting up right after boot */
	for (int i = 0; i < SCAN_RETRIES; i++) {
		ret = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, &params, sizeof(params));
		if (ret == 0) {
			break;
		}
		LOG_WRN("Scan request failed (%d), retrying", ret);
		k_msleep(RETRY_DELAY_MS);
	}
	if (ret) {
		return ret;
	}

	if (k_sem_take(&scan_done, K_SECONDS(SCAN_TIMEOUT_S)) != 0) {
		return -ETIMEDOUT;
	}

	LOG_INF("Scan found %u of %u saved network(s)", (unsigned int)scan_count,
		(unsigned int)saved_count);
	return 0;
}

/* Strongest of the saved networks that were seen in the scan, with its credentials, or NULL */
static const struct scan_entry *pick_saved_network(struct wifi_credentials_personal *creds)
{
	const struct scan_entry *best = NULL;

	for (size_t i = 0; i < scan_count; i++) {
		const struct scan_entry *e = &scan_results[i];

		if (best == NULL || e->rssi > best->rssi) {
			best = e;
		}
	}

	if (best != NULL &&
	    wifi_credentials_get_by_ssid_personal_struct(best->ssid, best->ssid_len, creds) != 0) {
		return NULL;
	}

	return best;
}

static int connect_once(struct net_if *iface, const struct scan_entry *network,
			const struct wifi_credentials_personal *creds)
{
	struct wifi_connect_req_params params = {
		.ssid = network->ssid,
		.ssid_length = network->ssid_len,
		.psk = creds->password,
		.psk_length = creds->password_len,
		.security = WIFI_SECURITY_TYPE_NONE,
		.channel = WIFI_CHANNEL_ANY,
		.band = WIFI_FREQ_BAND_2_4_GHZ,
		.mfp = WIFI_MFP_OPTIONAL,
	};
	int ret;

	if (creds->password_len > 0) {
		/* Use what the access point advertises so WPA3 networks work too */
		params.security = network->security == WIFI_SECURITY_TYPE_NONE
					  ? WIFI_SECURITY_TYPE_PSK
					  : network->security;
	}

	k_sem_reset(&connect_done);
	k_sem_reset(&got_ip);
	connect_status = -1;

	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("Connect request failed: %d", ret);
		return ret;
	}

	if (k_sem_take(&connect_done, K_SECONDS(CONNECT_TIMEOUT_S)) != 0) {
		return -ETIMEDOUT;
	}
	if (connect_status != 0) {
		LOG_ERR("Association failed: %d", connect_status);
		return -ECONNREFUSED;
	}
	if (k_sem_take(&got_ip, K_SECONDS(IP_TIMEOUT_S)) != 0) {
		LOG_ERR("No IPv4 address from DHCP");
		return -ETIMEDOUT;
	}

	return 0;
}

int wifi_connect_saved(void)
{
	struct net_if *iface = net_if_get_wifi_sta();
	static struct wifi_credentials_personal creds;
	const struct scan_entry *network;
	int ret;

	if (iface == NULL) {
		LOG_ERR("No Wi-Fi station interface");
		return -ENODEV;
	}
	if (wifi_credentials_is_empty()) {
		LOG_INF("No saved networks");
		return -ENOENT;
	}

	register_callbacks();

	for (int i = 1; i <= SCAN_ATTEMPTS; i++) {
		ret = scan(iface);
		if (ret) {
			LOG_ERR("Scan failed: %d", ret);
			return ret;
		}

		network = pick_saved_network(&creds);
		if (network != NULL) {
			break;
		}
		if (i < SCAN_ATTEMPTS) {
			LOG_INF("No saved network seen (scan %d of %d), scanning again", i,
				SCAN_ATTEMPTS);
			k_msleep(RETRY_DELAY_MS);
		}
	}
	if (network == NULL) {
		LOG_INF("None of the saved networks is in range");
		return -ENOENT;
	}

	for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
		LOG_INF("Connecting to \"%s\" (attempt %d/%d)", network->ssid, attempt,
			MAX_ATTEMPTS);
		ret = connect_once(iface, network, &creds);
		if (ret == 0) {
			struct net_in_addr *addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
			char ip[NET_IPV4_ADDR_LEN] = "?";

			if (addr != NULL) {
				net_addr_ntop(NET_AF_INET, addr, ip, sizeof(ip));
			}
			strncpy(connected_address, ip, sizeof(connected_address) - 1);
			strncpy(connected_ssid, network->ssid, sizeof(connected_ssid) - 1);
			LOG_INF("Connected, address %s", ip);
			return 0;
		}

		net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
		k_msleep(RETRY_DELAY_MS);
	}

	return ret;
}

const char *wifi_ssid(void)
{
	return connected_ssid;
}

const char *wifi_address(void)
{
	return connected_address;
}

int wifi_start_ap(char *ip, size_t ip_len)
{
	struct net_if *iface = net_if_get_wifi_sap();
	struct net_in_addr addr;
	struct net_in_addr netmask;
	struct net_in_addr pool_start;
	struct wifi_connect_req_params params = {
		.ssid = CONFIG_TC001_AP_SSID,
		.ssid_length = strlen(CONFIG_TC001_AP_SSID),
		.psk = CONFIG_TC001_AP_PSK,
		.psk_length = strlen(CONFIG_TC001_AP_PSK),
		.security = strlen(CONFIG_TC001_AP_PSK) ? WIFI_SECURITY_TYPE_PSK
							: WIFI_SECURITY_TYPE_NONE,
		.channel = WIFI_CHANNEL_ANY,
		.band = WIFI_FREQ_BAND_2_4_GHZ,
		.mfp = WIFI_MFP_OPTIONAL,
	};
	int ret;

	if (iface == NULL) {
		LOG_ERR("No Wi-Fi access point interface");
		return -ENODEV;
	}
	if (net_addr_pton(NET_AF_INET, CONFIG_TC001_AP_IP, &addr) ||
	    net_addr_pton(NET_AF_INET, "255.255.255.0", &netmask)) {
		LOG_ERR("Invalid CONFIG_TC001_AP_IP \"%s\"", CONFIG_TC001_AP_IP);
		return -EINVAL;
	}

	net_if_ipv4_set_gw(iface, &addr);
	if (net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0) == NULL) {
		LOG_ERR("Cannot set AP address");
		return -ENOMEM;
	}
	net_if_ipv4_set_netmask_by_addr(iface, &addr, &netmask);

	/* Hand out addresses starting ten above the AP's own */
	pool_start = addr;
	pool_start.s4_addr[3] += 10;
	ret = net_dhcpv4_server_start(iface, &pool_start);
	if (ret) {
		LOG_ERR("DHCP server failed to start: %d", ret);
		return ret;
	}

	ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("AP enable failed: %d", ret);
		return ret;
	}

	strncpy(ip, CONFIG_TC001_AP_IP, ip_len - 1);
	ip[ip_len - 1] = '\0';
	LOG_INF("Access point \"%s\" up at %s", CONFIG_TC001_AP_SSID, ip);

	return 0;
}

bool wifi_is_ap_address(const struct net_in_addr *addr)
{
	struct net_in_addr ap;

	return net_addr_pton(NET_AF_INET, CONFIG_TC001_AP_IP, &ap) == 0 &&
	       memcmp(&ap, addr, sizeof(ap)) == 0;
}

void wifi_stop_ap(void)
{
	struct net_if *iface = net_if_get_wifi_sap();
	struct net_in_addr addr;

	if (iface == NULL) {
		return;
	}

	net_mgmt(NET_REQUEST_WIFI_AP_DISABLE, iface, NULL, 0);
	net_dhcpv4_server_stop(iface);
	if (net_addr_pton(NET_AF_INET, CONFIG_TC001_AP_IP, &addr) == 0) {
		net_if_ipv4_addr_rm(iface, &addr);
	}
	LOG_INF("Access point stopped");
}
