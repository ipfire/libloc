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

const char*
get_description(db)
	struct loc_database* db;

	CODE:
		// Get database description
		RETVAL = loc_database_get_description(db);
	OUTPUT:
		RETVAL

const char*
get_license(db)
	struct loc_database* db;

	CODE:
		// Get database license
		RETVAL = loc_database_get_license(db);
	OUTPUT:
		RETVAL

#
# Lookup functions
#
SV*
lookup_country_code(db, address)
	struct loc_database* db;
	char* address;

	CODE:
		RETVAL = &PL_sv_undef;

		// Lookup network
		struct loc_network *network;
		int err = loc_database_lookup_from_string(db, address, &network);
		if (!err) {
			// Extract the country code
			const char* country_code = loc_network_get_country_code(network);
			RETVAL = newSVpv(country_code, strlen(country_code));

			loc_network_unref(network);
		}
	OUTPUT:
		RETVAL

SV*
lookup_asn(db, address)
	struct loc_database* db;
	char* address;

	CODE:
		RETVAL = &PL_sv_undef;

		// Lookup network
		struct loc_network *network;
		int err = loc_database_lookup_from_string(db, address, &network);
		if (!err) {
			// Extract the ASN
			unsigned int as_number = loc_network_get_asn(network);
			if (as_number > 0) {
				RETVAL = newSViv(as_number);
			}

			loc_network_unref(network);
		}
	OUTPUT:
		RETVAL

void
DESTROY(db)
	struct loc_database* db;

	CODE:
		// Close database
		loc_database_unref(db);
