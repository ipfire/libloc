/*
	libloc - A library to determine the location of someone on the Internet

	Copyright (C) 2017-2021 IPFire Development Team <info@ipfire.org>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
*/

#ifndef LIBLOC_NETWORK_H
#define LIBLOC_NETWORK_H

#include <netinet/in.h>

#include <libloc/libloc.h>
#include <libloc/format.h>
#include <libloc/network-list.h>

enum loc_network_flags {
	LOC_NETWORK_FLAG_ANONYMOUS_PROXY    = (1 << 0), // A1
	LOC_NETWORK_FLAG_SATELLITE_PROVIDER = (1 << 1), // A2
	LOC_NETWORK_FLAG_ANYCAST            = (1 << 2), // A3
	LOC_NETWORK_FLAG_DROP               = (1 << 3), // XD
};

struct loc_network;
int loc_network_new(struct loc_ctx* ctx, struct loc_network** network,
		struct in6_addr* first_address, unsigned int prefix);
int loc_network_new_from_string(struct loc_ctx* ctx, struct loc_network** network,
		const char* address_string);
struct loc_network* loc_network_ref(struct loc_network* network);
struct loc_network* loc_network_unref(struct loc_network* network);
const char* loc_network_str(struct loc_network* network);
int loc_network_address_family(struct loc_network* network);
unsigned int loc_network_prefix(struct loc_network* network);

const struct in6_addr* loc_network_get_first_address(struct loc_network* network);
const char* loc_network_format_first_address(struct loc_network* network);
const struct in6_addr* loc_network_get_last_address(struct loc_network* network);
const char* loc_network_format_last_address(struct loc_network* network);
int loc_network_matches_address(struct loc_network* network, const struct in6_addr* address);

const char* loc_network_get_country_code(struct loc_network* network);
int loc_network_set_country_code(struct loc_network* network, const char* country_code);
int loc_network_matches_country_code(struct loc_network* network, const char* country_code);

uint32_t loc_network_get_asn(struct loc_network* network);
int loc_network_set_asn(struct loc_network* network, uint32_t asn);

int loc_network_has_flag(struct loc_network* network, uint32_t flag);
int loc_network_set_flag(struct loc_network* network, uint32_t flag);

int loc_network_cmp(struct loc_network* self, struct loc_network* other);
int loc_network_overlaps(struct loc_network* self, struct loc_network* other);
int loc_network_is_subnet(struct loc_network* self, struct loc_network* other);
int loc_network_subnets(struct loc_network* network, struct loc_network** subnet1, struct loc_network** subnet2);
struct loc_network_list* loc_network_exclude(
		struct loc_network* self, struct loc_network* other);
struct loc_network_list* loc_network_exclude_list(
		struct loc_network* network, struct loc_network_list* list);

char* loc_network_reverse_pointer(struct loc_network* network, const char* suffix);

#ifdef LIBLOC_PRIVATE

int loc_network_properties_cmp(struct loc_network* self, struct loc_network* other);
unsigned int loc_network_raw_prefix(struct loc_network* network);

int loc_network_to_database_v1(struct loc_network* network, struct loc_database_network_v1* dbobj);
int loc_network_new_from_database_v1(struct loc_ctx* ctx, struct loc_network** network,
		struct in6_addr* address, unsigned int prefix, const struct loc_database_network_v1* dbobj);

int loc_network_merge(struct loc_network** n, struct loc_network* n1, struct loc_network* n2);

#endif
#endif
