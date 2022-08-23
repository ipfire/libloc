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

#ifndef LIBLOC_COMPAT_H
#define LIBLOC_COMPAT_H

#ifdef __APPLE__
/* Hacks to make this library compile on Mac OS X */

#include <libkern/OSByteOrder.h>
#define be16toh(x) OSSwapBigToHostInt16(x)
#define htobe16(x) OSSwapHostToBigInt16(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define htobe64(x) OSSwapHostToBigInt64(x)

#ifndef s6_addr16
#  define s6_addr16 __u6_addr.__u6_addr16
#endif
#ifndef s6_addr32
#  define s6_addr32 __u6_addr.__u6_addr32
#endif

#ifndef reallocarray
#  define reallocarray(ptr, nmemb, size) realloc(ptr, nmemb * size)
#endif

#endif

#endif
