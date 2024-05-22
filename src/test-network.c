/*
	libloc - A library to determine the location of someone on the Internet

	Copyright (C) 2017 IPFire Development Team <info@ipfire.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
*/

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <libloc/libloc.h>
#include <libloc/address.h>
#include <libloc/database.h>
#include <libloc/network.h>
#include <libloc/private.h>
#include <libloc/writer.h>

static int test_reverse_pointers(struct loc_ctx* ctx) {
	const struct test {
		const char* network;
		const char* rp;
	} tests[] = {
		// IPv6
		{ "::1/128", "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa." },
		{ "2001:db8::/32", "*.8.b.d.0.1.0.0.2.ip6.arpa." },

		// IPv4
		{ "10.0.0.0/32", "0.0.0.10.in-addr.arpa." },
		{ "10.0.0.0/24", "*.0.0.10.in-addr.arpa." },
		{ "10.0.0.0/16", "*.0.10.in-addr.arpa." },
		{ "10.0.0.0/8", "*.10.in-addr.arpa." },
		{ "10.0.0.0/0", "*.in-addr.arpa." },
		{ "10.0.0.0/1", NULL, },
		{ NULL, NULL },
	};

	struct loc_network* network = NULL;
	char* rp = NULL;
	int r;

	for (const struct test* test = tests; test->network; test++) {
		// Create a new network
		r = loc_network_new_from_string(ctx, &network, test->network);
		if (r)
			return r;

		// Fetch the reverse pointer
		rp = loc_network_reverse_pointer(network, NULL);

		// No RP expected and got none
		if (!test->rp && !rp)
			continue;

		// Got a result when expecting none
		else if (!test->rp && rp) {
			fprintf(stderr, "Got an RP for %s when expecting none\n", test->network);
			return EXIT_FAILURE;

		// Got nothing when expecting a result
		} else if (test->rp && !rp) {
			fprintf(stderr, "Got no RP for %s when expecting one\n", test->network);
			return EXIT_FAILURE;

		// Compare values
		} else if (strcmp(test->rp, rp) != 0) {
			fprintf(stderr, "Got an unexpected RP for %s: Got %s, expected %s\n",
				test->network, rp, test->rp);
			return EXIT_FAILURE;
		}

		loc_network_unref(network);
		if (rp)
			free(rp);
	}

	return 0;
}

int main(int argc, char** argv) {
	int err;

	struct loc_ctx* ctx;
	err = loc_new(&ctx);
	if (err < 0)
		exit(EXIT_FAILURE);

	// Enable debug logging
	loc_set_log_priority(ctx, LOG_DEBUG);

#if 0
	struct loc_network_tree* tree;
	err = loc_network_tree_new(ctx, &tree);
	if (err) {
		fprintf(stderr, "Could not create the network tree\n");
		exit(EXIT_FAILURE);
	}
#endif

	struct in6_addr address;
	err = inet_pton(AF_INET6, "2001:db8::1", &address);
	if (err != 1) {
		fprintf(stderr, "Could not parse IP address\n");
		exit(EXIT_FAILURE);
	}

	// Create a network
	struct loc_network* network1;
	err = loc_network_new_from_string(ctx, &network1, "2001:db8::1/32");
	if (err) {
		fprintf(stderr, "Could not create the network\n");
		exit(EXIT_FAILURE);
	}

	err = loc_network_set_country_code(network1, "DE");
	if (err) {
		fprintf(stderr, "Could not set country code\n");
		exit(EXIT_FAILURE);
	}

#if 0
	// Adding network to the tree
	err = loc_network_tree_add_network(tree, network1);
	if (err) {
		fprintf(stderr, "Could not add network to the tree\n");
		exit(EXIT_FAILURE);
	}
#endif

	// Check if the first and last addresses are correct
	const char* string = loc_network_format_first_address(network1);
	if (!string) {
		fprintf(stderr, "Did get NULL instead of a string for the first address\n");
		exit(EXIT_FAILURE);
	}

	if (strcmp(string, "2001:db8::") != 0) {
		fprintf(stderr, "Got an incorrect first address: %s\n", string);
		exit(EXIT_FAILURE);
	}

	string = loc_network_format_last_address(network1);
	if (!string) {
		fprintf(stderr, "Did get NULL instead of a string for the last address\n");
		exit(EXIT_FAILURE);
	}

	if (strcmp(string, "2001:db8:ffff:ffff:ffff:ffff:ffff:ffff") != 0) {
		fprintf(stderr, "Got an incorrect last address: %s\n", string);
		exit(EXIT_FAILURE);
	}

	err = loc_network_matches_address(network1, &address);
	if (!err) {
		fprintf(stderr, "Network1 does not match address\n");
		exit(EXIT_FAILURE);
	}

	struct loc_network* network2;
	err = loc_network_new_from_string(ctx, &network2, "2001:db8:ffff::/48");
	if (err) {
		fprintf(stderr, "Could not create the network\n");
		exit(EXIT_FAILURE);
	}

	err = loc_network_set_country_code(network2, "DE");
	if (err) {
		fprintf(stderr, "Could not set country code\n");
		exit(EXIT_FAILURE);
	}

#if 0
	// Adding network to the tree
	err = loc_network_tree_add_network(tree, network2);
	if (err) {
		fprintf(stderr, "Could not add network to the tree\n");
		exit(EXIT_FAILURE);
	}

	// Dump the tree
	err = loc_network_tree_dump(tree);
	if (err) {
		fprintf(stderr, "Error dumping tree: %d\n", err);
		exit(EXIT_FAILURE);
	}

	size_t nodes = loc_network_tree_count_nodes(tree);
	printf("The tree has %zu nodes\n", nodes);
#endif

	// Check equals function
	err = loc_network_cmp(network1, network1);
	if (err) {
		fprintf(stderr, "Network is not equal with itself\n");
		exit(EXIT_FAILURE);
	}

	err = loc_network_cmp(network1, network2);
	if (!err) {
		fprintf(stderr, "Networks equal unexpectedly\n");
		exit(EXIT_FAILURE);
	}

	// Check subnet function
	err = loc_network_is_subnet(network1, network2);
	if (!err) {
		fprintf(stderr, "Subnet check 1 failed: %d\n", err);
		exit(EXIT_FAILURE);
	}

	err = loc_network_is_subnet(network2, network1);
	if (err) {
		fprintf(stderr, "Subnet check 2 failed: %d\n", err);
		exit(EXIT_FAILURE);
	}

	// Make subnets
	struct loc_network* subnet1 = NULL;
	struct loc_network* subnet2 = NULL;

	err  = loc_network_subnets(network1, &subnet1, &subnet2);
	if (err || !subnet1 || !subnet2) {
		fprintf(stderr, "Could not find subnets of network: %d\n", err);
		exit(EXIT_FAILURE);
	}

	const char* s = loc_network_str(subnet1);
	printf("Received subnet1 = %s\n", s);

	s = loc_network_str(subnet2);
	printf("Received subnet2 = %s\n", s);

	if (!loc_network_is_subnet(network1, subnet1)) {
		fprintf(stderr, "Subnet1 is not a subnet\n");
		exit(EXIT_FAILURE);
	}

	if (!loc_network_is_subnet(network1, subnet2)) {
		fprintf(stderr, "Subnet2 is not a subnet\n");
		exit(EXIT_FAILURE);
	}

	if (!loc_network_overlaps(network1, subnet1)) {
		fprintf(stderr, "Network1 does not seem to contain subnet1\n");
		exit(EXIT_FAILURE);
	}

	if (!loc_network_overlaps(network1, subnet2)) {
		fprintf(stderr, "Network1 does not seem to contain subnet2\n");
		exit(EXIT_FAILURE);
	}

	loc_network_unref(subnet1);
	loc_network_unref(subnet2);

	struct loc_network_list* excluded = loc_network_exclude(network1, network2);
	if (!excluded) {
		fprintf(stderr, "Could not create excluded list\n");
		exit(EXIT_FAILURE);
	}

	loc_network_list_dump(excluded);
	loc_network_list_unref(excluded);

	// Create a database
	struct loc_writer* writer;
	err = loc_writer_new(ctx, &writer, NULL, NULL);
	if (err < 0)
		exit(EXIT_FAILURE);

	struct loc_network* network3;
	err = loc_writer_add_network(writer, &network3, "2001:db8::/64");
	if (err) {
		fprintf(stderr, "Could not add network\n");
		exit(EXIT_FAILURE);
	}

	// Set country code
	loc_network_set_country_code(network3, "XX");

	struct loc_network* network4;
	err = loc_writer_add_network(writer, &network4, "2001:db8:ffff::/64");
	if (err) {
		fprintf(stderr, "Could not add network\n");
		exit(EXIT_FAILURE);
	}

	// Set country code
	loc_network_set_country_code(network4, "XY");

	// Set ASN
	loc_network_set_asn(network4, 1024);

	// Try adding an invalid network
	struct loc_network* network;
	err = loc_writer_add_network(writer, &network, "xxxx:xxxx::/32");
	if (!err) {
		fprintf(stderr, "It was possible to add an invalid network (err = %d)\n", err);
		exit(EXIT_FAILURE);
	}

	// Try adding a single address
	err = loc_writer_add_network(writer, &network, "2001:db8::");
	if (err) {
		fprintf(stderr, "It was impossible to add an single IP address (err = %d)\n", err);
		exit(EXIT_FAILURE);
	}

	FILE* f = tmpfile();
	if (!f) {
		fprintf(stderr, "Could not open file for writing: %m\n");
		exit(EXIT_FAILURE);
	}

	err = loc_writer_write(writer, f, LOC_DATABASE_VERSION_UNSET);
	if (err) {
		fprintf(stderr, "Could not write database: %m\n");
		exit(EXIT_FAILURE);
	}
	loc_writer_unref(writer);

	loc_network_unref(network1);
	loc_network_unref(network2);
	loc_network_unref(network3);
	loc_network_unref(network4);

#if 0
	loc_network_tree_unref(tree);
#endif

	// And open it again from disk
	struct loc_database* db;
	err = loc_database_new(ctx, &db, f);
	if (err) {
		fprintf(stderr, "Could not open database: %m\n");
		exit(EXIT_FAILURE);
	}

	// Lookup an address in the subnet
	err = loc_database_lookup_from_string(db, "2001:db8::", &network1);
	if (err) {
		fprintf(stderr, "Could not look up 2001:db8::\n");
		exit(EXIT_FAILURE);
	}
	loc_network_unref(network1);

	// Lookup an address outside the subnet
	err = loc_database_lookup_from_string(db, "2001:db8:fffe:1::", &network1);
	if (err || network1) {
		fprintf(stderr, "Could look up 2001:db8:fffe:1::, but I shouldn't\n");
		exit(EXIT_FAILURE);
	}

	const struct bit_length_test {
		const char* network;
		unsigned int bit_length;
	} bit_length_tests[] = {
		{ "::/0", 0 },
		{ "2001::/128", 16 },
		{ "1.0.0.0/32", 8 },
		{ "0.0.0.1/32", 32 },
		{ "255.255.255.255/32", 32 },
		{ NULL, 0, },
	};

	for (const struct bit_length_test* t = bit_length_tests; t->network; t++) {
		err = loc_network_new_from_string(ctx, &network1, t->network);
		if (err) {
			fprintf(stderr, "Could not create network %s: %m\n", t->network);
			exit(EXIT_FAILURE);
		}

		const struct in6_addr* addr = loc_network_get_first_address(network1);

		unsigned int bit_length = loc_address_bit_length(addr);

		if (bit_length != t->bit_length) {
			printf("Bit length of %s didn't match: %u != %u\n",
				t->network, t->bit_length, bit_length);
			loc_network_unref(network1);
			exit(EXIT_FAILURE);
		}

		loc_network_unref(network1);
	}

	// Test reverse pointers
	err = test_reverse_pointers(ctx);
	if (err)
		exit(err);

	loc_unref(ctx);
	fclose(f);

	return EXIT_SUCCESS;
}
