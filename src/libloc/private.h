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

#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <syslog.h>

#include <libloc/libloc.h>

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
#    define secure_getenv getenv
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
	return ((address->s6_addr[i / 8] >> (7 - (i % 8))) & 1);
}

static inline void in6_addr_set_bit(struct in6_addr* address, unsigned int i, unsigned int val) {
	address->s6_addr[i / 8] ^= (-val ^ address->s6_addr[i / 8]) & (1 << (7 - (i % 8)));
}

static inline struct in6_addr loc_prefix_to_bitmask(const unsigned int prefix) {
	struct in6_addr bitmask;

	for (unsigned int i = 0; i < 16; i++)
		bitmask.s6_addr[i] = 0;

	for (int i = prefix, j = 0; i > 0; i -= 8, j++) {
		if (i >= 8)
			bitmask.s6_addr[j] = 0xff;
		else
			bitmask.s6_addr[j] = 0xff << (8 - i);
	}

	return bitmask;
}

static inline unsigned int __loc_address6_bit_length(const struct in6_addr* address) {
	unsigned int length = 128;

	for (int octet = 0; octet <= 15; octet++) {
		if (address->s6_addr[octet]) {
			length -= __builtin_clz(address->s6_addr[octet]) - 24;
			break;
		} else
			length -= 8;
	}

	return length;
}

static inline unsigned int __loc_address4_bit_length(const struct in6_addr* address) {
	unsigned int length = 32;

	for (int octet = 12; octet <= 15; octet++) {
		if (address->s6_addr[octet]) {
			length -= __builtin_clz(address->s6_addr[octet]) - 24;
			break;
		} else
			length -= 8;
	}

	return length;
}

static inline unsigned int loc_address_bit_length(const struct in6_addr* address) {
	if (IN6_IS_ADDR_V4MAPPED(address))
		return __loc_address4_bit_length(address);
	else
		return __loc_address6_bit_length(address);
}

static inline struct in6_addr loc_address_and(
		const struct in6_addr* address, const struct in6_addr* bitmask) {
	struct in6_addr a;

	// Perform bitwise AND
	for (unsigned int i = 0; i < 4; i++)
		a.s6_addr32[i] = address->s6_addr32[i] & bitmask->s6_addr32[i];

	return a;
}

static inline struct in6_addr loc_address_or(
		const struct in6_addr* address, const struct in6_addr* bitmask) {
	struct in6_addr a;

	// Perform bitwise OR
	for (unsigned int i = 0; i < 4; i++)
		a.s6_addr32[i] = address->s6_addr32[i] | ~bitmask->s6_addr32[i];

	return a;
}

static inline int __loc_address6_sub(struct in6_addr* result,
		const struct in6_addr* address1, const struct in6_addr* address2) {
	int remainder = 0;

	for (int octet = 15; octet >= 0; octet--) {
		int x = address1->s6_addr[octet] - address2->s6_addr[octet] + remainder;

		// Store remainder for the next iteration
		remainder = (x >> 8);

		result->s6_addr[octet] = x & 0xff;
	}

	return 0;
}

static inline int __loc_address4_sub(struct in6_addr* result,
		const struct in6_addr* address1, const struct in6_addr* address2) {
	int remainder = 0;

	for (int octet = 4; octet >= 0; octet--) {
		int x = address1->s6_addr[octet] - address2->s6_addr[octet] + remainder;

		// Store remainder for the next iteration
		remainder = (x >> 8);

		result->s6_addr[octet] = x & 0xff;
	}

	return 0;
}

static inline int loc_address_sub(struct in6_addr* result,
		const struct in6_addr* address1, const struct in6_addr* address2) {
	// XXX should be using loc_address_family
	int family1 = IN6_IS_ADDR_V4MAPPED(address1) ? AF_INET : AF_INET6;
	int family2 = IN6_IS_ADDR_V4MAPPED(address2) ? AF_INET : AF_INET6;

	// Address family must match
	if (family1 != family2) {
		errno = EINVAL;
		return 1;
	}

	// Clear result
	int r = loc_address_reset(result, family1);
	if (r)
		return r;

	switch (family1) {
		case AF_INET6:
			return __loc_address6_sub(result, address1, address2);

		case AF_INET:
			return __loc_address4_sub(result, address1, address2);

		default:
			errno = ENOTSUP;
			return 1;
	}
}

static inline void hexdump(struct loc_ctx* ctx, const void* addr, size_t len) {
	char buffer_hex[16 * 3 + 6];
	char buffer_ascii[17];

	unsigned int i = 0;
	unsigned char* p = (unsigned char*)addr;

	DEBUG(ctx, "Dumping %zu byte(s)\n", len);

	// Process every byte in the data
	for (i = 0; i < len; i++) {
		// Multiple of 16 means new line (with line offset)
		if ((i % 16) == 0) {
			// Just don't print ASCII for the zeroth line
			if (i != 0)
				DEBUG(ctx, "  %s %s\n", buffer_hex, buffer_ascii);

			// Output the offset.
			sprintf(buffer_hex, "%04x ", i);
		}

		// Now the hex code for the specific character
		sprintf(buffer_hex + 5 + ((i % 16) * 3), " %02x", p[i]);

		// And store a printable ASCII character for later
		if ((p[i] < 0x20) || (p[i] > 0x7e))
			buffer_ascii[i % 16] = '.';
		else
			buffer_ascii[i % 16] = p[i];

		// Terminate string
		buffer_ascii[(i % 16) + 1] = '\0';
	}

	// Pad out last line if not exactly 16 characters
	while ((i % 16) != 0) {
		sprintf(buffer_hex + 5 + ((i % 16) * 3), "   ");
		i++;
	}

	// And print the final bit
	DEBUG(ctx, "  %s %s\n", buffer_hex, buffer_ascii);
}

#endif
#endif
