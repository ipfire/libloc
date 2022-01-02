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
char* loc_network_str(struct loc_network* network);
int loc_network_address_family(struct loc_network* network);
unsigned int loc_network_prefix(struct loc_network* network);

const struct in6_addr* loc_network_get_first_address(struct loc_network* network);
char* loc_network_format_first_address(struct loc_network* network);
const struct in6_addr* loc_network_get_last_address(struct loc_network* network);
char* loc_network_format_last_address(struct loc_network* network);
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

#ifdef LIBLOC_PRIVATE

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

static inline int loc_address_family(const struct in6_addr* address) {
	if (IN6_IS_ADDR_V4MAPPED(address))
		return AF_INET;
	else
		return AF_INET6;
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

int loc_network_to_database_v1(struct loc_network* network, struct loc_database_network_v1* dbobj);
int loc_network_new_from_database_v1(struct loc_ctx* ctx, struct loc_network** network,
		struct in6_addr* address, unsigned int prefix, const struct loc_database_network_v1* dbobj);

struct loc_network_tree;
int loc_network_tree_new(struct loc_ctx* ctx, struct loc_network_tree** tree);
struct loc_network_tree* loc_network_tree_unref(struct loc_network_tree* tree);
struct loc_network_tree_node* loc_network_tree_get_root(struct loc_network_tree* tree);
int loc_network_tree_walk(struct loc_network_tree* tree,
		int(*filter_callback)(struct loc_network* network, void* data),
		int(*callback)(struct loc_network* network, void* data), void* data);
int loc_network_tree_dump(struct loc_network_tree* tree);
int loc_network_tree_add_network(struct loc_network_tree* tree, struct loc_network* network);
size_t loc_network_tree_count_networks(struct loc_network_tree* tree);
size_t loc_network_tree_count_nodes(struct loc_network_tree* tree);

struct loc_network_tree_node;
int loc_network_tree_node_new(struct loc_ctx* ctx, struct loc_network_tree_node** node);
struct loc_network_tree_node* loc_network_tree_node_ref(struct loc_network_tree_node* node);
struct loc_network_tree_node* loc_network_tree_node_unref(struct loc_network_tree_node* node);
struct loc_network_tree_node* loc_network_tree_node_get(struct loc_network_tree_node* node, unsigned int index);

int loc_network_tree_node_is_leaf(struct loc_network_tree_node* node);
struct loc_network* loc_network_tree_node_get_network(struct loc_network_tree_node* node);

#endif
#endif