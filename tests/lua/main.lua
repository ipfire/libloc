#!/usr/bin/lua
--[[###########################################################################
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
############################################################################--]]

luaunit = require("luaunit")

ENV_TEST_DATABASE = os.getenv("TEST_DATABASE")

function test_load()
	-- Try loading the module
	location = require("location")

	-- Print the version
	print(location.version())
end

function test_open_database()
	location = require("location")

	-- Open the database
	db = location.Database.open(ENV_TEST_DATABASE)
end

function test_lookup()
	location = require("location")

	-- Open the database
	db = location.Database.open(ENV_TEST_DATABASE)

	-- Perform a lookup
	network = db:lookup("81.3.27.32")

	luaunit.assertEquals(network:get_family(), 2) -- AF_INET
	luaunit.assertEquals(network:get_country_code(), "DE")
	luaunit.assertEquals(network:get_asn(), 24679)
end

function test_network()
	location = require("location")

	n1 = location.Network.new("10.0.0.0/8")

	-- The ASN should be nul
	luaunit.assertNil(n1:get_asn())

	-- The family should be IPv4
	luaunit.assertEquals(n1:get_family(), 2) -- AF_INET

	-- The country code should be empty
	luaunit.assertNil(n1:get_country_code())
end

function test_country()
	location = require("location")

	c1 = location.Country.new("DE")
	luaunit.assertEquals(c1:get_code(), "DE")

	c2 = location.Country.new("GB")
	luaunit.assertNotEquals(c1, c2)

	c1 = nil
	c2 = nil
end

-- This test is not very deterministic but should help to test the GC methods
function test_gc()
	print("GC: " .. collectgarbage("collect"))
end

os.exit(luaunit.LuaUnit.run())
