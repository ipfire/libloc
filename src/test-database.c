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

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include <loc/libloc.h>
#include "database.h"

const char* DESCRIPTION =
	"Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
	"Proin ultrices pulvinar dolor, et sollicitudin eros ultricies "
	"vitae. Nam in volutpat libero. Nulla facilisi. Pellentesque "
	"tempor felis enim. Integer congue nisi in maximus pretium. "
	"Pellentesque et turpis elementum, luctus mi at, interdum erat. "
	"Maecenas ut venenatis nunc.";

int main(int argc, char** argv) {
	int err;

	struct loc_ctx* ctx;
	err = loc_new(&ctx);
	if (err < 0)
		exit(EXIT_FAILURE);

	// Create a database
	struct loc_database* db;
	err = loc_database_new(ctx, &db, 1024);
	if (err < 0)
		exit(EXIT_FAILURE);

	// Set the vendor
	err = loc_database_set_vendor(db, "Test Vendor");
	if (err) {
		fprintf(stderr, "Could not set vendor\n");
		exit(EXIT_FAILURE);
	}

	// Retrieve vendor
	const char* vendor1 = loc_database_get_vendor(db);
	if (vendor1) {
		printf("Vendor is: %s\n", vendor1);
	} else {
		fprintf(stderr, "Could not retrieve vendor\n");
		exit(EXIT_FAILURE);
	}

	// Set a description
	err = loc_database_set_description(db, DESCRIPTION);
	if (err) {
		fprintf(stderr, "Could not set description\n");
		exit(EXIT_FAILURE);
	}

	// Retrieve description
	const char* description = loc_database_get_description(db);
	if (description) {
		printf("Description is: %s\n", description);
	} else {
		fprintf(stderr, "Could not retrieve description\n");
		exit(EXIT_FAILURE);
	}

	FILE* f = fopen("test.db", "w");
	if (!f) {
		fprintf(stderr, "Could not open file for writing: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	err = loc_database_write(db, f);
	if (err) {
		fprintf(stderr, "Could not write database: %s\n", strerror(err));
		exit(EXIT_FAILURE);
	}

	// Close the file
	fclose(f);

	// Close the database
	//loc_database_unref(db);

	// And open it again from disk
	f = fopen("test.db", "r");
	if (!f) {
		fprintf(stderr, "Could not open file for reading: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	struct loc_database* db2;
	err = loc_database_open(ctx, &db2, f);
	if (err) {
		fprintf(stderr, "Could not open database: %s\n", strerror(-err));
		exit(EXIT_FAILURE);
	}

	const char* vendor2 = loc_database_get_vendor(db2);
	if (!vendor2) {
		fprintf(stderr, "Could not retrieve vendor\n");
		exit(EXIT_FAILURE);
	} else if (strcmp(vendor1, vendor2) != 0) {
		fprintf(stderr, "Vendor doesn't match: %s != %s\n", vendor1, vendor2);
		exit(EXIT_FAILURE);
	}

	// Close the database
	loc_database_unref(db2);
	fclose(f);

	loc_unref(ctx);

	return EXIT_SUCCESS;
}
