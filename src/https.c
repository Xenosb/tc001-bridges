/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include "https.h"
#include "resp_scan.h"

LOG_MODULE_REGISTER(https, LOG_LEVEL_INF);

#define HTTPS_PORT      "443"
#define IO_TIMEOUT_MS   10000
#define USER_AGENT      "tc001-bridges/1.0 (Ulanzi TC001 stats display)"

/* Root CAs, DER encoded, in the order of enum https_root */
static const unsigned char usertrust_ecc_root[] = {
#include "usertrust_ecc_root.der.inc"
};
static const unsigned char globalsign_r3_root[] = {
#include "globalsign_r3_root.der.inc"
};
static const unsigned char digicert_global_g2_root[] = {
#include "digicert_global_g2_root.der.inc"
};
static const unsigned char gts_root_r4[] = {
#include "gts_root_r4.der.inc"
};
static const unsigned char isrg_root_x1[] = {
#include "isrg_root_x1.der.inc"
};

/* Responses are consumed as they arrive, so this only needs to be a read chunk */
static char chunk[512];

int https_init(void)
{
	static const struct {
		const unsigned char *der;
		size_t len;
	} roots[HTTPS_ROOT_COUNT] = {
		[HTTPS_ROOT_USERTRUST_ECC] = {usertrust_ecc_root, sizeof(usertrust_ecc_root)},
		[HTTPS_ROOT_GLOBALSIGN_R3] = {globalsign_r3_root, sizeof(globalsign_r3_root)},
		[HTTPS_ROOT_DIGICERT_G2] = {digicert_global_g2_root, sizeof(digicert_global_g2_root)},
		[HTTPS_ROOT_GTS_R4] = {gts_root_r4, sizeof(gts_root_r4)},
		[HTTPS_ROOT_ISRG_X1] = {isrg_root_x1, sizeof(isrg_root_x1)},
	};

	for (size_t i = 0; i < ARRAY_SIZE(roots); i++) {
		/* Zephyr keeps one CA credential per tag, so each root gets its own tag */
		int ret = tls_credential_add(i + 1, TLS_CREDENTIAL_CA_CERTIFICATE, roots[i].der,
					     roots[i].len);

		if (ret < 0) {
			LOG_ERR("Cannot register root certificate %u: %d", (unsigned int)i, ret);
			return ret;
		}
	}

	return 0;
}

static int set_option(int sock, int level, int name, const void *value, size_t len,
		      const char *what)
{
	if (zsock_setsockopt(sock, level, name, value, len) < 0) {
		LOG_ERR("Cannot set %s: %d", what, errno);
		return -1;
	}
	return 0;
}

static int open_connection(const char *host, enum https_root root)
{
	struct zsock_addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct zsock_addrinfo *res;
	struct zsock_timeval timeout = {
		.tv_sec = IO_TIMEOUT_MS / 1000,
	};
	sec_tag_t tags[] = {root + 1};
	int sock;
	int ret;

	ret = zsock_getaddrinfo(host, HTTPS_PORT, &hints, &res);
	if (ret) {
		LOG_ERR("Cannot resolve %s: %d", host, ret);
		return -EIO;
	}

	sock = zsock_socket(res->ai_family, SOCK_STREAM, IPPROTO_TLS_1_2);
	if (sock < 0) {
		LOG_ERR("Cannot create TLS socket: %d", errno);
		zsock_freeaddrinfo(res);
		return -EIO;
	}

	if (set_option(sock, SOL_TLS, TLS_SEC_TAG_LIST, tags, sizeof(tags), "TLS_SEC_TAG_LIST") ||
	    set_option(sock, SOL_TLS, TLS_HOSTNAME, host, strlen(host) + 1, "TLS_HOSTNAME") ||
	    set_option(sock, ZSOCK_SOL_SOCKET, ZSOCK_SO_RCVTIMEO, &timeout, sizeof(timeout),
		       "SO_RCVTIMEO") ||
	    set_option(sock, ZSOCK_SOL_SOCKET, ZSOCK_SO_SNDTIMEO, &timeout, sizeof(timeout),
		       "SO_SNDTIMEO")) {
		zsock_close(sock);
		zsock_freeaddrinfo(res);
		return -EIO;
	}

	ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
	zsock_freeaddrinfo(res);
	if (ret < 0) {
		LOG_ERR("TLS connect to %s failed: %d", host, errno);
		zsock_close(sock);
		return -EIO;
	}

	return sock;
}

int https_get(const char *host, enum https_root root, const char *path, struct resp_field *fields,
	      size_t count)
{
	struct resp_scan scan;
	char request[320];
	bool found = false;
	int sock;
	int len;

	len = snprintf(request, sizeof(request),
		       "GET %s HTTP/1.0\r\n"
		       "Host: %s\r\n"
		       "User-Agent: " USER_AGENT "\r\n"
		       "Accept: application/json\r\n"
		       "Connection: close\r\n\r\n",
		       path, host);
	if (len < 0 || len >= sizeof(request)) {
		return -EINVAL;
	}

	sock = open_connection(host, root);
	if (sock < 0) {
		return sock;
	}

	if (zsock_send(sock, request, len, 0) < 0) {
		LOG_ERR("Send to %s failed: %d", host, errno);
		zsock_close(sock);
		return -EIO;
	}

	resp_scan_init(&scan, fields, count);
	while (!found) {
		int n = zsock_recv(sock, chunk, sizeof(chunk), 0);

		if (n < 0) {
			LOG_ERR("Receive from %s failed: %d", host, errno);
			zsock_close(sock);
			return -EIO;
		}
		if (n == 0) {
			found = resp_scan_finish(&scan);
			break;
		}
		found = resp_scan_feed(&scan, chunk, n);
	}
	zsock_close(sock);

	if (resp_scan_status(&scan) != 200) {
		LOG_WRN("%s answered with status %d", host, resp_scan_status(&scan));
		return -EPROTO;
	}
	if (!found) {
		LOG_WRN("Not every field was found in the response from %s", host);
		return -ENOENT;
	}

	return 0;
}
