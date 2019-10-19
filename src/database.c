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
#include <ctype.h>
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

#ifdef HAVE_ENDIAN_H
#  include <endian.h>
#endif

#include <loc/libloc.h>
#include <loc/as.h>
#include <loc/compat.h>
#include <loc/country.h>
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

	// Countries
	struct loc_database_country_v0* countries_v0;
	size_t countries_count;

	struct loc_stringpool* pool;
};

#define MAX_STACK_DEPTH 256

struct loc_node_stack {
	off_t offset;
	int i; // Is this node 0 or 1?
	int depth;
};

struct loc_database_enumerator {
	struct loc_ctx* ctx;
	struct loc_database* db;
	enum loc_database_enumerator_mode mode;
	int refcount;

	// Search string
	char* string;
	char country_code[3];
	uint32_t asn;
	enum loc_network_flags flags;

	// Index of the AS we are looking at
	unsigned int as_index;

	// Network state
	struct in6_addr network_address;
	struct loc_node_stack network_stack[MAX_STACK_DEPTH];
	int network_stack_depth;
	unsigned int* networks_visited;
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

	DEBUG(db->ctx, "Reading AS section from %jd (%zu bytes)\n", (intmax_t)as_offset, as_length);

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
		(intmax_t)network_nodes_offset, network_nodes_length);

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
		(intmax_t)networks_offset, networks_length);

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

static int loc_database_read_countries_section_v0(struct loc_database* db,
		FILE* f, const struct loc_database_header_v0* header) {
	off_t countries_offset  = be32toh(header->countries_offset);
	size_t countries_length = be32toh(header->countries_length);

	DEBUG(db->ctx, "Reading countries section from %jd (%zu bytes)\n",
		(intmax_t)countries_offset, countries_length);

	if (countries_length > 0) {
		db->countries_v0 = mmap(NULL, countries_length, PROT_READ,
			MAP_SHARED, fileno(f), countries_offset);

		if (db->countries_v0 == MAP_FAILED)
			return -errno;
	}

	db->countries_count = countries_length / sizeof(*db->countries_v0);

	INFO(db->ctx, "Read %zu countries from the database\n",
		db->countries_count);

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

	// countries
	r = loc_database_read_countries_section_v0(db, f, &header);
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

	INFO(db->ctx, "Opened database in %.4fms\n",
		(double)(end - start) / CLOCKS_PER_SEC * 1000);

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

	DEBUG(db->ctx, "Fetching AS at position %jd\n", (intmax_t)pos);

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
			DEBUG(db->ctx, "Found AS%u in %.4fms\n", as_number,
				(double)(end - start) / CLOCKS_PER_SEC * 1000);

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
static int loc_database_fetch_network(struct loc_database* db, struct loc_network** network,
		struct in6_addr* address, unsigned int prefix, off_t pos) {
	if ((size_t)pos >= db->networks_count)
		return -EINVAL;

	DEBUG(db->ctx, "Fetching network at position %jd\n", (intmax_t)pos);

	int r;
	switch (db->version) {
		case 0:
			r = loc_network_new_from_database_v0(db->ctx, network,
				address, prefix, db->networks_v0 + pos);
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
	return (node->network != htobe32(0xffffffff));
}

static int __loc_database_lookup_handle_leaf(struct loc_database* db, const struct in6_addr* address,
		struct loc_network** network, struct in6_addr* network_address, unsigned int prefix,
		const struct loc_database_network_node_v0* node) {
	off_t network_index = be32toh(node->network);

	DEBUG(db->ctx, "Handling leaf node at %jd (%jd)\n", (intmax_t)(node - db->network_nodes_v0), (intmax_t)network_index);

	// Fetch the network
	int r = loc_database_fetch_network(db, network,
		network_address, prefix, network_index);
	if (r) {
		ERROR(db->ctx, "Could not fetch network %jd from database\n", (intmax_t)network_index);
		return r;
	}

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

// Searches for an exact match along the path
static int __loc_database_lookup(struct loc_database* db, const struct in6_addr* address,
		struct loc_network** network, struct in6_addr* network_address,
		const struct loc_database_network_node_v0* node, unsigned int level) {
	int r;
	off_t node_index;

	// Follow the path
	int bit = in6_addr_get_bit(address, level);
	in6_addr_set_bit(network_address, level, bit);

	if (bit == 0)
		node_index = be32toh(node->zero);
	else
		node_index = be32toh(node->one);

	// If the node index is zero, the tree ends here
	// and we cannot descend any further
	if (node_index > 0) {
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

		DEBUG(db->ctx, "No match found below level %u\n", level);
	} else {
		DEBUG(db->ctx, "Tree ended at level %u\n", level);
	}

	// If this node has a leaf, we will check if it matches
	if (__loc_database_node_is_leaf(node)) {
		r = __loc_database_lookup_handle_leaf(db, address, network, network_address, level, node);
		if (r <= 0)
			return r;
	}

	return 1;
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
	DEBUG(db->ctx, "Executed network search in %.4fms\n",
		(double)(end - start) / CLOCKS_PER_SEC * 1000);

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

// Returns the country at position pos
static int loc_database_fetch_country(struct loc_database* db,
		struct loc_country** country, off_t pos) {
	if ((size_t)pos >= db->countries_count)
		return -EINVAL;

	DEBUG(db->ctx, "Fetching country at position %jd\n", (intmax_t)pos);

	int r;
	switch (db->version) {
		case 0:
			r = loc_country_new_from_database_v0(db->ctx, db->pool, country, db->countries_v0 + pos);
			break;

		default:
			return -1;
	}

	if (r == 0) {
		DEBUG(db->ctx, "Got country %s\n", loc_country_get_code(*country));
	}

	return r;
}

// Performs a binary search to find the country in the list
LOC_EXPORT int loc_database_get_country(struct loc_database* db,
		struct loc_country** country, const char* code) {
	off_t lo = 0;
	off_t hi = db->countries_count - 1;

	// Save start time
	clock_t start = clock();

	while (lo <= hi) {
		off_t i = (lo + hi) / 2;

		// Fetch country in the middle between lo and hi
		int r = loc_database_fetch_country(db, country, i);
		if (r)
			return r;

		// Check if this is a match
		const char* cc = loc_country_get_code(*country);
		int result = strcmp(code, cc);

		if (result == 0) {
			clock_t end = clock();

			// Log how fast this has been
			DEBUG(db->ctx, "Found country %s in %.4fms\n", cc,
				(double)(end - start) / CLOCKS_PER_SEC * 1000);

			return 0;
		}

		// If it wasn't, we release the country and
		// adjust our search pointers
		loc_country_unref(*country);

		if (result > 0) {
			lo = i + 1;
		} else
			hi = i - 1;
	}

	// Nothing found
	*country = NULL;

	return 1;
}

// Enumerator

LOC_EXPORT int loc_database_enumerator_new(struct loc_database_enumerator** enumerator,
		struct loc_database* db, enum loc_database_enumerator_mode mode) {
	struct loc_database_enumerator* e = calloc(1, sizeof(*e));
	if (!e)
		return -ENOMEM;

	// Reference context
	e->ctx = loc_ref(db->ctx);
	e->db = loc_database_ref(db);
	e->mode = mode;
	e->refcount = 1;

	// Initialise graph search
	//e->network_stack[++e->network_stack_depth] = 0;
	e->network_stack_depth = 1;
	e->networks_visited = calloc(db->network_nodes_count, sizeof(*e->networks_visited));

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

	if (enumerator->string)
		free(enumerator->string);

	// Free network search
	free(enumerator->networks_visited);

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

LOC_EXPORT int loc_database_enumerator_set_string(struct loc_database_enumerator* enumerator, const char* string) {
	enumerator->string = strdup(string);

	// Make the string lowercase
	for (char *p = enumerator->string; *p; p++)
		*p = tolower(*p);

	return 0;
}

LOC_EXPORT int loc_database_enumerator_set_country_code(struct loc_database_enumerator* enumerator, const char* country_code) {
	// Set empty country code
	if (!country_code || !*country_code) {
		*enumerator->country_code = '\0';
		return 0;
	}

	// Treat A1, A2, A3 as special country codes,
	// but perform search for flags instead
	if (strcmp(country_code, "A1") == 0) {
		return loc_database_enumerator_set_flag(enumerator,
			LOC_NETWORK_FLAG_ANONYMOUS_PROXY);
	} else if (strcmp(country_code, "A2") == 0) {
		return loc_database_enumerator_set_flag(enumerator,
			LOC_NETWORK_FLAG_SATELLITE_PROVIDER);
	} else if (strcmp(country_code, "A3") == 0) {
		return loc_database_enumerator_set_flag(enumerator,
			LOC_NETWORK_FLAG_ANYCAST);
	}

	// Country codes must be two characters
	if (!loc_country_code_is_valid(country_code))
		return -EINVAL;

	for (unsigned int i = 0; i < 3; i++) {
		enumerator->country_code[i] = country_code[i];
	}

	return 0;
}

LOC_EXPORT int loc_database_enumerator_set_asn(
		struct loc_database_enumerator* enumerator, unsigned int asn) {
	enumerator->asn = asn;

	return 0;
}

LOC_EXPORT int loc_database_enumerator_set_flag(
		struct loc_database_enumerator* enumerator, enum loc_network_flags flag) {
	enumerator->flags |= flag;

	return 0;
}

LOC_EXPORT int loc_database_enumerator_next_as(
		struct loc_database_enumerator* enumerator, struct loc_as** as) {
	*as = NULL;

	// Do not do anything if not in AS mode
	if (enumerator->mode != LOC_DB_ENUMERATE_ASES)
		return 0;

	struct loc_database* db = enumerator->db;

	while (enumerator->as_index < db->as_count) {
		// Fetch the next AS
		int r = loc_database_fetch_as(db, as, enumerator->as_index++);
		if (r)
			return r;

		r = loc_as_match_string(*as, enumerator->string);
		if (r == 1) {
			DEBUG(enumerator->ctx, "AS%d (%s) matches %s\n",
				loc_as_get_number(*as), loc_as_get_name(*as), enumerator->string);

			return 0;
		}

		// No match
		loc_as_unref(*as);
		*as = NULL;
	}

	// Reset the index
	enumerator->as_index = 0;

	// We have searched through all of them
	return 0;
}

static int loc_database_enumerator_stack_push_node(
		struct loc_database_enumerator* e, off_t offset, int i, int depth) {
	// Do not add empty nodes
	if (!offset)
		return 0;

	// Check if there is any space left on the stack
	if (e->network_stack_depth >= MAX_STACK_DEPTH) {
		ERROR(e->ctx, "Maximum stack size reached: %d\n", e->network_stack_depth);
		return -1;
	}

	// Increase stack size
	int s = ++e->network_stack_depth;

	DEBUG(e->ctx, "Added node %jd to stack (%d)\n", (intmax_t)offset, depth);

	e->network_stack[s].offset = offset;
	e->network_stack[s].i = i;
	e->network_stack[s].depth = depth;

	return 0;
}

LOC_EXPORT int loc_database_enumerator_next_network(
		struct loc_database_enumerator* enumerator, struct loc_network** network) {
	// Reset network
	*network = NULL;

	// Do not do anything if not in network mode
	if (enumerator->mode != LOC_DB_ENUMERATE_NETWORKS)
		return 0;

	int r;

	DEBUG(enumerator->ctx, "Called with a stack of %u nodes\n",
		enumerator->network_stack_depth);

	// Perform DFS
	while (enumerator->network_stack_depth > 0) {
		DEBUG(enumerator->ctx, "Stack depth: %u\n", enumerator->network_stack_depth);

		// Get object from top of the stack
		struct loc_node_stack* node = &enumerator->network_stack[enumerator->network_stack_depth];

		// Remove the node from the stack if we have already visited it
		if (enumerator->networks_visited[node->offset]) {
			enumerator->network_stack_depth--;
			continue;
		}

		// Mark the bits on the path correctly
		in6_addr_set_bit(&enumerator->network_address,
			(node->depth > 0) ? node->depth - 1 : 0, node->i);

		DEBUG(enumerator->ctx, "Looking at node %jd\n", (intmax_t)node->offset);
		enumerator->networks_visited[node->offset]++;

		// Pop node from top of the stack
		struct loc_database_network_node_v0* n =
			enumerator->db->network_nodes_v0 + node->offset;

		// Add edges to stack
		r = loc_database_enumerator_stack_push_node(enumerator,
			be32toh(n->one), 1, node->depth + 1);

		if (r)
			return r;

		r = loc_database_enumerator_stack_push_node(enumerator,
			be32toh(n->zero), 0, node->depth + 1);

		if (r)
			return r;

		// Check if this node is a leaf and has a network object
		if (__loc_database_node_is_leaf(n)) {
			off_t network_index = be32toh(n->network);

			DEBUG(enumerator->ctx, "Node has a network at %jd\n", (intmax_t)network_index);

			// Fetch the network object
			r = loc_database_fetch_network(enumerator->db, network,
				&enumerator->network_address, node->depth, network_index);

			// Break on any errors
			if (r)
				return r;

			// Check if we are interested in this network

			// Skip if the country code does not match
			if (*enumerator->country_code &&
					!loc_network_match_country_code(*network, enumerator->country_code)) {
				loc_network_unref(*network);
				*network = NULL;

				continue;
			}

			// Skip if the ASN does not match
			if (enumerator->asn &&
					!loc_network_match_asn(*network, enumerator->asn)) {
				loc_network_unref(*network);
				*network = NULL;

				continue;
			}

			// Skip if flags do not match
			if (enumerator->flags &&
					!loc_network_match_flag(*network, enumerator->flags)) {
				loc_network_unref(*network);
				*network = NULL;
			}

			return 0;
		}
	}

	// Reached the end of the search

	// Mark all nodes as non-visited
	for (unsigned int i = 0; i < enumerator->db->network_nodes_count; i++)
		enumerator->networks_visited[i] = 0;

	return 0;
}
