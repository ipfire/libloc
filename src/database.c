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
#include <endian.h>
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <loc/libloc.h>
#include <loc/as.h>
#include <loc/database.h>
#include <loc/format.h>
#include <loc/network.h>
#include <loc/private.h>
#include <loc/stringpool.h>

struct loc_database {
	struct loc_ctx* ctx;
	int refcount;

	unsigned int version;
	time_t created_at;
	off_t vendor;
	off_t description;
	off_t license;

	// ASes in the database
	struct loc_database_as_v0* as_v0;
	size_t as_count;

	// Network tree
	struct loc_database_network_node_v0* network_nodes_v0;
	size_t network_nodes_count;

	// Networks
	struct loc_database_network_v0* networks_v0;
	size_t networks_count;

	struct loc_stringpool* pool;
};

struct loc_database_enumerator {
	struct loc_ctx* ctx;
	struct loc_database* db;
	int refcount;
};

static int loc_database_read_magic(struct loc_database* db, FILE* f) {
	struct loc_database_magic magic;

	// Read from file
	size_t bytes_read = fread(&magic, 1, sizeof(magic), f);

	// Check if we have been able to read enough data
	if (bytes_read < sizeof(magic)) {
		ERROR(db->ctx, "Could not read enough data to validate magic bytes\n");
		DEBUG(db->ctx, "Read %zu bytes, but needed %zu\n", bytes_read, sizeof(magic));
		return -ENOMSG;
	}

	// Compare magic bytes
	if (memcmp(LOC_DATABASE_MAGIC, magic.magic, strlen(LOC_DATABASE_MAGIC)) == 0) {
		DEBUG(db->ctx, "Magic value matches\n");

		// Parse version
		db->version = be16toh(magic.version);
		DEBUG(db->ctx, "Database version is %u\n", db->version);

		return 0;
	}

	ERROR(db->ctx, "Database format is not compatible\n");

	// Return an error
	return 1;
}

static int loc_database_read_as_section_v0(struct loc_database* db,
		FILE* f, const struct loc_database_header_v0* header) {
	off_t as_offset  = be32toh(header->as_offset);
	size_t as_length = be32toh(header->as_length);

	DEBUG(db->ctx, "Reading AS section from %jd (%zu bytes)\n", as_offset, as_length);

	if (as_length > 0) {
		db->as_v0 = mmap(NULL, as_length, PROT_READ,
			MAP_SHARED, fileno(f), as_offset);

		if (db->as_v0 == MAP_FAILED)
			return -errno;
	}

	db->as_count = as_length / sizeof(*db->as_v0);

	INFO(db->ctx, "Read %zu ASes from the database\n", db->as_count);

	return 0;
}

static int loc_database_read_network_nodes_section_v0(struct loc_database* db,
		FILE* f, const struct loc_database_header_v0* header) {
	off_t network_nodes_offset  = be32toh(header->network_tree_offset);
	size_t network_nodes_length = be32toh(header->network_tree_length);

	DEBUG(db->ctx, "Reading network nodes section from %jd (%zu bytes)\n",
		network_nodes_offset, network_nodes_length);

	if (network_nodes_length > 0) {
		db->network_nodes_v0 = mmap(NULL, network_nodes_length, PROT_READ,
			MAP_SHARED, fileno(f), network_nodes_offset);

		if (db->network_nodes_v0 == MAP_FAILED)
			return -errno;
	}

	db->network_nodes_count = network_nodes_length / sizeof(*db->network_nodes_v0);

	INFO(db->ctx, "Read %zu network nodes from the database\n", db->network_nodes_count);

	return 0;
}

static int loc_database_read_networks_section_v0(struct loc_database* db,
		FILE* f, const struct loc_database_header_v0* header) {
	off_t networks_offset  = be32toh(header->network_data_offset);
	size_t networks_length = be32toh(header->network_data_length);

	DEBUG(db->ctx, "Reading networks section from %jd (%zu bytes)\n",
		networks_offset, networks_length);

	if (networks_length > 0) {
		db->networks_v0 = mmap(NULL, networks_length, PROT_READ,
			MAP_SHARED, fileno(f), networks_offset);

		if (db->networks_v0 == MAP_FAILED)
			return -errno;
	}

	db->networks_count = networks_length / sizeof(*db->networks_v0);

	INFO(db->ctx, "Read %zu networks from the database\n", db->networks_count);

	return 0;
}

static int loc_database_read_header_v0(struct loc_database* db, FILE* f) {
	struct loc_database_header_v0 header;

	// Read from file
	size_t size = fread(&header, 1, sizeof(header), f);

	if (size < sizeof(header)) {
		ERROR(db->ctx, "Could not read enough data for header\n");
		return -ENOMSG;
	}

	// Copy over data
	db->created_at  = be64toh(header.created_at);
	db->vendor      = be32toh(header.vendor);
	db->description = be32toh(header.description);
	db->license     = be32toh(header.license);

	// Open pool
	off_t pool_offset  = be32toh(header.pool_offset);
	size_t pool_length = be32toh(header.pool_length);

	int r = loc_stringpool_open(db->ctx, &db->pool,
		f, pool_length, pool_offset);
	if (r)
		return r;

	// AS section
	r = loc_database_read_as_section_v0(db, f, &header);
	if (r)
		return r;

	// Network Nodes
	r = loc_database_read_network_nodes_section_v0(db, f, &header);
	if (r)
		return r;

	// Networks
	r = loc_database_read_networks_section_v0(db, f, &header);
	if (r)
		return r;

	return 0;
}

static int loc_database_read_header(struct loc_database* db, FILE* f) {
	switch (db->version) {
		case 0:
			return loc_database_read_header_v0(db, f);

		default:
			ERROR(db->ctx, "Incompatible database version: %u\n", db->version);
			return 1;
	}
}

static int loc_database_read(struct loc_database* db, FILE* f) {
	clock_t start = clock();

	// Read magic bytes
	int r = loc_database_read_magic(db, f);
	if (r)
		return r;

	// Read the header
	r = loc_database_read_header(db, f);
	if (r)
		return r;

	clock_t end = clock();

	INFO(db->ctx, "Opened database in %.8fs\n",
		(double)(end - start) / CLOCKS_PER_SEC);

	return 0;
}

LOC_EXPORT int loc_database_new(struct loc_ctx* ctx, struct loc_database** database, FILE* f) {
	// Fail on invalid file handle
	if (!f)
		return -EINVAL;

	struct loc_database* db = calloc(1, sizeof(*db));
	if (!db)
		return -ENOMEM;

	// Reference context
	db->ctx = loc_ref(ctx);
	db->refcount = 1;

	DEBUG(db->ctx, "Database object allocated at %p\n", db);

	int r = loc_database_read(db, f);
	if (r) {
		loc_database_unref(db);
		return r;
	}

	*database = db;

	return 0;
}

LOC_EXPORT struct loc_database* loc_database_ref(struct loc_database* db) {
	db->refcount++;

	return db;
}

static void loc_database_free(struct loc_database* db) {
	int r;

	DEBUG(db->ctx, "Releasing database %p\n", db);

	// Removing all ASes
	if (db->as_v0) {
		r = munmap(db->as_v0, db->as_count * sizeof(*db->as_v0));
		if (r)
			ERROR(db->ctx, "Could not unmap AS section: %s\n", strerror(errno));
	}

	// Remove mapped network sections
	if (db->networks_v0) {
		r = munmap(db->networks_v0, db->networks_count * sizeof(*db->networks_v0));
		if (r)
			ERROR(db->ctx, "Could not unmap networks section: %s\n", strerror(errno));
	}

	// Remove mapped network nodes section
	if (db->network_nodes_v0) {
		r = munmap(db->network_nodes_v0, db->network_nodes_count * sizeof(*db->network_nodes_v0));
		if (r)
			ERROR(db->ctx, "Could not unmap network nodes section: %s\n", strerror(errno));
	}

	loc_stringpool_unref(db->pool);

	loc_unref(db->ctx);
	free(db);
}

LOC_EXPORT struct loc_database* loc_database_unref(struct loc_database* db) {
	if (--db->refcount > 0)
		return NULL;

	loc_database_free(db);
	return NULL;
}

LOC_EXPORT time_t loc_database_created_at(struct loc_database* db) {
	return db->created_at;
}

LOC_EXPORT const char* loc_database_get_vendor(struct loc_database* db) {
	return loc_stringpool_get(db->pool, db->vendor);
}

LOC_EXPORT const char* loc_database_get_description(struct loc_database* db) {
	return loc_stringpool_get(db->pool, db->description);
}

LOC_EXPORT const char* loc_database_get_license(struct loc_database* db) {
	return loc_stringpool_get(db->pool, db->license);
}

LOC_EXPORT size_t loc_database_count_as(struct loc_database* db) {
	return db->as_count;
}

// Returns the AS at position pos
static int loc_database_fetch_as(struct loc_database* db, struct loc_as** as, off_t pos) {
	if ((size_t)pos >= db->as_count)
		return -EINVAL;

	DEBUG(db->ctx, "Fetching AS at position %jd\n", pos);

	int r;
	switch (db->version) {
		case 0:
			r = loc_as_new_from_database_v0(db->ctx, db->pool, as, db->as_v0 + pos);
			break;

		default:
			return -1;
	}

	if (r == 0) {
		DEBUG(db->ctx, "Got AS%u\n", loc_as_get_number(*as));
	}

	return r;
}

// Performs a binary search to find the AS in the list
LOC_EXPORT int loc_database_get_as(struct loc_database* db, struct loc_as** as, uint32_t number) {
	off_t lo = 0;
	off_t hi = db->as_count - 1;

	// Save start time
	clock_t start = clock();

	while (lo <= hi) {
		off_t i = (lo + hi) / 2;

		// Fetch AS in the middle between lo and hi
		int r = loc_database_fetch_as(db, as, i);
		if (r)
			return r;

		// Check if this is a match
		uint32_t as_number = loc_as_get_number(*as);
		if (as_number == number) {
			clock_t end = clock();

			// Log how fast this has been
			DEBUG(db->ctx, "Found AS%u in %.8fs\n", as_number,
				(double)(end - start) / CLOCKS_PER_SEC);

			return 0;
		}

		// If it wasn't, we release the AS and
		// adjust our search pointers
		loc_as_unref(*as);

		if (as_number < number) {
			lo = i + 1;
		} else
			hi = i - 1;
	}

	// Nothing found
	*as = NULL;

	return 1;
}

// Returns the network at position pos
static int loc_database_fetch_network(struct loc_database* db, struct loc_network** network, struct in6_addr* address, off_t pos) {
	if ((size_t)pos >= db->networks_count)
		return -EINVAL;

	DEBUG(db->ctx, "Fetching network at position %jd\n", pos);

	int r;
	switch (db->version) {
		case 0:
			r = loc_network_new_from_database_v0(db->ctx, network, address, db->networks_v0 + pos);
			break;

		default:
			return -1;
	}

	if (r == 0) {
		char* string = loc_network_str(*network);
		DEBUG(db->ctx, "Got network %s\n", string);
		free(string);
	}

	return r;
}

static int __loc_database_node_is_leaf(const struct loc_database_network_node_v0* node) {
	return (node->zero == htobe32(0xffffffff));
}

static int __loc_database_lookup_handle_leaf(struct loc_database* db, const struct in6_addr* address,
		struct loc_network** network, struct in6_addr* network_address,
		const struct loc_database_network_node_v0* node) {
	DEBUG(db->ctx, "Handling leaf node at %jd\n", node - db->network_nodes_v0);

	// Fetch the network
	int r = loc_database_fetch_network(db, network,
		network_address, be32toh(node->one));
	if (r)
		return r;

	// Check if the given IP address is inside the network
	r = loc_network_match_address(*network, address);
	if (r) {
		DEBUG(db->ctx, "Searched address is not part of the network\n");

		loc_network_unref(*network);
		*network = NULL;
		return 1;
	}

	// A network was found and the IP address matches
	return 0;
}

// Returns the highest result available
static int __loc_database_lookup_max(struct loc_database* db, const struct in6_addr* address,
		struct loc_network** network, struct in6_addr* network_address,
		const struct loc_database_network_node_v0* node, unsigned int level) {
	// If the node is a leaf node, we end here
	if (__loc_database_node_is_leaf(node))
		return __loc_database_lookup_handle_leaf(db, address, network, network_address, node);

	int r;
	off_t node_index;

	// Try to go down the ones path first
	if (node->one) {
		node_index = be32toh(node->one);
		in6_addr_set_bit(network_address, level, 1);

		// Check boundaries
		if (node_index > 0 && (size_t)node_index <= db->network_nodes_count) {
			r = __loc_database_lookup_max(db, address, network, network_address,
				db->network_nodes_v0 + node_index, level + 1);

			// Abort when match was found or error
			if (r <= 0)
				return r;
		}
	}

	// ... and if that fails, we try to go down one step on a zero
	// branch and then try the ones again...
	if (node->zero) {
		node_index = be32toh(node->zero);
		in6_addr_set_bit(network_address, level, 0);

		// Check boundaries
		if (node_index > 0 && (size_t)node_index <= db->network_nodes_count) {
			r = __loc_database_lookup_max(db, address, network, network_address,
				db->network_nodes_v0 + node_index, level + 1);

			// Abort when match was found or error
			if (r <= 0)
				return r;
		}
	}

	// End of path
	return 1;
}

// Searches for an exact match along the path
static int __loc_database_lookup(struct loc_database* db, const struct in6_addr* address,
		struct loc_network** network, struct in6_addr* network_address,
		const struct loc_database_network_node_v0* node, unsigned int level) {
	// If the node is a leaf node, we end here
	if (__loc_database_node_is_leaf(node))
		return __loc_database_lookup_handle_leaf(db, address, network, network_address, node);

	int r;
	off_t node_index;

	// Follow the path
	int bit = in6_addr_get_bit(address, level);
	in6_addr_set_bit(network_address, level, bit);

	if (bit == 0)
		node_index = be32toh(node->zero);
	else
		node_index = be32toh(node->one);

	// If we point back to root, the path ends here
	if (node_index == 0) {
		DEBUG(db->ctx, "Tree ends here\n");
		return 1;
	}

	// Check boundaries
	if ((size_t)node_index >= db->network_nodes_count)
		return -EINVAL;

	// Move on to the next node
	r = __loc_database_lookup(db, address, network, network_address,
		db->network_nodes_v0 + node_index, level + 1);

	// End here if a result was found
	if (r == 0)
		return r;

	// Raise any errors
	else if (r < 0)
		return r;

	DEBUG(db->ctx, "Could not find an exact match at %u\n", level);

	// If nothing was found, we have to search for an inexact match
	return __loc_database_lookup_max(db, address, network, network_address, node, level);
}

LOC_EXPORT int loc_database_lookup(struct loc_database* db,
		struct in6_addr* address, struct loc_network** network) {
	struct in6_addr network_address;
	memset(&network_address, 0, sizeof(network_address));

	*network = NULL;

	// Save start time
	clock_t start = clock();

	int r = __loc_database_lookup(db, address, network, &network_address,
		db->network_nodes_v0, 0);

	clock_t end = clock();

	// Log how fast this has been
	DEBUG(db->ctx, "Executed network search in %.8fs\n",
		(double)(end - start) / CLOCKS_PER_SEC);

	return r;
}

LOC_EXPORT int loc_database_lookup_from_string(struct loc_database* db,
		const char* string, struct loc_network** network) {
	struct in6_addr address;

	int r = loc_parse_address(db->ctx, string, &address);
	if (r)
		return r;

	return loc_database_lookup(db, &address, network);
}

// Enumerator

LOC_EXPORT int loc_database_enumerator_new(struct loc_database_enumerator** enumerator, struct loc_database* db) {
	struct loc_database_enumerator* e = calloc(1, sizeof(*e));
	if (!e)
		return -ENOMEM;

	// Reference context
	e->ctx = loc_ref(db->ctx);
	e->db = loc_database_ref(db);
	e->refcount = 1;

	DEBUG(e->ctx, "Database enumerator object allocated at %p\n", e);

	*enumerator = e;
	return 0;
}

LOC_EXPORT struct loc_database_enumerator* loc_database_enumerator_ref(struct loc_database_enumerator* enumerator) {
	enumerator->refcount++;

	return enumerator;
}

static void loc_database_enumerator_free(struct loc_database_enumerator* enumerator) {
	DEBUG(enumerator->ctx, "Releasing database enumerator %p\n", enumerator);

	// Release all references
	loc_database_unref(enumerator->db);
	loc_unref(enumerator->ctx);

	free(enumerator);
}

LOC_EXPORT struct loc_database_enumerator* loc_database_enumerator_unref(struct loc_database_enumerator* enumerator) {
	if (!enumerator)
		return NULL;

	if (--enumerator->refcount > 0)
		return enumerator;

	loc_database_enumerator_free(enumerator);
	return NULL;
}
