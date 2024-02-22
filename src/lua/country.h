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

#ifndef LUA_LOCATION_COUNTRY_H
#define LUA_LOCATION_COUNTRY_H

#include <lua.h>
#include <lauxlib.h>

#include <libloc/country.h>

int register_country(lua_State* L);

int create_country(lua_State* L, struct loc_country* country);

#endif /* LUA_LOCATION_COUNTRY_H */
