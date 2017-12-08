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
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <loc/libloc.h>

#include "libloc-private.h"
#include "database.h"
#include "stringpool.h"

struct loc_database {
	struct loc_ctx* ctx;
	int refcount;

	unsigned int version;
	off_t vendor;
	off_t description;

	struct loc_stringpool* pool;
};

const char* LOC_DATABASE_MAGIC = "LOCDBXX";
unsigned int LOC_DATABASE_VERSION = 0;

struct loc_database_magic {
	char magic[7];

	// Database version information
	uint8_t version;
};

struct loc_database_header_v0 {
	// Vendor who created the database
	uint32_t vendor;

	// Description of the database
	uint32_t description;

	// Tells us where the pool starts
	uint32_t pool_offset;
	uint32_t pool_length;
};

LOC_EXPORT int loc_database_new(struct loc_ctx* ctx, struct loc_database** database, size_t pool_size) {
	struct loc_database* db = calloc(1, sizeof(*db));
	if (!db)
		return -ENOMEM;

	// Reference context
	db->ctx = loc_ref(ctx);
	db->refcount = 1;

	DEBUG(db->ctx, "Database allocated at %p\n", db);

	// Create string pool
	int r = loc_stringpool_new(db->ctx, &db->pool, pool_size);
	if (r) {
		loc_database_unref(db);
		return r;
	}

	*database = db;

	return 0;
}

LOC_EXPORT int loc_database_open(struct loc_ctx* ctx, struct loc_database** database, FILE* f) {
	int r = loc_database_new(ctx, database, 0);
	if (r)
		return r;

	return loc_database_read(*database, f);
}

LOC_EXPORT struct loc_database* loc_database_ref(struct loc_database* db) {
	db->refcount++;

	return db;
}

static void loc_database_free(struct loc_database* db) {
	DEBUG(db->ctx, "Releasing database %p\n", db);

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

LOC_EXPORT const char* loc_database_get_vendor(struct loc_database* db) {
	return loc_stringpool_get(db->pool, db->vendor);
}

LOC_EXPORT int loc_database_set_vendor(struct loc_database* db, const char* vendor) {
	// Add the string to the string pool
	off_t offset = loc_stringpool_add(db->pool, vendor);
	if (offset < 0)
		return offset;

	db->vendor = offset;
	return 0;
}

LOC_EXPORT const char* loc_database_get_description(struct loc_database* db) {
	return loc_stringpool_get(db->pool, db->description);
}

LOC_EXPORT int loc_database_set_description(struct loc_database* db, const char* description) {
	// Add the string to the string pool
	off_t offset = loc_stringpool_add(db->pool, description);
	if (offset < 0)
		return offset;

	db->description = offset;
	return 0;
}

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
		db->version = ntohs(magic.version);
		DEBUG(db->ctx, "Database version is %u\n", db->version);

		return 0;
	}

	ERROR(db->ctx, "Database format is not compatible\n");

	// Return an error
	return 1;
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
	db->vendor      = ntohl(header.vendor);
	db->description = ntohl(header.description);

	// Open pool
	off_t pool_offset  = ntohl(header.pool_offset);
	size_t pool_length = ntohl(header.pool_length);

	int r = loc_stringpool_read(db->pool, f, pool_offset, pool_length);
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

LOC_EXPORT int loc_database_read(struct loc_database* db, FILE* f) {
	int r = fseek(f, 0, SEEK_SET);
	if (r)
		return r;

	// Read magic bytes
	r = loc_database_read_magic(db, f);
	if (r)
		return r;

	// Read the header
	r = loc_database_read_header(db, f);
	if (r)
		return r;

	return 0;
}

static void loc_database_make_magic(struct loc_database* db, struct loc_database_magic* magic) {
	// Copy magic bytes
	for (unsigned int i = 0; i < strlen(LOC_DATABASE_MAGIC); i++)
		magic->magic[i] = LOC_DATABASE_MAGIC[i];

	// Set version
	magic->version = htons(LOC_DATABASE_VERSION);
}

LOC_EXPORT int loc_database_write(struct loc_database* db, FILE* f) {
	struct loc_database_magic magic;
	loc_database_make_magic(db, &magic);

	// Make the header
	struct loc_database_header_v0 header;
	header.vendor      = htonl(db->vendor);
	header.description = htonl(db->description);

	int r;
	off_t offset = 0;

	// Start writing at the beginning of the file
	r = fseek(f, 0, SEEK_SET);
	if (r)
		return r;

	// Write the magic
	offset += fwrite(&magic, 1, sizeof(magic), f);

	// Skip the space we need to write the header later
	r = fseek(f, sizeof(header), SEEK_CUR);
	if (r) {
		DEBUG(db->ctx, "Could not seek to position after header\n");
		return r;
	}
	offset += sizeof(header);

	// Save the offset of the pool section
	DEBUG(db->ctx, "Pool starts at %jd bytes\n", offset);
	header.pool_offset = htonl(offset);

	// Size of the pool
	size_t pool_length = loc_stringpool_write(db->pool, f);
	DEBUG(db->ctx, "Pool has a length of %zu bytes\n", pool_length);
	header.pool_length = htonl(pool_length);

	// Write the header
	r = fseek(f, sizeof(magic), SEEK_SET);
	if (r)
		return r;

	offset += fwrite(&header, 1, sizeof(header), f);

	return 0;
}
