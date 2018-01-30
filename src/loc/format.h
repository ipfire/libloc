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

#define LOC_DATABASE_MAGIC      "LOCDBXX"

#ifdef LIBLOC_PRIVATE

#define LOC_DATABASE_VERSION    0

#define LOC_DATABASE_PAGE_SIZE  4096

struct loc_database_magic {
	char magic[7];

	// Database version information
	uint8_t version;
};

struct loc_database_header_v0 {
	// UNIX timestamp when the database was created
	uint64_t created_at;

	// Vendor who created the database
	uint32_t vendor;

	// Description of the database
	uint32_t description;

	// License of the database
	uint32_t license;

	// Tells us where the ASes start
	uint32_t as_offset;
	uint32_t as_length;

	// Tells us where the networks start
	uint32_t network_data_offset;
	uint32_t network_data_length;

	// Tells us where the network nodes start
	uint32_t network_tree_offset;
	uint32_t network_tree_length;

	// Tells us where the pool starts
	uint32_t pool_offset;
	uint32_t pool_length;
};

struct loc_database_network_node_v0 {
	uint32_t zero;
	uint32_t one;

	uint32_t network;
};

struct loc_database_network_v0 {
	// The start address and prefix will be encoded in the tree

	// The country this network is located in
	char country_code[2];

	// ASN
	uint32_t asn;
};

struct loc_database_as_v0 {
	// The AS number
	uint32_t number;

	// Name
	uint32_t name;
};

#endif
#endif
