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

#ifndef LIBLOC_DATABASE_H
#define LIBLOC_DATABASE_H

#include <stdio.h>
#include <stdint.h>

#include <loc/libloc.h>
#include <loc/as.h>

struct loc_database;
int loc_database_new(struct loc_ctx* ctx, struct loc_database** database, FILE* f);
struct loc_database* loc_database_ref(struct loc_database* db);
struct loc_database* loc_database_unref(struct loc_database* db);

time_t loc_database_created_at(struct loc_database* db);
const char* loc_database_get_vendor(struct loc_database* db);
const char* loc_database_get_description(struct loc_database* db);

int loc_database_get_as(struct loc_database* db, struct loc_as** as, uint32_t number);
size_t loc_database_count_as(struct loc_database* db);

int loc_database_write(struct loc_database* db, FILE* f);

#endif
