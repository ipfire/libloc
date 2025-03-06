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
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <libloc/libloc.h>
#include <libloc/compat.h>
#include <libloc/private.h>

struct loc_ctx {
	int refcount;

	// Logging
	struct loc_ctx_logging {
		int priority;

		// Callback
		loc_log_callback callback;
		void* data;
	} log;
};

void loc_log(struct loc_ctx* ctx,
		int priority, const char* file, int line, const char* fn,
		const char* format, ...) {
	va_list args;

	va_start(args, format);
	ctx->log.callback(ctx, ctx->log.data, priority, file, line, fn, format, args);
	va_end(args);
}

static void log_stderr(struct loc_ctx* ctx, void* data, int priority,
	const char* file, int line, const char* fn, const char* format, va_list args)
	__attribute__((format(printf, 7, 0)));

static void log_stderr(struct loc_ctx* ctx, void* data,
		int priority, const char* file, int line, const char* fn,
		const char* format, va_list args) {
	fprintf(stderr, "libloc: %s: ", fn);
	vfprintf(stderr, format, args);
}

static int log_priority(const char* priority) {
	char *endptr;

	int prio = strtol(priority, &endptr, 10);

	if (endptr[0] == '\0' || isspace(endptr[0]))
		return prio;

	if (strncmp(priority, "err", 3) == 0)
		return LOG_ERR;

	if (strncmp(priority, "info", 4) == 0)
		return LOG_INFO;

	if (strncmp(priority, "debug", 5) == 0)
		return LOG_DEBUG;

	return 0;
}

LOC_EXPORT int loc_new(struct loc_ctx** ctx) {
	struct loc_ctx* c = calloc(1, sizeof(*c));
	if (!c)
		return 1;

	c->refcount = 1;
	c->log.callback = log_stderr;
	c->log.priority = LOG_ERR;

	const char* env = secure_getenv("LOC_LOG");
	if (env)
		loc_set_log_priority(c, log_priority(env));

	INFO(c, "ctx %p created\n", c);
	DEBUG(c, "log_priority=%d\n", c->log.priority);
	*ctx = c;

	return 0;
}

LOC_EXPORT struct loc_ctx* loc_ref(struct loc_ctx* ctx) {
	if (!ctx)
		return NULL;

	ctx->refcount++;

	return ctx;
}

LOC_EXPORT struct loc_ctx* loc_unref(struct loc_ctx* ctx) {
	if (--ctx->refcount > 0)
		return NULL;

	INFO(ctx, "context %p released\n", ctx);
	free(ctx);

	return NULL;
}

LOC_EXPORT void loc_set_log_callback(struct loc_ctx* ctx, loc_log_callback callback, void* data) {
	ctx->log.callback = callback;
	ctx->log.data     = data;
}

LOC_EXPORT void loc_set_log_fn(struct loc_ctx* ctx,
		void (*log_fn)(struct loc_ctx* ctx, int priority, const char* file,
		int line, const char* fn, const char* format, va_list args)) {
	//ctx->log_fn = log_fn;
	INFO(ctx, "custom logging function %p registered\n", log_fn);
}

LOC_EXPORT int loc_get_log_priority(struct loc_ctx* ctx) {
	return ctx->log.priority;
}

LOC_EXPORT void loc_set_log_priority(struct loc_ctx* ctx, int priority) {
	ctx->log.priority = priority;
}
