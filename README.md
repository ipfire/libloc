# **_`libloc`_** - IP Address Location

[Home](https://www.ipfire.org/location)

`libloc` is a library for fast and efficient IP address location.

It offers:

- **The Fastest Lookups**: O(1) lookup time for IP addresses using a binary tree structure.
- **Low Memory Footprint**: The database is packed in a very efficient format.
- **Security**: Integrated signature verification for data integrity.
- **Maintainability**: Automatic updates.
- **Standalone**: No external dependencies, easy to integrate.

`libloc` is ideal for:

- Firewalls
- Intrusion Prevention/Detection Systems (IPS/IDS)
- Web Applications
- Network Management Tools

The publicly available daily updated database stores information about:

- The entire IPv6 and IPv4 Internet
- Autonomous System Information including names
- Country Codes, Names and Continent Codes

## Command Line

`libloc` comes with a command line tool which makes it easy to test the library or
integrate it into your shell scripts. location(8) knows a couple of commands to retrieve
country or Autonomous System of an IP address and can generate lists of networks to be
imported into other software.

`location (8)` is versatile and very easy to use.

## Language Bindings

`libloc` itself is written in C. There are bindings for the following languages available:

- Python 3
- Lua
- Perl

`libloc` comes with native Python bindings which are used by its main command-line tool
location. They are the most advanced bindings as they support reading from the database
as well as writing to it.
