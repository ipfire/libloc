#!/usr/bin/python3

import location

w = location.Writer()

# Set the vendor
w.vendor = "IPFire Project"

# Set a description
w.description = "This is a geo location database"

# Set a license
w.license = "CC"

# Add an AS
a = w.add_as(204867)
a.name = "Lightning Wire Labs GmbH"

print(a)

# Add a network
n = w.add_network("2a07:1c44:5800::/40")
n.country_code = "DE"
n.asn = a.number

print(n)

# Write the database to disk
w.write("test.db")
