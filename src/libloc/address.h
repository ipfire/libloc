/*
	libloc - A library to determine the location of someone on the Internet

	Copyright (C) 2022 IPFire Development Team <info@ipfire.org>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
*/

#ifndef LIBLOC_ADDRESS_H
#define LIBLOC_ADDRESS_H

#ifdef LIBLOC_PRIVATE

#include <errno.h>
#include <netinet/in.h>

/*
	All of these functions are private and for internal use only
*/

static inline int loc_address_family(const struct in6_addr* address) {
	if (IN6_IS_ADDR_V4MAPPED(address))
		return AF_INET;
	else
		return AF_INET6;
}

static inline int loc_address_cmp(const struct in6_addr* a1, const struct in6_addr* a2) {
	for (unsigned int i = 0; i < 16; i++) {
		if (a1->s6_addr[i] > a2->s6_addr[i])
			return 1;

		else if (a1->s6_addr[i] < a2->s6_addr[i])
			return -1;
	}

	return 0;
}

static inline int loc_address_get_bit(const struct in6_addr* address, unsigned int i) {
	return ((address->s6_addr[i / 8] >> (7 - (i % 8))) & 1);
}

static inline void loc_address_set_bit(struct in6_addr* address, unsigned int i, unsigned int val) {
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

static inline int loc_address_reset(struct in6_addr* address, int family) {
	switch (family) {
		case AF_INET6:
			address->s6_addr32[0] = 0x0000;
			address->s6_addr32[1] = 0x0000;
			address->s6_addr32[2] = 0x0000;
			address->s6_addr32[3] = 0x0000;
			return 0;

		case AF_INET:
			address->s6_addr32[0] = 0x0000;
			address->s6_addr32[1] = 0x0000;
			address->s6_addr32[2] = htonl(0xffff);
			address->s6_addr32[3] = 0x0000;
			return 0;
	}

	return -1;
}

static inline int loc_address_reset_last(struct in6_addr* address, int family) {
	switch (family) {
		case AF_INET6:
			address->s6_addr32[0] = 0xffff;
			address->s6_addr32[1] = 0xffff;
			address->s6_addr32[2] = 0xffff;
			address->s6_addr32[3] = 0xffff;
			return 0;

		case AF_INET:
			address->s6_addr32[0] = 0x0000;
			address->s6_addr32[1] = 0x0000;
			address->s6_addr32[2] = htonl(0xffff);
			address->s6_addr32[3] = 0xffff;
			return 0;
	}

	return -1;
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

	for (int octet = 15; octet >= 12; octet--) {
		int x = address1->s6_addr[octet] - address2->s6_addr[octet] + remainder;

		// Store remainder for the next iteration
		remainder = (x >> 8);

		result->s6_addr[octet] = x & 0xff;
	}

	return 0;
}

static inline int loc_address_sub(struct in6_addr* result,
		const struct in6_addr* address1, const struct in6_addr* address2) {
	int family1 = loc_address_family(address1);
	int family2 = loc_address_family(address2);

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

static inline struct in6_addr address_increment(const struct in6_addr* address) {
	struct in6_addr a = *address;

	for (int octet = 15; octet >= 0; octet--) {
		if (a.s6_addr[octet] < 255) {
			a.s6_addr[octet]++;
			break;
		} else {
			a.s6_addr[octet] = 0;
		}
	}

	return a;
}

static inline struct in6_addr address_decrement(const struct in6_addr* address) {
	struct in6_addr a = *address;

	for (int octet = 15; octet >= 0; octet--) {
		if (a.s6_addr[octet] > 0) {
			a.s6_addr[octet]--;
			break;
		}
	}

	return a;
}

static inline int loc_address_family_bit_length(const int family) {
	switch (family) {
		case AF_INET6:
			return 128;

		case AF_INET:
			return 32;

		default:
			return -1;
	}
}

static inline int loc_address_all_zeroes(const struct in6_addr* address) {
	struct in6_addr all_zeroes = IN6ADDR_ANY_INIT;

	const int family = loc_address_family(address);

	int r = loc_address_reset(&all_zeroes, family);
	if (r)
		return r;

	if (loc_address_cmp(address, &all_zeroes) == 0)
		return 1;

	return 0;
}

static inline int loc_address_count_trailing_zero_bits(const struct in6_addr* address) {
	int zeroes = 0;

	for (int octet = 15; octet >= 0; octet--) {
		if (address->s6_addr[octet]) {
			zeroes += __builtin_ctz(address->s6_addr[octet]);
			break;
		} else
			zeroes += 8;
	}

	return zeroes;
}

#endif /* LIBLOC_PRIVATE */

#endif /* LIBLOC_ADDRESS_H */
