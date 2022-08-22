#!/usr/bin/python3
###############################################################################
#                                                                             #
# libloc - A library to determine the location of someone on the Internet     #
#                                                                             #
# Copyright (C) 2022 IPFire Development Team <info@ipfire.org>                #
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

import location
import os
import unittest

TEST_DATA_DIR = os.environ["TEST_DATA_DIR"]

class Test(unittest.TestCase):
	def setUp(self):
		path = os.path.join(TEST_DATA_DIR, "location-2022-03-30.db")

		# Load the database
		self.db = location.Database(path)

	def test_fetch_network(self):
		n = self.db.lookup("81.3.27.38")
		self.assertIsInstance(n, location.Network)

	def test_fetch_network_invalid(self):
		with self.assertRaises(ValueError):
			self.db.lookup("XXX")


if __name__ == "__main__":
	unittest.main()
