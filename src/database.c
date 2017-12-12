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
#include <unistd.h>

#include <loc/libloc.h>
#include <loc/format.h>

#include "libloc-private.h"
#include "as.h"
#include "database.h"
#include "stringpool.h"

struct loc_database {
	struct loc_ctx* ctx;
	int refcount;

	FILE* file;
	unsigned int version;
	off_t vendor;
	off_t description;

	// ASes in the database
	struct loc_as** as;
	size_t as_count;

	struct loc_stringpool* pool;
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

	// Remove references to all ASes
	if (db->as) {
		for (unsigned int i = 0; i < db->as_count; i++) {
			loc_as_unref(db->as[i]);
		}
		free(db->as);
	}

	loc_stringpool_unref(db->pool);

	// Close file
	if (db->file)
		fclose(db->file);

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

LOC_EXPORT size_t loc_database_count_as(struct loc_database* db) {
	return db->as_count;
}

static int loc_database_has_as(struct loc_database* db, struct loc_as* as) {
	for (unsigned int i = 0; i < db->as_count; i++) {
		if (loc_as_cmp(as, db->as[i]) == 0)
			return i;
	}

	return -1;
}

static int __loc_as_cmp(const void* as1, const void* as2) {
	return loc_as_cmp(*(struct loc_as**)as1, *(struct loc_as**)as2);
}

static void loc_database_sort_ases(struct loc_database* db) {
	qsort(db->as, db->as_count, sizeof(*db->as), __loc_as_cmp);
}

static struct loc_as* __loc_database_add_as(struct loc_database* db, struct loc_as* as) {
	// Check if AS exists already
	int i = loc_database_has_as(db, as);
	if (i >= 0) {
		loc_as_unref(as);

		// Select already existing AS
		as = db->as[i];

		return loc_as_ref(as);
	}

	db->as_count++;

	// Make space for the new entry
	db->as = realloc(db->as, sizeof(*db->as) * db->as_count);

	// Add the new entry at the end
	db->as[db->as_count - 1] = loc_as_ref(as);

	// Sort everything
	loc_database_sort_ases(db);

	return as;
}

LOC_EXPORT struct loc_as* loc_database_add_as(struct loc_database* db, uint32_t number) {
	struct loc_as* as;
	int r = loc_as_new(db->ctx, db->pool, &as, number);
	if (r)
		return NULL;

	return __loc_database_add_as(db, as);
}

static int loc_database_read_magic(struct loc_database* db) {
	struct loc_database_magic magic;

	// Read from file
	size_t bytes_read = fread(&magic, 1, sizeof(magic), db->file);

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

static int loc_database_read_as_section_v0(struct loc_database* db,
		off_t as_offset, size_t as_length) {
	struct loc_database_as_v0 dbobj;

	// Read from the start of the section
	int r = fseek(db->file, as_offset, SEEK_SET);
	if (r)
		return r;

	// Read all ASes
	size_t as_count = as_length / sizeof(dbobj);
	for (unsigned int i = 0; i < as_count; i++) {
		size_t bytes_read = fread(&dbobj, 1, sizeof(dbobj), db->file);
		if (bytes_read < sizeof(dbobj)) {
			ERROR(db->ctx, "Could not read an AS object\n");
			return -ENOMSG;
		}

		// Allocate a new AS
		struct loc_as* as;
		r = loc_as_new_from_database_v0(db->ctx, db->pool, &as, &dbobj);
		if (r)
			return r;

		// Attach it to the database
		as = __loc_database_add_as(db, as);
		loc_as_unref(as);
	}

	INFO(db->ctx, "Read %zu ASes from the database\n", db->as_count);

	return 0;
}

static int loc_database_read_header_v0(struct loc_database* db) {
	struct loc_database_header_v0 header;

	// Read from file
	size_t size = fread(&header, 1, sizeof(header), db->file);

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

	int r = loc_stringpool_read(db->pool, db->file, pool_offset, pool_length);
	if (r)
		return r;

	// AS section
	off_t as_offset  = ntohl(header.as_offset);
	size_t as_length = ntohl(header.as_length);

	r = loc_database_read_as_section_v0(db, as_offset, as_length);
	if (r)
		return r;

	return 0;
}

static int loc_database_read_header(struct loc_database* db) {
	switch (db->version) {
		case 0:
			return loc_database_read_header_v0(db);

		default:
			ERROR(db->ctx, "Incompatible database version: %u\n", db->version);
			return 1;
	}
}

LOC_EXPORT int loc_database_read(struct loc_database* db, FILE* f) {
	// Copy the file pointer and work on that so we don't care if
	// the calling function closes the file
	int fd = fileno(f);

	// Make a copy
	fd = dup(fd);

	// Retrieve a file pointer
	db->file = fdopen(fd, "r");
	if (!db->file)
		return -errno;

	int r = fseek(db->file, 0, SEEK_SET);
	if (r)
		return r;

	// Read magic bytes
	r = loc_database_read_magic(db);
	if (r)
		return r;

	// Read the header
	r = loc_database_read_header(db);
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

	// Write all ASes
	header.as_offset = htonl(offset);

	struct loc_database_as_v0 dbas;
	for (unsigned int i = 0; i < db->as_count; i++) {
		// Convert AS into database format
		loc_as_to_database_v0(db->as[i], &dbas);

		// Write to disk
		offset += fwrite(&dbas, 1, sizeof(dbas), f);
	}
	header.as_length = htonl(db->as_count * sizeof(dbas));

	// Move to next page boundary
	while (offset % LOC_DATABASE_PAGE_SIZE > 0)
		offset += fwrite("", 1, 1, f);

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
