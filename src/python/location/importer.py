###############################################################################
#                                                                             #
# libloc - A library to determine the location of someone on the Internet     #
#                                                                             #
# Copyright (C) 2020 IPFire Development Team <info@ipfire.org>                #
#                                                                             #
# This library is free software; you can redistribute it and/or               #
# modify it under the terms of the GNU Lesser General Public                  #
# License as published by the Free Software Foundation; either                #
# version 2.1 of the License, or (at your option) any later version.          #
#                                                                             #
# This library is distributed in the hope that it will be useful,             #
# but WITHOUT ANY WARRANTY; without even the implied warranty of              #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU           #
# Lesser General Public License for more details.                             #
#                                                                             #
###############################################################################

import gzip
import logging
import tempfile
import urllib.request

# Initialise logging
log = logging.getLogger("location.importer")
log.propagate = 1

class Downloader(object):
	def __init__(self):
		self.proxy = None

	def set_proxy(self, url):
		"""
			Sets a HTTP proxy that is used to perform all requests
		"""
		log.info("Using proxy %s" % url)
		self.proxy = url

	def retrieve(self, url, **kwargs):
		"""
			This method will fetch the content at the given URL
			and will return a file-object to a temporary file.

			If the content was compressed, it will be decompressed on the fly.
		"""
		# Open a temporary file to buffer the downloaded content
		t = tempfile.SpooledTemporaryFile(max_size=100 * 1024 * 1024)

		# Create a new request
		req = urllib.request.Request(url, **kwargs)

		# Configure proxy
		if self.proxy:
			req.set_proxy(self.proxy, "http")

		log.info("Retrieving %s..." % req.full_url)

		# Send request
		res = urllib.request.urlopen(req)

		# Log the response headers
		log.debug("Response Headers:")
		for header in res.headers:
			log.debug("	%s: %s" % (header, res.headers[header]))

		# Write the payload to the temporary file
		with res as f:
			while True:
				buf = f.read(65536)
				if not buf:
					break

				t.write(buf)

		# Rewind the temporary file
		t.seek(0)

		gzip_compressed = False

		# Fetch the content type
		content_type = res.headers.get("Content-Type")

		# Decompress any gzipped response on the fly
		if content_type in ("application/x-gzip", "application/gzip"):
			gzip_compressed = True

		# Check for the gzip magic in case web servers send a different MIME type
		elif t.read(2) == b"\x1f\x8b":
			gzip_compressed = True

		# Reset again
		t.seek(0)

		# Decompress the temporary file
		if gzip_compressed:
			log.debug("Gzip compression detected")

			t = gzip.GzipFile(fileobj=t, mode="rb")

		# Return the temporary file handle
		return t
