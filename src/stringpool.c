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

#include <libloc/libloc.h>
#include <libloc/format.h>
#include <libloc/private.h>
#include <libloc/stringpool.h>

struct loc_stringpool {
	struct loc_ctx* ctx;
	int refcount;

	// A file descriptor when we open an existing stringpool
	int fd;

	off_t offset;
	ssize_t length;

	// Mapped data (from mmap())
	char* mmapped_data;

	char* data;
	char* pos;

	char buffer[LOC_DATABASE_PAGE_SIZE];
};

static off_t loc_stringpool_get_offset(struct loc_stringpool* pool, const char* pos) {
	if (pos < pool->data) {
		errno = EFAULT;
		return -1;
	}

	if (pos > (pool->data + pool->length)) {
		errno = EFAULT;
		return -1;
	}

	return pos - pool->data;
}

static char* __loc_stringpool_get(struct loc_stringpool* pool, off_t offset) {
	ssize_t bytes_read;

	// Check boundaries
	if (offset < 0 || offset >= pool->length) {
		errno = ERANGE;
		return NULL;
	}

	// Return any data that we have in memory
	if (pool->data)
		return pool->data + offset;

	// Otherwise read a block from file
	bytes_read = pread(pool->fd, pool->buffer, sizeof(pool->buffer),
		pool->offset + offset);

	// Break on error
	if (bytes_read < 0) {
		ERROR(pool->ctx, "Could not read from string pool: %m\n");
		return NULL;
	}

	// It is okay, if we did not read as much as we wanted, since we might be reading
	// the last block which might be of an unknown size.

	// Search for a complete string. If there is no NULL byte, the block is garbage.
	char* end = memchr(pool->buffer, bytes_read, '\0');
	if (!end)
		return NULL;

	// Return what's in the buffer
	return pool->buffer;
}

static int loc_stringpool_grow(struct loc_stringpool* pool, size_t length) {
	DEBUG(pool->ctx, "Growing string pool to %zu bytes\n", length);

	// Save pos pointer
	off_t pos = loc_stringpool_get_offset(pool, pool->pos);

	// Reallocate data section
	pool->data = realloc(pool->data, length);
	if (!pool->data)
		return 1;

	pool->length = length;

	// Restore pos
	pool->pos = __loc_stringpool_get(pool, pos);

	return 0;
}

static off_t loc_stringpool_append(struct loc_stringpool* pool, const char* string) {
	if (!string) {
		errno = EINVAL;
		return -1;
	}

	DEBUG(pool->ctx, "Appending '%s' to string pool at %p\n", string, pool);

	// Make sure we have enough space
	int r = loc_stringpool_grow(pool, pool->length + strlen(string) + 1);
	if (r)
		return -1;

	off_t offset = loc_stringpool_get_offset(pool, pool->pos);

	// Copy string byte by byte
	while (*string)
		*pool->pos++ = *string++;

	// Terminate the string
	*pool->pos++ = '\0';

	return offset;
}

static void loc_stringpool_free(struct loc_stringpool* pool) {
	DEBUG(pool->ctx, "Releasing string pool %p\n", pool);
	int r;

	// Close file
	if (pool->fd > 0)
		close(pool->fd);

	// Unmap any mapped memory
	if (pool->mmapped_data) {
		r = munmap(pool->mmapped_data, pool->length);
		if (r)
			ERROR(pool->ctx, "Error unmapping string pool: %m\n");

		if (pool->mmapped_data == pool->data)
			pool->data = NULL;
	}

	// Free any data
	if (pool->data)
		free(pool->data);

	loc_unref(pool->ctx);
	free(pool);
}

int loc_stringpool_new(struct loc_ctx* ctx, struct loc_stringpool** pool) {
	struct loc_stringpool* p = calloc(1, sizeof(*p));
	if (!p)
		return 1;

	p->ctx = loc_ref(ctx);
	p->refcount = 1;

	*pool = p;

	return 0;
}

static int loc_stringpool_mmap(struct loc_stringpool* pool) {
	// Try mmap()
	char* p = mmap(NULL, pool->length, PROT_READ, MAP_PRIVATE, pool->fd, pool->offset);

	if (p == MAP_FAILED) {
		// Ignore if data hasn't been aligned correctly
		if (errno == EINVAL)
			return 0;

		ERROR(pool->ctx, "Could not mmap stringpool: %m\n");
		return 1;
	}

	// Store mapped memory area
	pool->data = pool->mmapped_data = pool->pos = p;

	return 0;
}

int loc_stringpool_open(struct loc_ctx* ctx, struct loc_stringpool** pool,
		FILE* f, size_t length, off_t offset) {
	struct loc_stringpool* p = NULL;

	// Allocate a new stringpool
	int r = loc_stringpool_new(ctx, &p);
	if (r)
		goto ERROR;

	// Store offset and length
	p->offset = offset;
	p->length = length;

	DEBUG(p->ctx, "Reading string pool starting from %jd (%zu bytes)\n",
		(intmax_t)p->offset, p->length);

	int fd = fileno(f);

	// Copy the file descriptor
	p->fd = dup(fd);
	if (p->fd < 0) {
		ERROR(ctx, "Could not duplicate file the file descriptor: %m\n");
		r = 1;
		goto ERROR;
	}

	// Map data into memory
	if (p->length > 0) {
		r = loc_stringpool_mmap(p);
		if (r)
			goto ERROR;
	}

	*pool = p;
	return 0;

ERROR:
	if (p)
		loc_stringpool_free(p);

	return r;
}

struct loc_stringpool* loc_stringpool_ref(struct loc_stringpool* pool) {
	pool->refcount++;

	return pool;
}

struct loc_stringpool* loc_stringpool_unref(struct loc_stringpool* pool) {
	if (--pool->refcount > 0)
		return NULL;

	loc_stringpool_free(pool);

	return NULL;
}

const char* loc_stringpool_get(struct loc_stringpool* pool, off_t offset) {
	return __loc_stringpool_get(pool, offset);
}

size_t loc_stringpool_get_size(struct loc_stringpool* pool) {
	return loc_stringpool_get_offset(pool, pool->pos);
}

static off_t loc_stringpool_find(struct loc_stringpool* pool, const char* s) {
	if (!s || !*s) {
		errno = EINVAL;
		return -1;
	}

	off_t offset = 0;
	while (offset < pool->length) {
		const char* string = loc_stringpool_get(pool, offset);

		// Error!
		if (!string)
			return 1;

		// Is this a match?
		if (strcmp(s, string) == 0)
			return offset;

		// Shift offset
		offset += strlen(string) + 1;
	}

	// Nothing found
	errno = ENOENT;
	return -1;
}

off_t loc_stringpool_add(struct loc_stringpool* pool, const char* string) {
	off_t offset = loc_stringpool_find(pool, string);
	if (offset >= 0) {
		DEBUG(pool->ctx, "Found '%s' at position %jd\n", string, (intmax_t)offset);
		return offset;
	}

	return loc_stringpool_append(pool, string);
}

void loc_stringpool_dump(struct loc_stringpool* pool) {
	off_t offset = 0;

	while (offset < pool->length) {
		const char* string = loc_stringpool_get(pool, offset);
		if (!string)
			return;

		printf("%jd (%zu): %s\n", (intmax_t)offset, strlen(string), string);

		// Shift offset
		offset += strlen(string) + 1;
	}
}

size_t loc_stringpool_write(struct loc_stringpool* pool, FILE* f) {
	size_t size = loc_stringpool_get_size(pool);

	return fwrite(pool->data, sizeof(*pool->data), size, f);
}
