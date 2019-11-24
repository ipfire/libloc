/*
	libloc - A library to determine the location of someone on the Internet

	Copyright (C) 2019 IPFire Development Team <info@ipfire.org>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
*/

#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include <resolv.h>
#include <string.h>
#include <time.h>

#include <loc/format.h>
#include <loc/private.h>
#include <loc/resolv.h>

static int parse_timestamp(const unsigned char* txt, time_t* t) {
    struct tm ts;

    // Parse timestamp
    char* p = strptime((const char*)txt, "%a, %d %b %Y %H:%M:%S GMT", &ts);

    // If the whole string has been parsed, we convert the parse value to time_t
    if (p && !*p) {
        *t = mktime(&ts);

    // Otherwise we reset t
    } else {
        *t = 0;
        return -1;
    }

    return 0;
}

LOC_EXPORT int loc_discover_latest_version(struct loc_ctx* ctx, const char* domain, time_t* t) {
    // Initialise the resolver
    int r = res_init();
    if (r) {
        ERROR(ctx, "res_init() failed\n");
        return r;
    }

    // Fall back to default domain
    if (!domain)
        domain = LOC_DATABASE_DOMAIN_LATEST(LOC_DATABASE_VERSION);

    unsigned char answer[PACKETSZ];
    int len;

    DEBUG(ctx, "Querying %s\n", domain);

    // Send a query
    if ((len = res_query(domain, C_IN, T_TXT, answer, sizeof(answer))) < 0 || len > PACKETSZ) {
        ERROR(ctx, "Could not query %s: \n", domain);

        return -1;
    }

    unsigned char* end = answer + len;
    unsigned char* payload = answer + sizeof(HEADER);

    // Expand domain name
    char host[128];
    if ((len = dn_expand(answer, end, payload, host, sizeof(host))) < 0) {
        ERROR(ctx, "dn_expand() failed\n");
        return -1;
    }

    // Payload starts after hostname
    payload += len;

    if (payload > end - 4) {
        ERROR(ctx, "DNS reply too short\n");
        return -1;
    }

    int type;
    GETSHORT(type, payload);
    if (type != T_TXT) {
        ERROR(ctx, "DNS reply of unexpected type: %d\n", type);
        return -1;
    }

    // Skip class
    payload += INT16SZ;

    // Walk through CNAMEs
    unsigned int size = 0;
    int ttl;
    do {
        payload += size;

        if ((len = dn_expand(answer, end, payload, host, sizeof(host))) < 0) {
            ERROR(ctx, "dn_expand() failed\n");
            return -1;
        }

        payload += len;

        if (payload > end - 10) {
            ERROR(ctx, "DNS reply too short\n");
            return -1;
        }

        // Skip type, class, ttl
        GETSHORT(type, payload);
        payload += INT16SZ;
        GETLONG(ttl, payload);

        // Read size
        GETSHORT(size, payload);
        if (payload + size < answer || payload + size > end) {
            ERROR(ctx, "DNS RR overflow\n");
            return -1;
        }
    } while (type == T_CNAME);

    if (type != T_TXT) {
        ERROR(ctx, "Not a TXT record\n");
        return -1;
    }

    if (!size || (len = *payload) >= size || !len) {
        ERROR(ctx, "Broken TXT record (len = %d, size = %d)\n", len, size);
        return -1;
    }

    // Get start of the string
    unsigned char* txt = payload + 1;
    txt[len] = '\0';

    DEBUG(ctx, "Resolved to: %s\n", txt);

    // Parse timestamp
    r = parse_timestamp(txt, t);

    return r;
}
