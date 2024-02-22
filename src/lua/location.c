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
#include <lualib.h>

#include <libloc/libloc.h>

#include "location.h"
#include "database.h"
#include "network.h"

struct loc_ctx* ctx = NULL;

static int version(lua_State* L) {
	lua_pushstring(L, PACKAGE_VERSION);
	return 1;
}

static const struct luaL_Reg location_functions[] = {
	{ "version", version },
	{ NULL, NULL },
};

int luaopen_location(lua_State* L) {
	int r;

	// Initialize the context
	r = loc_new(&ctx);
	if (r)
		return luaL_error(L,
			"Could not initialize location context: %s\n", strerror(errno));

	// Register functions
	luaL_newlib(L, location_functions);

	// Register Database type
	register_database(L);

	lua_setfield(L, -2, "Database");

	// Register Network type
	register_network(L);

	lua_setfield(L, -2, "Network");

	return 1;
}
