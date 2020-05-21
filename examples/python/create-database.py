#!/usr/bin/python3

import _location as location
import os
import sys

ABS_SRCDIR = os.environ.get("ABS_SRCDIR", ".")

private_key_path = os.path.join(ABS_SRCDIR, "examples/private-key.pem")

with open(private_key_path, "r") as pkey:
    w = location.Writer(pkey)

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
    for f in sys.argv[1:]:
        w.write(f)
