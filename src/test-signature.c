/*
	libloc - A library to determine the location of someone on the Internet

	Copyright (C) 2019 IPFire Development Team <info@ipfire.org>

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
#include <loc/database.h>
#include <loc/writer.h>

int main(int argc, char** argv) {
	int err;

	// Open public key
	FILE* public_key = fopen(ABS_SRCDIR "/examples/public-key.pem", "r");
	if (!public_key) {
		fprintf(stderr, "Could not open public key file: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Open private key
	FILE* private_key = fopen(ABS_SRCDIR "/examples/private-key.pem", "r");
	if (!private_key) {
		fprintf(stderr, "Could not open private key file: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	struct loc_ctx* ctx;
	err = loc_new(&ctx);
	if (err < 0)
		exit(EXIT_FAILURE);

	// Create an empty database
	struct loc_writer* writer;
	err = loc_writer_new(ctx, &writer, private_key);
	if (err < 0)
		exit(EXIT_FAILURE);

	FILE* f = fopen("test.db", "w+");
	if (!f) {
		fprintf(stderr, "Could not open file for writing: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	err = loc_writer_write(writer, f);
	if (err) {
		fprintf(stderr, "Could not write database: %s\n", strerror(err));
		exit(EXIT_FAILURE);
	}
	loc_writer_unref(writer);

	// Close the file
	fclose(f);

	// And open it again from disk
	f = fopen("test.db", "r");
	if (!f) {
		fprintf(stderr, "Could not open file for reading: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	struct loc_database* db;
	err = loc_database_new(ctx, &db, f);
	if (err) {
		fprintf(stderr, "Could not open database: %s\n", strerror(-err));
		exit(EXIT_FAILURE);
	}

	// Verify the database signature
	err = loc_database_verify(db, public_key);
	if (err) {
		fprintf(stderr, "Could not verify the database: %d\n", err);
		exit(EXIT_FAILURE);
	}

	// Open another public key
	public_key = freopen(ABS_SRCDIR "/src/signing-key.pem", "r", public_key);
	if (!public_key) {
		fprintf(stderr, "Could not open public key file: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// Verify with an incorrect key
	err = loc_database_verify(db, public_key);
	if (err == 0) {
		fprintf(stderr, "Database was verified with an incorrect key: %d\n", err);
		exit(EXIT_FAILURE);
	}

	// Close the database
	loc_database_unref(db);

	loc_unref(ctx);

	fclose(private_key);
	fclose(public_key);

	return EXIT_SUCCESS;
}
