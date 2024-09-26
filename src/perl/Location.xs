#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <stdio.h>
#include <string.h>

#include <libloc/libloc.h>
#include <libloc/database.h>
#include <libloc/network.h>
#include <libloc/country.h>

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
bool
verify(db, keyfile)
	struct loc_database* db;
	char* keyfile;

	CODE:
		// Try to open the keyfile
		FILE* f = fopen(keyfile, "r");
		if (!f) {
			croak("Could not open keyfile %s: %s\n",
				keyfile, strerror(errno));
		}

		// Verify the database
		int status = loc_database_verify(db, f);
		if (status) {
			RETVAL = false;
			fclose(f);

			croak("Could not verify the database signature\n");
		}

		// Database was validated successfully
		RETVAL = true;

		// Close the keyfile
		fclose(f);
	OUTPUT:
		RETVAL

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

void
database_countries(db)
	struct loc_database* db;

	PPCODE:
		// Create Database enumerator
		struct loc_database_enumerator* enumerator;
		int err = loc_database_enumerator_new(&enumerator, db, LOC_DB_ENUMERATE_COUNTRIES, 0);

		if (err) {
			croak("Could not create a database enumerator\n");
		}

		// Init and enumerate first country.
		struct loc_country* country;
		err = loc_database_enumerator_next_country(enumerator, &country);
		if (err) {
			croak("Could not enumerate next country\n");
		}

		while (country) {
			// Extract the country code.
			const char* ccode = loc_country_get_code(country);

			// Push country code.
			XPUSHs(sv_2mortal(newSVpv(ccode, 2)));

			// Unref country pointer.
			loc_country_unref(country);

			// Enumerate next item.
			err = loc_database_enumerator_next_country(enumerator, &country);
			if (err) {
				croak("Could not enumerate next country\n");
			}
		}

		loc_database_enumerator_unref(enumerator);

#
# Lookup functions
#
SV*
lookup_country_code(db, address)
	struct loc_database* db;
	char* address;

	CODE:
		struct loc_network *network = NULL;
		const char* country_code = NULL;
		RETVAL = &PL_sv_undef;

		// Lookup network
		int err = loc_database_lookup_from_string(db, address, &network);
		if (err) {
			croak("Error fetching a network from the database\n");
		}

		// Extract the country code if we have found a network
		if (network) {
			country_code = loc_network_get_country_code(network);
			if (country_code)
				RETVAL = newSVpv(country_code, strlen(country_code));

			loc_network_unref(network);
		}
	OUTPUT:
		RETVAL

bool
lookup_network_has_flag(db, address, flag)
	struct loc_database* db;
	char* address;
	char* flag;

	CODE:
		struct loc_network *network = NULL;
		RETVAL = false;

		enum loc_network_flags iv = 0;

		if (strcmp("LOC_NETWORK_FLAG_ANONYMOUS_PROXY", flag) == 0)
			iv |= LOC_NETWORK_FLAG_ANONYMOUS_PROXY;
		else if (strcmp("LOC_NETWORK_FLAG_SATELLITE_PROVIDER", flag) == 0)
			iv |= LOC_NETWORK_FLAG_SATELLITE_PROVIDER;
		else if (strcmp("LOC_NETWORK_FLAG_ANYCAST", flag) == 0)
			iv |= LOC_NETWORK_FLAG_ANYCAST;
		else if (strcmp("LOC_NETWORK_FLAG_DROP", flag) == 0)
			iv |= LOC_NETWORK_FLAG_DROP;
		else
			croak("Invalid flag");

		// Lookup network
		int err = loc_database_lookup_from_string(db, address, &network);
		if (err) {
			croak("Error fetching a network from the database\n");
		}

		// Check if the network has the given flag
		if (network) {
			if (loc_network_has_flag(network, iv)) {
				RETVAL = true;
			}

			loc_network_unref(network);
		}

	OUTPUT:
		RETVAL

SV*
lookup_asn(db, address)
	struct loc_database* db;
	char* address;

	CODE:
		struct loc_network *network = NULL;
		RETVAL = &PL_sv_undef;

		// Lookup network
		int err = loc_database_lookup_from_string(db, address, &network);
		if (err) {
			croak("Error fetching a network from the database\n");
		}

		// Extract the ASN
		if (network) {
			unsigned int as_number = loc_network_get_asn(network);
			if (as_number > 0) {
				RETVAL = newSViv(as_number);
			}

			loc_network_unref(network);
		}
	OUTPUT:
		RETVAL

#
# Get functions
#
SV*
get_country_name(db, ccode)
	struct loc_database* db;
	char* ccode;

	CODE:
		RETVAL = &PL_sv_undef;

		// Lookup country code
		struct loc_country *country;
		int err = loc_database_get_country(db, &country, ccode);
		if(!err) {
			// Extract the name for the given country code.
			const char* country_name = loc_country_get_name(country);
			RETVAL = newSVpv(country_name, strlen(country_name));

			loc_country_unref(country);
		}

	OUTPUT:
		RETVAL

SV*
get_continent_code(db, ccode)
	struct loc_database* db;
	char* ccode;

	CODE:
		RETVAL = &PL_sv_undef;

		// Lookup country code
		struct loc_country *country;
		int err = loc_database_get_country(db, &country, ccode);
		if(!err) {
			//Extract the continent code for the given country code.
			const char* continent_code =  loc_country_get_continent_code(country);
			RETVAL = newSVpv(continent_code, strlen(continent_code));

			loc_country_unref(country);
		}

	OUTPUT:
		RETVAL

SV*
get_as_name(db, as_number)
	struct loc_database* db;
	unsigned int as_number;

	CODE:
		RETVAL = &PL_sv_undef;

		// Lookup AS.
		struct loc_as *as;
		int err = loc_database_get_as(db, &as, as_number);
		if(!err) {
			// Get the name of the given AS number.
			const char* as_name = loc_as_get_name(as);

			RETVAL = newSVpv(as_name, strlen(as_name));

			loc_as_unref(as);
		}

	OUTPUT:
		RETVAL

void
DESTROY(db)
	struct loc_database* db;

	CODE:
		// Close database
		loc_database_unref(db);
