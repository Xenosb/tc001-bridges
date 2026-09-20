/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/service.h>
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/sys/reboot.h>

#include "config.h"
#include "form.h"
#include "portal.h"
#include "wifi.h"

LOG_MODULE_REGISTER(portal, LOG_LEVEL_INF);

#define PSK_MIN_LEN     8
#define PSK_MAX_LEN     63
#define REBOOT_DELAY_MS 2000

/*
 * A request that arrives on the clock's own access point gets the Wi-Fi setup page, and one that
 * arrives over the home network gets the settings page. Wi-Fi credentials are never offered on the
 * home network, so nobody else on it can read or replace them. Set at the start of each request.
 */
static bool request_from_ap;

static void note_request(struct http_client_ctx *client)
{
	struct sockaddr_storage local;
	socklen_t len = sizeof(local);

	request_from_ap = zsock_getsockname(client->fd, (struct sockaddr *)&local, &len) == 0 &&
			  local.ss_family == AF_INET &&
			  wifi_is_ap_address(&((struct sockaddr_in *)&local)->sin_addr);
}

/* Room for a full list of long, fully escaped SSIDs plus the static markup */
static char page[5120];
static size_t page_len;

/* Form bodies are at most ~350 bytes, but arrive in client-buffer sized pieces */
static char post_body[512];
static size_t post_len;

static void reboot_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_INF("Rebooting to apply the saved network");
	sys_reboot(SYS_REBOOT_COLD);
}
static K_WORK_DELAYABLE_DEFINE(reboot_work, reboot_handler);

static void append(const char *text)
{
	size_t n = strlen(text);

	if (page_len + n + 1 > sizeof(page)) {
		return;
	}
	memcpy(page + page_len, text, n + 1);
	page_len += n;
}

static void append_saved_cb(void *cb_arg, const char *ssid, size_t ssid_len)
{
	char name[WIFI_SSID_MAX_LEN + 1];

	ARG_UNUSED(cb_arg);

	memcpy(name, ssid, ssid_len);
	name[ssid_len] = '\0';

	append("<li><span>");
	html_escape_append(page, sizeof(page), &page_len, name);
	append("</span><form method=\"post\" action=\"/forget\">"
	       "<input type=\"hidden\" name=\"ssid\" value=\"");
	html_escape_append(page, sizeof(page), &page_len, name);
	append("\"><button class=\"quiet\">Forget</button></form></li>");
}

static void append_head(const char *title, const char *notice)
{
	append("<!doctype html><html><head><meta charset=\"utf-8\">"
	       "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
	       "<title>");
	append(title);
	append("</title><style>"
	       "body{font:16px system-ui,sans-serif;max-width:28rem;margin:1rem auto;padding:0 1rem}"
	       "li{display:flex;justify-content:space-between;align-items:center;"
	       "padding:.4rem 0;border-bottom:1px solid #ddd}"
	       "li form{margin:0}label{display:block;margin:.8rem 0 .2rem}"
	       "label.opt{display:flex;gap:.6rem;align-items:center;margin:.4rem 0}"
	       "input[type=text],input[type=password]{width:100%;box-sizing:border-box;padding:.5rem}"
	       "button{margin-top:1rem;padding:.6rem 1.2rem}button.quiet{margin:0;padding:.2rem .6rem}"
	       ".notice{padding:.6rem;background:#eef;border-radius:4px}"
	       "</style></head><body><h1>");
	append(title);
	append("</h1>");

	if (notice != NULL) {
		append("<p class=\"notice\">");
		html_escape_append(page, sizeof(page), &page_len, notice);
		append("</p>");
	}
}

static void render_wifi_page(const char *notice)
{
	append_head("TC001 Wi-Fi", notice);

	append("<h2>Saved networks</h2>");
	if (wifi_credentials_is_empty()) {
		append("<p>None yet.</p>");
	} else {
		append("<ul style=\"list-style:none;padding:0\">");
		wifi_credentials_for_each_ssid(append_saved_cb, NULL);
		append("</ul>");
	}

	append("<h2>Add a network</h2>"
	       "<form method=\"post\" action=\"/save\">"
	       "<label for=\"ssid\">Network name (SSID)</label>"
	       "<input id=\"ssid\" type=\"text\" name=\"ssid\" maxlength=\"32\" required>"
	       "<label for=\"password\">Password (leave empty for an open network)</label>"
	       "<input id=\"password\" type=\"password\" name=\"password\" maxlength=\"63\">"
	       "<button type=\"submit\">Save</button></form></body></html>");
}

/* The apps the user can switch on and off, in the order they are listed */
static const struct {
	enum app_id id;
	const char *field;
	const char *label;
} app_toggles[] = {
	{APP_CLOCK, "app_clock", "Clock and weather"},
	{APP_BRIDGES_TOTAL, "app_bridges_total", "Bridges total"},
	{APP_BRIDGES_TRENDS, "app_bridges_trends", "Bridges trends"},
	{APP_SCREENSAVER, "app_screensaver", "Screen saver"},
};

static void append_option(const char *type, const char *name, const char *value, bool checked,
			  const char *label)
{
	append("<label class=\"opt\"><input type=\"");
	append(type);
	append("\" name=\"");
	append(name);
	append("\"");
	if (value != NULL) {
		append(" value=\"");
		append(value);
		append("\"");
	}
	append(checked ? " checked" : "");
	append(">");
	append(label);
	append("</label>");
}

static void render_settings_page(const char *notice)
{
	const struct config *cfg = config_get();

	append_head("TC001 settings", notice);
	append("<form method=\"post\" action=\"/settings\"><h2>Apps</h2>");
	for (size_t i = 0; i < ARRAY_SIZE(app_toggles); i++) {
		append_option("checkbox", app_toggles[i].field, NULL,
			      config_app_enabled(cfg, app_toggles[i].id), app_toggles[i].label);
	}

	append("<h2>Date</h2>");
	append_option("radio", "date", "0", cfg->date_format == DATE_DAY_MONTH, "dd.mm.");
	append_option("radio", "date", "1", cfg->date_format == DATE_MONTH_DAY, "mm/dd");

	append("<h2>Time</h2>");
	append_option("radio", "time", "0", cfg->time_format == TIME_24H, "24 hour");
	append_option("radio", "time", "1", cfg->time_format == TIME_12H, "12 hour (with A or P)");

	append("<button type=\"submit\">Save</button></form></body></html>");
}

/* Build the whole page for the current mode; `notice` (may be NULL) is shown above the form. */
static void render_page(const char *notice)
{
	page_len = 0;
	page[0] = '\0';

	if (request_from_ap) {
		render_wifi_page(notice);
	} else {
		render_settings_page(notice);
	}
}

static void respond(struct http_response_ctx *rsp, enum http_status status, const char *notice)
{
	render_page(notice);
	rsp->status = status;
	rsp->body = (const uint8_t *)page;
	rsp->body_len = page_len;
	rsp->final_chunk = true;
}

static int index_handler(struct http_client_ctx *client, enum http_transaction_status status,
			 const struct http_request_ctx *request_ctx,
			 struct http_response_ctx *response_ctx, void *user_data)
{
	/* A GET carries no body: wait for the final callback, then send the page */
	note_request(client);
	if (status == HTTP_SERVER_REQUEST_DATA_FINAL) {
		respond(response_ctx, HTTP_200_OK, NULL);
	}

	return 0;
}

/*
 * Collect a POST body across callbacks. Returns true, with the complete
 * NUL-terminated body in post_body, once the final piece has arrived.
 */
static bool collect_body(enum http_transaction_status status,
			 const struct http_request_ctx *request_ctx)
{
	if (status == HTTP_SERVER_TRANSACTION_ABORTED ||
	    status == HTTP_SERVER_TRANSACTION_COMPLETE) {
		post_len = 0;
		return false;
	}

	if (request_ctx->data != NULL && request_ctx->data_len > 0) {
		if (post_len + request_ctx->data_len >= sizeof(post_body)) {
			post_len = sizeof(post_body); /* overflow: reject at the end */
		} else {
			memcpy(post_body + post_len, request_ctx->data, request_ctx->data_len);
			post_len += request_ctx->data_len;
		}
	}

	if (status != HTTP_SERVER_REQUEST_DATA_FINAL) {
		return false;
	}

	if (post_len >= sizeof(post_body)) {
		post_len = 0;
		post_body[0] = '\0';
		return true;
	}

	post_body[post_len] = '\0';
	post_len = 0;
	return true;
}

static int save_handler(struct http_client_ctx *client, enum http_transaction_status status,
			const struct http_request_ctx *request_ctx,
			struct http_response_ctx *response_ctx, void *user_data)
{
	char ssid[WIFI_SSID_MAX_LEN + 1];
	char password[PSK_MAX_LEN + 1];
	char notice[96];
	int ssid_len;
	int pw_len;
	int ret;

	note_request(client);
	if (!collect_body(status, request_ctx)) {
		return 0;
	}
	if (!request_from_ap) {
		respond(response_ctx, HTTP_403_FORBIDDEN,
			"Wi-Fi networks can only be changed in setup mode.");
		return 0;
	}

	ssid_len = form_get(post_body, "ssid", ssid, sizeof(ssid));
	pw_len = form_get(post_body, "password", password, sizeof(password));
	if (pw_len == -ENOENT) {
		/* No password field at all is the same as an empty one */
		password[0] = '\0';
		pw_len = 0;
	}

	if (ssid_len <= 0) {
		respond(response_ctx, HTTP_400_BAD_REQUEST,
			"Enter a network name of up to 32 characters.");
		return 0;
	}
	if (pw_len < 0 || (pw_len > 0 && pw_len < PSK_MIN_LEN)) {
		respond(response_ctx, HTTP_400_BAD_REQUEST,
			"The password must be 8 to 63 characters, or empty for an open network.");
		return 0;
	}

	ret = wifi_credentials_set_personal(ssid, ssid_len,
					    pw_len ? WIFI_SECURITY_TYPE_PSK : WIFI_SECURITY_TYPE_NONE,
					    NULL, 0, password, pw_len, 0, 0, 0);
	if (ret == -ENOBUFS) {
		respond(response_ctx, HTTP_409_CONFLICT,
			"The list is full. Forget a network before adding another.");
		return 0;
	}
	if (ret) {
		LOG_ERR("Saving credentials failed: %d", ret);
		respond(response_ctx, HTTP_500_INTERNAL_SERVER_ERROR,
			"Could not save the network.");
		return 0;
	}

	LOG_INF("Saved network \"%s\"", ssid);
	snprintf(notice, sizeof(notice), "Saved \"%s\". The clock is restarting to connect.", ssid);
	respond(response_ctx, HTTP_200_OK, notice);
	k_work_reschedule(&reboot_work, K_MSEC(REBOOT_DELAY_MS));

	return 0;
}

static int forget_handler(struct http_client_ctx *client, enum http_transaction_status status,
			  const struct http_request_ctx *request_ctx,
			  struct http_response_ctx *response_ctx, void *user_data)
{
	char ssid[WIFI_SSID_MAX_LEN + 1];
	char notice[96];
	int ssid_len;

	note_request(client);
	if (!collect_body(status, request_ctx)) {
		return 0;
	}
	if (!request_from_ap) {
		respond(response_ctx, HTTP_403_FORBIDDEN,
			"Wi-Fi networks can only be changed in setup mode.");
		return 0;
	}

	ssid_len = form_get(post_body, "ssid", ssid, sizeof(ssid));
	if (ssid_len <= 0 || wifi_credentials_delete_by_ssid(ssid, ssid_len) != 0) {
		respond(response_ctx, HTTP_404_NOT_FOUND, "That network is not saved.");
		return 0;
	}

	LOG_INF("Forgot network \"%s\"", ssid);
	snprintf(notice, sizeof(notice), "Forgot \"%s\".", ssid);
	respond(response_ctx, HTTP_200_OK, notice);

	return 0;
}

/* A radio button group's value: "0" to count-1, or -1 if missing or out of range */
static int choice(const char *field, int count)
{
	char value[4];

	if (form_get(post_body, field, value, sizeof(value)) != 1 || value[0] < '0' ||
	    value[0] >= '0' + count) {
		return -1;
	}
	return value[0] - '0';
}

static int settings_handler(struct http_client_ctx *client, enum http_transaction_status status,
			    const struct http_request_ctx *request_ctx,
			    struct http_response_ctx *response_ctx, void *user_data)
{
	struct config *cfg = config_get();
	char unused[8]; /* a ticked checkbox sends "on" (or whatever the browser uses) */
	uint8_t apps = cfg->apps;
	int date;
	int time;

	note_request(client);
	if (!collect_body(status, request_ctx)) {
		return 0;
	}
	if (request_from_ap) {
		respond(response_ctx, HTTP_404_NOT_FOUND, "Settings are available on the home network.");
		return 0;
	}

	/* A checkbox is in the form only if it was ticked */
	for (size_t i = 0; i < ARRAY_SIZE(app_toggles); i++) {
		if (form_get(post_body, app_toggles[i].field, unused, sizeof(unused)) >= 0) {
			apps |= BIT(app_toggles[i].id);
		} else {
			apps &= ~BIT(app_toggles[i].id);
		}
	}
	date = choice("date", DATE_FORMAT_COUNT);
	time = choice("time", TIME_FORMAT_COUNT);

	if (date < 0 || time < 0) {
		respond(response_ctx, HTTP_400_BAD_REQUEST, "Choose a date and a time format.");
		return 0;
	}
	if (!(apps & (BIT(APP_CLOCK) | BIT(APP_BRIDGES_TOTAL) | BIT(APP_BRIDGES_TRENDS) |
		      BIT(APP_SCREENSAVER)))) {
		respond(response_ctx, HTTP_400_BAD_REQUEST, "Keep at least one app switched on.");
		return 0;
	}

	cfg->apps = apps;
	cfg->date_format = date;
	cfg->time_format = time;
	config_save();
	LOG_INF("Settings saved: apps 0x%02x, date %d, time %d", apps, date, time);

	respond(response_ctx, HTTP_200_OK, "Saved.");

	return 0;
}

static struct http_resource_detail_dynamic index_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_GET),
		.content_type = "text/html",
	},
	.cb = index_handler,
};

static struct http_resource_detail_dynamic save_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "text/html",
	},
	.cb = save_handler,
};

static struct http_resource_detail_dynamic settings_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "text/html",
	},
	.cb = settings_handler,
};

static struct http_resource_detail_dynamic forget_detail = {
	.common = {
		.type = HTTP_RESOURCE_TYPE_DYNAMIC,
		.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		.content_type = "text/html",
	},
	.cb = forget_handler,
};

static uint16_t portal_port = 80;
HTTP_SERVICE_DEFINE(portal_service, NULL, &portal_port, CONFIG_HTTP_SERVER_MAX_CLIENTS, 4, NULL,
		    NULL, NULL);

HTTP_RESOURCE_DEFINE(index_resource, portal_service, "/", &index_detail);
HTTP_RESOURCE_DEFINE(save_resource, portal_service, "/save", &save_detail);
HTTP_RESOURCE_DEFINE(forget_resource, portal_service, "/forget", &forget_detail);
HTTP_RESOURCE_DEFINE(settings_resource, portal_service, "/settings", &settings_detail);

int portal_start(void)
{
	static bool started;
	int ret;

	if (started) {
		return 0;
	}
	ret = http_server_start();

	if (ret) {
		LOG_ERR("HTTP server failed to start: %d", ret);
	} else {
		started = true;
	}

	return ret;
}
