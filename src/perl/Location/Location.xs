#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <stdio.h>
#include <string.h>


#include <loc/libloc.h>
#include <loc/database.h>
#include <loc/network.h>


MODULE = Location		PACKAGE = Location

struct loc_database *
init(file)
	char * file;

	CODE:
		struct loc_ctx* ctx = NULL;

		// Initialise location context
		int err = loc_new(&ctx);
		if (err < 0)
			croak("Could not initialize libloc context: %d\n", err);

		// Open the database file for reading
		FILE* f = fopen(file, "r");
		if (!f) {
			croak("Could not open file for reading: %s: %s\n",
				file, strerror(errno));
		}

		// Parse the database
		struct loc_database *db = NULL;
		err = loc_database_new(ctx, &db, f);
		if (err) {
			croak("Could not read database: %s\n", file);
		}

		RETVAL = db;
	OUTPUT:
		RETVAL

char*
get_country_code(db, address)
	struct loc_database* db = NULL;
	char* address = NULL;

	CODE:
		int err;
		const char* country_code = NULL;

		// Lookup network
		struct loc_network *network;
		err = loc_database_lookup_from_string(db, address, &network);
		if (err) {
			croak("Could not look up for %s\n", address);
		}

		country_code = loc_network_get_country_code(network);
		loc_network_unref(network);

		if (!country_code) {
			croak("Could not get the country code for %s\n", address);
		}

		RETVAL = strdup(country_code);
	OUTPUT:
		RETVAL



char*
database_get_vendor(db)
	struct loc_database* db = NULL;

	CODE:
		const char* vendor = NULL;

		// Get vendor
		vendor = loc_database_get_vendor(db);
		if (!vendor) {
			croak("Could not retrieve vendor\n");
		}

		RETVAL = strdup(vendor);
	OUTPUT:
		RETVAL

void
DESTROY(db)
	struct loc_database* db = NULL;

	CODE:
		// Close database
		loc_database_unref(db);
