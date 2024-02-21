#!/usr/bin/python3
###############################################################################
#                                                                             #
# libloc - A library to determine the location of someone on the Internet     #
#                                                                             #
# Copyright (C) 2024 IPFire Development Team <info@ipfire.org>                #
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
import tempfile
import unittest

class Test(unittest.TestCase):
	def test_dudup_simple(self):
		"""
			Creates a couple of redundant networks and expects fewer being written
		"""
		with tempfile.NamedTemporaryFile() as f:
			w = location.Writer()

			# Add 10.0.0.0/8
			n = w.add_network("10.0.0.0/8")

			# Add 10.0.0.0/16
			w.add_network("10.0.0.0/16")

			# Add 10.0.0.0/24
			w.add_network("10.0.0.0/24")

			# Write file
			w.write(f.name)

			# Re-open the database
			db = location.Database(f.name)

			for i, network in enumerate(db.networks):
				# The only network we should see is 10.0.0.0/8
				self.assertEqual(network, n)

				self.assertTrue(i == 0)


if __name__ == "__main__":
	unittest.main()
