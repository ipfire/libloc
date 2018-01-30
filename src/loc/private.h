/*
	libloc - A library to determine the location of someone on the Internet

	Copyright (C) 2017 IPFire Development Team <info@ipfire.org>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
*/

#ifndef LIBLOC_PRIVATE_H
#define LIBLOC_PRIVATE_H

#ifdef LIBLOC_PRIVATE

#include <netinet/in.h>
#include <stdbool.h>
#include <syslog.h>

#include <loc/libloc.h>

static inline void __attribute__((always_inline, format(printf, 2, 3)))
loc_log_null(struct loc_ctx *ctx, const char *format, ...) {}

#define loc_log_cond(ctx, prio, arg...) \
	do { \
		if (loc_get_log_priority(ctx) >= prio) \
			loc_log(ctx, prio, __FILE__, __LINE__, __FUNCTION__, ## arg); \
	} while (0)

#ifdef ENABLE_DEBUG
#  define DEBUG(ctx, arg...) loc_log_cond(ctx, LOG_DEBUG, ## arg)
#else
#  define DEBUG(ctx, arg...) loc_log_null(ctx, ## arg)
#endif

#define INFO(ctx, arg...) loc_log_cond(ctx, LOG_INFO, ## arg)
#define ERROR(ctx, arg...) loc_log_cond(ctx, LOG_ERR, ## arg)

#ifndef HAVE_SECURE_GETENV
#  ifdef HAVE___SECURE_GETENV
#    define secure_getenv __secure_getenv
#  else
#    error neither secure_getenv nor __secure_getenv is available
#  endif
#endif

#define LOC_EXPORT __attribute__ ((visibility("default")))

void loc_log(struct loc_ctx *ctx,
	int priority, const char *file, int line, const char *fn,
	const char *format, ...) __attribute__((format(printf, 6, 7)));

static inline int in6_addr_cmp(const struct in6_addr* a1, const struct in6_addr* a2) {
	for (unsigned int i = 0; i < 16; i++) {
		if (a1->s6_addr[i] > a2->s6_addr[i])
			return 1;

		else if (a1->s6_addr[i] < a2->s6_addr[i])
			return -1;
	}

	return 0;
}

static inline int in6_addr_get_bit(const struct in6_addr* address, unsigned int i) {
	return ((address->s6_addr[i / 8] >> (i % 8)) & 1);
}

static inline void in6_addr_set_bit(struct in6_addr* address, unsigned int i, unsigned int val) {
	address->s6_addr[i / 8] ^= (-val ^ address->s6_addr[i / 8]) & (1 << (i % 8));
}

#endif
#endif
