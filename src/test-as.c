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
#include <stdlib.h>
#include <string.h>

#include <loc/libloc.h>
#include "database.h"

#define TEST_AS_COUNT 100

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

	char name[256];
	for (unsigned int i = 1; i <= TEST_AS_COUNT; i++) {
		struct loc_as* as = loc_database_add_as(db, i);

		sprintf(name, "Test AS%u", i);
		loc_as_set_name(as, name);

		loc_as_unref(as);
	}

	FILE* f = fopen("test.db", "w");
	if (!f) {
		fprintf(stderr, "Could not open file for writing: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	err = loc_database_write(db, f);
	if (err) {
		fprintf(stderr, "Could not write database: %s\n", strerror(-err));
		exit(EXIT_FAILURE);
	}
	fclose(f);

	loc_database_unref(db);

	// And open it again from disk
	f = fopen("test.db", "r");
	if (!f) {
		fprintf(stderr, "Could not open file for reading: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	err = loc_database_open(ctx, &db, f);
	if (err) {
		fprintf(stderr, "Could not open database: %s\n", strerror(-err));
		exit(EXIT_FAILURE);
	}

	size_t as_count = loc_database_count_as(db);
	if (as_count != TEST_AS_COUNT) {
		fprintf(stderr, "Could not read all ASes\n");
		exit(EXIT_FAILURE);
	}

	loc_database_unref(db);
	loc_unref(ctx);

	return EXIT_SUCCESS;
}
