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

#ifndef LIBLOC_FORMAT_H
#define LIBLOC_FORMAT_H

#include <stdint.h>

struct loc_database_header_v0 {
	// Vendor who created the database
	uint32_t vendor;

	// Description of the database
	uint32_t description;

	// Tells us where the ASes start
	uint32_t as_offset;
	uint32_t as_length;

	// Tells us where the pool starts
	uint32_t pool_offset;
	uint32_t pool_length;
};

struct loc_database_as_v0 {
	// The AS number
	uint32_t number;

	// Name
	uint32_t name;
};

#endif
