/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_HTTPS_H_
#define TC001_HTTPS_H_

#include "resp_scan.h"

/* Trusted root certificates, see certs/README.md. Each server is checked against one of them. */
enum https_root {
	HTTPS_ROOT_USERTRUST_ECC, /* api.github.com */
	HTTPS_ROOT_GLOBALSIGN_R3, /* crates.io */
	HTTPS_ROOT_DIGICERT_G2,   /* azuresearch-usnc.nuget.org */
	HTTPS_ROOT_GTS_R4,        /* ipwho.is */
	HTTPS_ROOT_ISRG_X1,       /* api.open-meteo.com */
	HTTPS_ROOT_COUNT,
};

/**
 * Register the trusted root certificates. Call once before https_get().
 *
 * @return 0 on success, negative errno on failure
 */
int https_init(void);

/**
 * GET https://<host><path> and extract numeric JSON fields from the answer.
 *
 * The server certificate is verified against @p root only, which keeps the handshake small.
 * Every entry of @p fields needs `key` (and `decimals`) set; see resp_scan.h.
 *
 * @return 0 if every field was found, otherwise a negative errno: -EIO for network or TLS
 *         errors, -EPROTO for a non-200 answer, -ENOENT if a field is not in the response
 */
int https_get(const char *host, enum https_root root, const char *path, struct resp_field *fields,
	      size_t count);

#endif /* TC001_HTTPS_H_ */
