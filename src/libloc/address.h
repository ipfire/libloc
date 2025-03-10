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

#include <libloc/compat.h>

/*
	All of these functions are private and for internal use only
*/

const char* loc_address_str(const struct in6_addr* address);
int loc_address_parse(struct in6_addr* address, unsigned int* prefix, const char* string);

static inline int loc_address_family(const struct in6_addr* address) {
	if (IN6_IS_ADDR_V4MAPPED(address))
		return AF_INET;
	else
		return AF_INET6;
}

static inline unsigned int loc_address_family_bit_length(const int family) {
	switch (family) {
		case AF_INET6:
			return 128;

		case AF_INET:
			return 32;

		default:
			return 0;
	}
}

/*
	Checks whether prefix is valid for the given address
*/
static inline int loc_address_valid_prefix(const struct in6_addr* address, unsigned int prefix) {
	const int family = loc_address_family(address);

	// What is the largest possible prefix?
	const unsigned int bit_length = loc_address_family_bit_length(family);

	if (prefix <= bit_length)
		return 1;

	return 0;
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

#define foreach_octet_in_address(octet, address) \
	for (octet = (IN6_IS_ADDR_V4MAPPED(address) ? 12 : 0); octet <= 15; octet++)

#define foreach_octet_in_address_reverse(octet, address) \
	for (octet = 15; octet >= (IN6_IS_ADDR_V4MAPPED(address) ? 12 : 0); octet--)

static inline int loc_address_all_zeroes(const struct in6_addr* address) {
	int octet = 0;

	foreach_octet_in_address(octet, address) {
		if (address->s6_addr[octet])
			return 0;
	}

	return 1;
}

static inline int loc_address_all_ones(const struct in6_addr* address) {
	int octet = 0;

	foreach_octet_in_address(octet, address) {
		if (address->s6_addr[octet] < 255)
			return 0;
	}

	return 1;
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

static inline unsigned int loc_address_bit_length(const struct in6_addr* address) {
	unsigned int bitlength = 0;
	int trailing_zeroes;

	int octet = 0;

	// Initialize the bit length
	if (IN6_IS_ADDR_V4MAPPED(address))
		bitlength = 32;
	else
		bitlength = 128;

	// Walk backwards until we find the first one
	foreach_octet_in_address_reverse(octet, address) {
		// __builtin_ctz does not support zero as input
		if (!address->s6_addr[octet]) {
			bitlength -= 8;
			continue;
		}

		// Count all trailing zeroes
		trailing_zeroes = __builtin_ctz(address->s6_addr[octet]);

		// We only have one byte
		if (trailing_zeroes > 8)
			trailing_zeroes = 8;

		// Remove any trailing zeroes from the total length
		bitlength -= trailing_zeroes;

		if (trailing_zeroes < 8)
			return bitlength;
	}

	return 0;
}

static inline int loc_address_common_bits(const struct in6_addr* a1, const struct in6_addr* a2) {
	int bits = 0;

	// Both must be of the same family
	if (IN6_IS_ADDR_V4MAPPED(a1) && !IN6_IS_ADDR_V4MAPPED(a2))
		return -EINVAL;

	else if (!IN6_IS_ADDR_V4MAPPED(a1) && IN6_IS_ADDR_V4MAPPED(a2))
		return -EINVAL;

	// Walk through both addresses octet by octet
	for (unsigned int i = (IN6_IS_ADDR_V4MAPPED(a1) ? 12 : 0); i <= 15; i++) {
		// Fast path if the entire octet matches
		if (a1->s6_addr[i] == a2->s6_addr[i]) {
			bits += 8;

		// Otherwise we XOR the octets and count the leading zeroes
		// (where both octets have been identical).
		} else {
			bits += __builtin_clz(a1->s6_addr[i] ^ a2->s6_addr[i]) - 24;
			break;
		}
	}

	return bits;
}

static inline int loc_address_reset(struct in6_addr* address, int family) {
	switch (family) {
		case AF_INET6:
			address->s6_addr32[0] = 0x00000000;
			address->s6_addr32[1] = 0x00000000;
			address->s6_addr32[2] = 0x00000000;
			address->s6_addr32[3] = 0x00000000;
			return 0;

		case AF_INET:
			address->s6_addr32[0] = 0x00000000;
			address->s6_addr32[1] = 0x00000000;
			address->s6_addr32[2] = htonl(0xffff);
			address->s6_addr32[3] = 0x00000000;
			return 0;
	}

	return -1;
}

static inline int loc_address_reset_last(struct in6_addr* address, int family) {
	switch (family) {
		case AF_INET6:
			address->s6_addr32[0] = 0xffffffff;
			address->s6_addr32[1] = 0xffffffff;
			address->s6_addr32[2] = 0xffffffff;
			address->s6_addr32[3] = 0xffffffff;
			return 0;

		case AF_INET:
			address->s6_addr32[0] = 0x00000000;
			address->s6_addr32[1] = 0x00000000;
			address->s6_addr32[2] = htonl(0xffff);
			address->s6_addr32[3] = 0xffffffff;
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

	int octet = 0;
	int remainder = 0;

	foreach_octet_in_address_reverse(octet, address1) {
		int x = address1->s6_addr[octet] - address2->s6_addr[octet] + remainder;

		// Store remainder for the next iteration
		remainder = (x >> 8);

		result->s6_addr[octet] = x & 0xff;
	}

	return 0;
}

static inline void loc_address_increment(struct in6_addr* address) {
	// Prevent overflow when everything is ones
	if (loc_address_all_ones(address))
		return;

	int octet = 0;
	foreach_octet_in_address_reverse(octet, address) {
		if (address->s6_addr[octet] < 255) {
			address->s6_addr[octet]++;
			break;
		} else {
			address->s6_addr[octet] = 0;
		}
	}
}

static inline void loc_address_decrement(struct in6_addr* address) {
	// Prevent underflow when everything is ones
	if (loc_address_all_zeroes(address))
		return;

	int octet = 0;
	foreach_octet_in_address_reverse(octet, address) {
		if (address->s6_addr[octet] > 0) {
			address->s6_addr[octet]--;
			break;
		} else {
			address->s6_addr[octet] = 255;
		}
	}
}

static inline int loc_address_get_octet(const struct in6_addr* address, const unsigned int i) {
	if (IN6_IS_ADDR_V4MAPPED(address)) {
		if (i >= 4)
			return -ERANGE;

		// Format the IPv4 in host-byte order
		unsigned int a4 = be32toh(address->s6_addr32[3]);

		// Return the nth byte from the left
		return a4 >> ((3 - i) * 8) & 0xff;

	} else {
		if (i >= 32)
			return -ERANGE;

		return address->s6_addr[i];
	}
}

static inline int loc_address_get_nibble(const struct in6_addr* address, const unsigned int i) {
	int octet = 0;

	// Fetch the octet
	octet = loc_address_get_octet(address, i / 2);
	if (octet < 0)
		return octet;

	// Shift if we want an uneven nibble
	if (i % 2 == 0)
		octet >>= 4;

	// Return the nibble
	return octet & 0x0f;
}

#endif /* LIBLOC_PRIVATE */

#endif /* LIBLOC_ADDRESS_H */
