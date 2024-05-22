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

#ifndef LIBLOC_H
#define LIBLOC_H

#include <netinet/in.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct loc_ctx;
struct loc_ctx *loc_ref(struct loc_ctx* ctx);
struct loc_ctx *loc_unref(struct loc_ctx* ctx);

int loc_new(struct loc_ctx** ctx);

typedef void (*loc_log_callback)(
	struct loc_ctx* ctx,
	void* data,
	int priority,
	const char* file,
	int line,
	const char* fn,
	const char* format,
	va_list args);
void loc_set_log_callback(struct loc_ctx* ctx, loc_log_callback callback, void* data);

void loc_set_log_fn(struct loc_ctx* ctx,
	void (*log_fn)(struct loc_ctx* ctx,
	int priority, const char* file, int line, const char* fn,
	const char* format, va_list args));
int loc_get_log_priority(struct loc_ctx* ctx);
void loc_set_log_priority(struct loc_ctx* ctx, int priority);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
