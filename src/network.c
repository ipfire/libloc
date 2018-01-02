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

#include <arpa/inet.h>
#include <assert.h>
#include <endian.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <loc/libloc.h>
#include <loc/network.h>
#include <loc/private.h>

struct loc_network {
	struct loc_ctx* ctx;
	int refcount;

	struct in6_addr start_address;
	unsigned int prefix;

	char country_code[3];
	uint32_t asn;
};

static int valid_prefix(struct in6_addr* address, unsigned int prefix) {
	// The prefix cannot be larger than 128 bits
	if (prefix > 128)
		return 1;

	// And the prefix cannot be zero
	if (prefix == 0)
		return 1;

	return 0;
}

LOC_EXPORT int loc_network_new(struct loc_ctx* ctx, struct loc_network** network,
		struct in6_addr start_address, unsigned int prefix) {
	// Address cannot be unspecified
	if (IN6_IS_ADDR_UNSPECIFIED(&start_address)) {
		DEBUG(ctx, "Start address is unspecified\n");
		return -EINVAL;
	}

	// Address cannot be loopback
	if (IN6_IS_ADDR_LOOPBACK(&start_address)) {
		DEBUG(ctx, "Start address is loopback address\n");
		return -EINVAL;
	}

	// Address cannot be link-local
	if (IN6_IS_ADDR_LINKLOCAL(&start_address)) {
		DEBUG(ctx, "Start address cannot be link-local\n");
		return -EINVAL;
	}

	// Address cannot be site-local
	if (IN6_IS_ADDR_SITELOCAL(&start_address)) {
		DEBUG(ctx, "Start address cannot be site-local\n");
		return -EINVAL;
	}

	// Validate the prefix
	if (valid_prefix(&start_address, prefix) != 0) {
		DEBUG(ctx, "Invalid prefix: %u\n", prefix);
		return -EINVAL;
	}

	struct loc_network* n = calloc(1, sizeof(*n));
	if (!n)
		return -ENOMEM;

	n->ctx = loc_ref(ctx);
	n->refcount = 1;

	n->start_address = start_address;
	n->prefix = prefix;

	DEBUG(n->ctx, "Network allocated at %p\n", n);
	*network = n;
	return 0;
}

static int loc_network_address_family(struct loc_network* network) {
	if (IN6_IS_ADDR_V4MAPPED(&network->start_address))
		return AF_INET;

	return AF_INET6;
}

static int parse_address(struct loc_ctx* ctx, const char* string, struct in6_addr* address) {
	DEBUG(ctx, "Paring IP address %s\n", string);

	// Try parsing this as an IPv6 address
	int r = inet_pton(AF_INET6, string, address);

	// If inet_pton returns one it has been successful
	if (r == 1) {
		DEBUG(ctx, "%s is an IPv6 address\n", string);
		return 0;
	}

	// Try parsing this as an IPv4 address
	struct in_addr ipv4_address;
	r = inet_pton(AF_INET, string, &ipv4_address);
	if (r == 1) {
		DEBUG(ctx, "%s is an IPv4 address\n", string);

		// Convert to IPv6-mapped address
		address->s6_addr32[0] = htonl(0x0000);
		address->s6_addr32[1] = htonl(0x0000);
		address->s6_addr32[2] = htonl(0xffff);
		address->s6_addr32[3] = ipv4_address.s_addr;

		return 0;
	}

	DEBUG(ctx, "%s is not an valid IP address\n", string);
	return 1;
}

LOC_EXPORT int loc_network_new_from_string(struct loc_ctx* ctx, struct loc_network** network,
		const char* address_string) {
	struct in6_addr start_address;
	unsigned int prefix = 0;
	char* prefix_string;
	int r = 1;

	// Make a copy of the string to work on it
	char* buffer = strdup(address_string);
	address_string = prefix_string = buffer;

	// Split address and prefix
	address_string = strsep(&prefix_string, "/");

	// Did we find a prefix?
	if (prefix_string) {
		// Convert prefix to integer
		prefix = strtol(prefix_string, NULL, 10);

		if (prefix) {
			// Parse the address
			r = parse_address(ctx, address_string, &start_address);
		}
	}

	// Free temporary buffer
	free(buffer);

	if (r == 0) {
		r = loc_network_new(ctx, network, start_address, prefix);
	}

	return r;
}

LOC_EXPORT struct loc_network* loc_network_ref(struct loc_network* network) {
	network->refcount++;

	return network;
}

static void loc_network_free(struct loc_network* network) {
	DEBUG(network->ctx, "Releasing network at %p\n", network);

	loc_unref(network->ctx);
	free(network);
}

LOC_EXPORT struct loc_network* loc_network_unref(struct loc_network* network) {
	if (--network->refcount > 0)
		return network;

	loc_network_free(network);
	return NULL;
}

static int format_ipv6_address(struct loc_network* network, char* string, size_t length) {
	const char* ret = inet_ntop(AF_INET6, &network->start_address, string, length);
	if (!ret)
		return -1;

	return 0;
}

static int format_ipv4_address(struct loc_network* network, char* string, size_t length) {
	struct in_addr ipv4_address;
	ipv4_address.s_addr = network->start_address.s6_addr32[3];

	const char* ret = inet_ntop(AF_INET, &ipv4_address, string, length);
	if (!ret)
		return -1;

	return 0;
}

LOC_EXPORT char* loc_network_str(struct loc_network* network) {
	int r;
	const size_t length = INET6_ADDRSTRLEN + 4;

	char* string = malloc(length);
	if (!string)
		return NULL;

	int family = loc_network_address_family(network);
	switch (family) {
		case AF_INET6:
			r = format_ipv6_address(network, string, length);
			break;

		case AF_INET:
			r = format_ipv4_address(network, string, length);
			break;

		default:
			r = -1;
			break;
	}

	if (r) {
		ERROR(network->ctx, "Could not convert network to string: %s\n", strerror(errno));
		free(string);

		return NULL;
	}

	// Append prefix
	sprintf(string + strlen(string), "/%u", network->prefix);

	return string;
}

LOC_EXPORT const char* loc_network_get_country_code(struct loc_network* network) {
	return network->country_code;
}

LOC_EXPORT int loc_network_set_country_code(struct loc_network* network, const char* country_code) {
	// Country codes must be two characters
	if (strlen(country_code) != 2)
		return -EINVAL;

	for (unsigned int i = 0; i < 3; i++) {
		network->country_code[i] = country_code[i];
	}

	return 0;
}

LOC_EXPORT uint32_t loc_network_get_asn(struct loc_network* network) {
	return network->asn;
}

LOC_EXPORT int loc_network_set_asn(struct loc_network* network, uint32_t asn) {
	network->asn = asn;

	return 0;
}

LOC_EXPORT int loc_network_to_database_v0(struct loc_network* network, struct loc_database_network_v0* dbobj) {
	dbobj->prefix = htobe16(network->prefix);

	// Add country code
	for (unsigned int i = 0; i < 2; i++) {
		dbobj->country_code[i] = network->country_code ? network->country_code[i] : '\0';
	}

	// Add ASN
	dbobj->asn = htobe32(network->asn);

	return 0;
}

struct loc_network_tree {
	struct loc_ctx* ctx;
	int refcount;

	struct loc_network_tree_node* root;
};

struct loc_network_tree_node {
	struct loc_network_tree_node* zero;
	struct loc_network_tree_node* one;

	struct loc_network* network;
};

LOC_EXPORT int loc_network_tree_new(struct loc_ctx* ctx, struct loc_network_tree** tree) {
	struct loc_network_tree* t = calloc(1, sizeof(*t));
	if (!t)
		return -ENOMEM;

	t->ctx = loc_ref(ctx);
	t->refcount = 1;

	// Create the root node
	t->root = calloc(1, sizeof(*t->root));

	DEBUG(t->ctx, "Network tree allocated at %p\n", t);
	*tree = t;
	return 0;
}

static int loc_network_tree_node_new(struct loc_network_tree_node** node) {
	struct loc_network_tree_node* n = calloc(1, sizeof(*n));
	if (!n)
		return -ENOMEM;

	n->zero = n->one = NULL;

	*node = n;
	return 0;
}

static struct loc_network_tree_node* loc_network_tree_get_node(struct loc_network_tree_node* node, int path) {
	struct loc_network_tree_node** n;

	if (path)
		n = &node->one;
	else
		n = &node->zero;

	// If the desired node doesn't exist, yet, we will create it
	if (*n == NULL) {
		int r = loc_network_tree_node_new(n);
		if (r)
			return NULL;
	}

	return *n;
}

static struct loc_network_tree_node* loc_network_tree_get_path(struct loc_network_tree* tree, const struct in6_addr* address) {
	struct loc_network_tree_node* node = tree->root;

	for (unsigned int i = 127; i > 0; i--) {
		// Check if the ith bit is one or zero
		node = loc_network_tree_get_node(node, ((address->s6_addr32[i / 32] & (1 << (i % 32))) == 0));
	}

	return node;
}

static int __loc_network_tree_walk(struct loc_ctx* ctx, struct loc_network_tree_node* node,
		int(*filter_callback)(struct loc_network* network, void* data),
		int(*callback)(struct loc_network* network, void* data), void* data) {
	int r;

	// Finding a network ends the walk here
	if (node->network) {
		if (filter_callback) {
			int f = filter_callback(node->network, data);
			if (f < 0)
				return f;

			// Skip network if filter function returns value greater than zero
			if (f > 0)
				return 0;
		}

		r = callback(node->network, data);
		if (r)
			return r;
	}

	// Walk down on the left side of the tree first
	if (node->zero) {
		r = __loc_network_tree_walk(ctx, node->zero, filter_callback, callback, data);
		if (r)
			return r;
	}

	// Then walk on the other side
	if (node->one) {
		r = __loc_network_tree_walk(ctx, node->one, filter_callback, callback, data);
		if (r)
			return r;
	}

	return 0;
}

LOC_EXPORT int loc_network_tree_walk(struct loc_network_tree* tree,
		int(*filter_callback)(struct loc_network* network, void* data),
		int(*callback)(struct loc_network* network, void* data), void* data) {
	return __loc_network_tree_walk(tree->ctx, tree->root, filter_callback, callback, data);
}

static void loc_network_tree_free_subtree(struct loc_network_tree_node* node) {
	if (node->network)
		loc_network_unref(node->network);

	if (node->zero)
		loc_network_tree_free_subtree(node->zero);

	if (node->one)
		loc_network_tree_free_subtree(node->one);

	free(node);
}

static void loc_network_tree_free(struct loc_network_tree* tree) {
	DEBUG(tree->ctx, "Releasing network tree at %p\n", tree);

	loc_network_tree_free_subtree(tree->root);

	loc_unref(tree->ctx);
	free(tree);
}

LOC_EXPORT struct loc_network_tree* loc_network_tree_unref(struct loc_network_tree* tree) {
	if (--tree->refcount > 0)
		return tree;

	loc_network_tree_free(tree);
	return NULL;
}

int __loc_network_tree_dump(struct loc_network* network, void* data) {
	DEBUG(network->ctx, "Dumping network at %p\n", network);

	char* s = loc_network_str(network);
	if (!s)
		return 1;

	INFO(network->ctx, "%s\n", s);
	free(s);

	return 0;
}

LOC_EXPORT int loc_network_tree_dump(struct loc_network_tree* tree) {
	DEBUG(tree->ctx, "Dumping network tree at %p\n", tree);

	return loc_network_tree_walk(tree, NULL, __loc_network_tree_dump, NULL);
}

LOC_EXPORT int loc_network_tree_add_network(struct loc_network_tree* tree, struct loc_network* network) {
	DEBUG(tree->ctx, "Adding network %p to tree %p\n", network, tree);

	struct loc_network_tree_node* node = loc_network_tree_get_path(tree, &network->start_address);
	if (!node) {
		ERROR(tree->ctx, "Could not find a node\n");
		return -ENOMEM;
	}

	// Check if node has not been set before
	if (node->network) {
		DEBUG(tree->ctx, "There is already a network at this path\n");
		return 1;
	}

	// Point node to the network
	node->network = loc_network_ref(network);

	return 0;
}

static int __loc_network_tree_count(struct loc_network* network, void* data) {
	size_t* counter = (size_t*)data;

	// Increase the counter for each network
	counter++;

	return 0;
}

LOC_EXPORT size_t loc_network_tree_count_networks(struct loc_network_tree* tree) {
	size_t counter = 0;

	int r = loc_network_tree_walk(tree, NULL, __loc_network_tree_count, &counter);
	if (r)
		return r;

	return counter;
}
