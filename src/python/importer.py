#!/usr/bin/python3
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

	def request(self, url, data=None):
		req = urllib.request.Request(url, data=data)

		# Configure proxy
		if self.proxy:
			req.set_proxy(self.proxy, "http")

		return DownloaderContext(self, req)


class DownloaderContext(object):
	def __init__(self, downloader, request):
		self.downloader = downloader
		self.request = request

		# Save the response object
		self.response = None

	def __enter__(self):
		log.info("Retrieving %s..." % self.request.full_url)

		# Send request
		self.response = urllib.request.urlopen(self.request)

		# Log the response headers
		log.debug("Response Headers:")
		for header in self.headers:
			log.debug("	%s: %s" % (header, self.get_header(header)))

		return self

	def __exit__(self, type, value, traceback):
		pass

	def __iter__(self):
		"""
			Makes the object iterable by going through each block
		"""
		# Store body
		body = self.body

		while True:
			line = body.readline()
			if not line:
				break

			# Decode the line
			line = line.decode()

			# Strip the ending
			yield line.rstrip()

	@property
	def headers(self):
		if self.response:
			return self.response.headers

	def get_header(self, name):
		if self.headers:
			return self.headers.get(name)

	@property
	def body(self):
		"""
			Returns a file-like object with the decoded content
			of the response.
		"""
		# Return the response by default
		return self.response
