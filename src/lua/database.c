/*
	libloc - A library to determine the location of someone on the Internet

	Copyright (C) 2024 IPFire Development Team <info@ipfire.org>

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
#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include <libloc/database.h>

#include "location.h"
#include "database.h"
#include "network.h"

typedef struct database {
	struct loc_database* db;
} Database;

static Database* luaL_checkdatabase(lua_State* L, int i) {
	void* userdata = luaL_checkudata(L, i, "location.Database");

	// Throw an error if the argument doesn't match
	luaL_argcheck(L, userdata, i, "Database expected");

	return (Database*)userdata;
}

static int Database_open(lua_State* L) {
	const char* path = NULL;
	FILE* f = NULL;
	int r;

	// Fetch the path
	path = luaL_checkstring(L, 1);

	// Allocate a new object
	Database* self = (Database*)lua_newuserdata(L, sizeof(*self));

	// Set metatable
	luaL_setmetatable(L, "location.Database");

	// Open the database file
	f = fopen(path, "r");
	if (!f)
		return luaL_error(L, "Could not open %s: %s\n", path, strerror(errno));

	// Open the database
	r = loc_database_new(ctx, &self->db, f);

	// Close the file descriptor
	fclose(f);

	// Check for errors
	if (r)
		return luaL_error(L, "Could not open database %s: %s\n", path, strerror(errno));

	return 1;
}

static int Database_close(lua_State* L) {
	Database* self = luaL_checkdatabase(L, 0);

	if (self->db)
		loc_database_unref(self->db);

	free(self);
	return 0;
}

static int Database_lookup(lua_State* L) {
	struct loc_network* network = NULL;
	int r;

	Database* self = luaL_checkdatabase(L, 0);

	// Require a string
	const char* address = luaL_checkstring(L, 1);

	// Perform lookup
	r = loc_database_lookup_from_string(self->db, address, &network);
	if (r)
		return luaL_error(L, "Could not lookup address %s: %s\n", address, strerror(errno));

	// Create a network object
	r = create_network(L, network);
	loc_network_unref(network);

	return r;
}

static const struct luaL_Reg database_functions[] = {
	{ "open", Database_open },
	{ "lookup", Database_lookup },
	{ "__gc", Database_close },
	{ NULL, NULL },
};

int register_database(lua_State* L) {
	return register_class(L, "location.Database", database_functions);
}
