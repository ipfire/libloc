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

#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <loc/libloc.h>
#include <loc/network.h>
#include <loc/writer.h>

int main(int argc, char** argv) {
	int err;

	struct loc_ctx* ctx;
	err = loc_new(&ctx);
	if (err < 0)
		exit(EXIT_FAILURE);

	struct loc_network_tree* tree;
	err = loc_network_tree_new(ctx, &tree);
	if (err) {
		fprintf(stderr, "Could not create the network tree\n");
		exit(EXIT_FAILURE);
	}

	// Create a network
	struct loc_network* network1;
	err = loc_network_new_from_string(ctx, &network1, "2001:db8::/32");
	if (err) {
		fprintf(stderr, "Could not create the network\n");
		exit(EXIT_FAILURE);
	}

	err = loc_network_set_country_code(network1, "XX");
	if (err) {
		fprintf(stderr, "Could not set country code\n");
		exit(EXIT_FAILURE);
	}

	// Adding network to the tree
	err = loc_network_tree_add_network(tree, network1);
	if (err) {
		fprintf(stderr, "Could not add network to the tree\n");
		exit(EXIT_FAILURE);
	}

	struct loc_network* network2;
	err = loc_network_new_from_string(ctx, &network2, "2001:db8:ffff::/48");
	if (err) {
		fprintf(stderr, "Could not create the network\n");
		exit(EXIT_FAILURE);
	}

	err = loc_network_set_country_code(network2, "XY");
	if (err) {
		fprintf(stderr, "Could not set country code\n");
		exit(EXIT_FAILURE);
	}

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

	// Create a database
	struct loc_writer* writer;
	err = loc_writer_new(ctx, &writer);
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

	FILE* f = fopen("test.db", "w");
	if (!f) {
		fprintf(stderr, "Could not open file for writing: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	err = loc_writer_write(writer, f);
	if (err) {
		fprintf(stderr, "Could not write database: %s\n", strerror(-err));
		exit(EXIT_FAILURE);
	}
	fclose(f);

	loc_writer_unref(writer);

	loc_network_unref(network1);
	loc_network_unref(network2);
	loc_network_unref(network3);
	loc_network_unref(network4);
	loc_network_tree_unref(tree);
	loc_unref(ctx);

	return EXIT_SUCCESS;
}
