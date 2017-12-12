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

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <loc/libloc.h>
#include <loc/format.h>
#include "libloc-private.h"
#include "stringpool.h"

struct loc_stringpool {
	struct loc_ctx* ctx;

	int refcount;
	char* data;
	char* pos;

	ssize_t max_length;
};

static int loc_stringpool_deallocate(struct loc_stringpool* pool) {
	if (pool->data) {
		int r = munmap(pool->data, pool->max_length);
		if (r) {
			ERROR(pool->ctx, "Could not unmap data at %p: %s\n",
				pool->data, strerror(errno));

			return r;
		}
	}

	return 0;
}

static int loc_stringpool_allocate(struct loc_stringpool* pool, size_t length) {
	// Drop old data
	int r = loc_stringpool_deallocate(pool);
	if (r)
		return r;

	pool->max_length = length;

	// Align to page size
	while (pool->max_length % sysconf(_SC_PAGE_SIZE) > 0)
		pool->max_length++;

	DEBUG(pool->ctx, "Allocating pool of %zu bytes\n", pool->max_length);

	// Allocate some memory
	pool->data = pool->pos = mmap(NULL, pool->max_length,
		PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

	if (pool->data == MAP_FAILED) {
		DEBUG(pool->ctx, "%s\n", strerror(errno));
		return -errno;
	}

	DEBUG(pool->ctx, "Allocated pool at %p\n", pool->data);

	return 0;
}

LOC_EXPORT int loc_stringpool_new(struct loc_ctx* ctx, struct loc_stringpool** pool, size_t max_length) {
	struct loc_stringpool* p = calloc(1, sizeof(*p));
	if (!p)
		return -ENOMEM;

	p->ctx = loc_ref(ctx);
	p->refcount = 1;

	// Allocate the data section
	if (max_length > 0) {
		int r = loc_stringpool_allocate(p, max_length);
		if (r) {
			loc_stringpool_unref(p);
			return r;
		}
	}

	DEBUG(p->ctx, "String pool allocated at %p\n", p);
	DEBUG(p->ctx, "  Maximum size: %zu bytes\n", p->max_length);
	*pool = p;

	return 0;
}

LOC_EXPORT struct loc_stringpool* loc_stringpool_ref(struct loc_stringpool* pool) {
	pool->refcount++;

	return pool;
}

static void loc_stringpool_free(struct loc_stringpool* pool) {
	DEBUG(pool->ctx, "Releasing string pool %p\n", pool);

	loc_stringpool_deallocate(pool);
	loc_unref(pool->ctx);
	free(pool);
}

LOC_EXPORT struct loc_stringpool* loc_stringpool_unref(struct loc_stringpool* pool) {
	if (--pool->refcount > 0)
		return NULL;

	loc_stringpool_free(pool);

	return NULL;
}

static off_t loc_stringpool_get_offset(struct loc_stringpool* pool, const char* pos) {
	if (pos < pool->data)
		return -EFAULT;

	if (pos > (pool->data + pool->max_length))
		return -EFAULT;

	return pos - pool->data;
}

static off_t loc_stringpool_get_next_offset(struct loc_stringpool* pool, off_t offset) {
	const char* string = loc_stringpool_get(pool, offset);

	return offset + strlen(string) + 1;
}

static size_t loc_stringpool_space_left(struct loc_stringpool* pool) {
	return pool->max_length - loc_stringpool_get_size(pool);
}

LOC_EXPORT const char* loc_stringpool_get(struct loc_stringpool* pool, off_t offset) {
	if (offset >= (ssize_t)pool->max_length)
		return NULL;

	const char* string = pool->data + offset;

	// If the string is empty, we have reached the end
	if (!*string)
		return NULL;

	return string;
}

LOC_EXPORT size_t loc_stringpool_get_size(struct loc_stringpool* pool) {
	return loc_stringpool_get_offset(pool, pool->pos);
}

static off_t loc_stringpool_find(struct loc_stringpool* pool, const char* s) {
	if (!s || !*s)
		return -EINVAL;

	off_t offset = 0;
	while (offset < pool->max_length) {
		const char* string = loc_stringpool_get(pool, offset);
		if (!string)
			break;

		int r = strcmp(s, string);
		if (r == 0)
			return offset;

		offset = loc_stringpool_get_next_offset(pool, offset);
	}

	return -ENOENT;
}

static off_t loc_stringpool_append(struct loc_stringpool* pool, const char* string) {
	if (!string || !*string)
		return -EINVAL;

	DEBUG(pool->ctx, "Appending '%s' to string pool at %p\n", string, pool);

	// Check if we have enough space left
	size_t l = strlen(string) + 1;
	if (l > loc_stringpool_space_left(pool)) {
		DEBUG(pool->ctx, "Not enough space to append '%s'\n", string);
		DEBUG(pool->ctx, "  Need %zu bytes but only have %zu\n", l, loc_stringpool_space_left(pool));
		return -ENOSPC;
	}

	off_t offset = loc_stringpool_get_offset(pool, pool->pos);

	// Copy string byte by byte
	while (*string && loc_stringpool_space_left(pool) > 1) {
		*pool->pos++ = *string++;
	}

	// Terminate the string
	*pool->pos++ = '\0';

	return offset;
}

LOC_EXPORT off_t loc_stringpool_add(struct loc_stringpool* pool, const char* string) {
	off_t offset = loc_stringpool_find(pool, string);
	if (offset >= 0) {
		DEBUG(pool->ctx, "Found '%s' at position %jd\n", string, offset);
		return offset;
	}

	return loc_stringpool_append(pool, string);
}

LOC_EXPORT void loc_stringpool_dump(struct loc_stringpool* pool) {
	off_t offset = 0;

	while (offset < pool->max_length) {
		const char* string = loc_stringpool_get(pool, offset);
		if (!string)
			break;

		printf("%jd (%zu): %s\n", offset, strlen(string), string);

		offset = loc_stringpool_get_next_offset(pool, offset);
	}
}

#include <assert.h>

LOC_EXPORT int loc_stringpool_read(struct loc_stringpool* pool, FILE* f, off_t offset, size_t length) {
	DEBUG(pool->ctx, "Reading string pool from %zu (%zu bytes)\n", offset, length);

	pool->data = pool->pos = mmap(NULL, length, PROT_READ,
		MAP_PRIVATE, fileno(f), offset);
	pool->max_length = length;

	if (pool->data == MAP_FAILED)
		return -errno;

	return 0;
}

LOC_EXPORT size_t loc_stringpool_write(struct loc_stringpool* pool, FILE* f) {
	size_t size = loc_stringpool_get_size(pool);

	size_t bytes_written = fwrite(pool->data, sizeof(*pool->data), size, f);

	// Move to next page boundary
	while (bytes_written % LOC_DATABASE_PAGE_SIZE > 0)
		bytes_written += fwrite("", 1, 1, f);

	return bytes_written;
}
