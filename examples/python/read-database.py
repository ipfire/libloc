#!/usr/bin/python3

import location

# Open the database
d = location.Database("test.db")
print(d)

# Try to get information about AS123
a = d.get_as(123)
print(a)

# Try to get information about AS204867
a = d.get_as(204867)
print(a)

# Search for an IP address in the database
n = d.lookup("8.8.8.8")
print(n)
