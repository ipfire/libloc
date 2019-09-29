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
	char* file;

	CODE:
		struct loc_ctx* ctx = NULL;

		// Initialise location context
		int err = loc_new(&ctx);
		if (err < 0)
			croak("Could not initialize libloc context: %d\n", err);

		// Open the database file for reading
		FILE* f = fopen(file, "r");
		if (!f) {
			loc_unref(ctx);

			croak("Could not open file for reading: %s: %s\n",
				file, strerror(errno));
		}

		// Parse the database
		struct loc_database* db = NULL;
		err = loc_database_new(ctx, &db, f);

		// We can close the database file straight away
		// because loc_database_new creates a copy of the file descriptor
		fclose(f);

		if (err) {
			loc_unref(ctx);

			croak("Could not read database: %s\n", file);
		}

		// Cleanup
		loc_unref(ctx);

		RETVAL = db;
	OUTPUT:
		RETVAL

#
# Database functions
#
const char*
get_vendor(db)
	struct loc_database* db;

	CODE:
		// Get vendor
		RETVAL = loc_database_get_vendor(db);
	OUTPUT:
		RETVAL

#
# Lookup functions
#
char*
lookup_country_code(db, address)
	struct loc_database* db;
	char* address;

	CODE:
		// Lookup network
		struct loc_network *network;
		int err = loc_database_lookup_from_string(db, address, &network);
		if (err) {
			croak("Could not look up for %s\n", address);
		}

		// Extract the country code
		const char* country_code = loc_network_get_country_code(network);
		loc_network_unref(network);

		if (country_code) {
			RETVAL = strdup(country_code);
		} else {
			RETVAL = NULL;
		}
	OUTPUT:
		RETVAL

void
DESTROY(db)
	struct loc_database* db;

	CODE:
		// Close database
		loc_database_unref(db);
