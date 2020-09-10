# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Location.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

# Where to find the test database.
my $testdb = $ENV{'database'};
my $keyfile = $ENV{'keyfile'};

use Test::More tests => 11;
BEGIN { use_ok('Location') };

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

# Address which should be used for database lookup.
my $address = "2a07:1c44:5800::1";

# Connect to the database.
my $db = &Location::init("$testdb");

# Verify
my $status = &Location::verify($db, $keyfile);
ok($status, "This database is valid");

my $vendor = &Location::get_vendor($db);
ok($vendor eq "IPFire Project", "Test 1 - Get Database Vendor");

my $license = &Location::get_license($db);
ok($license eq "CC", "Test 2 - Get Database license");

my $description = &Location::get_description($db);
ok($description eq "This is a geo location database", "Test 3 - Get Database Description");

my $country_code = &Location::lookup_country_code($db, $address);
ok($country_code eq "DE", "Test 4 - Lookup country code for $address");

$country_code = &Location::lookup_country_code($db, "1.1.1.1");
if(defined($country_code)) { fail("Test 5 - Lookup country code for address not in Database."); }

$country_code = &Location::lookup_country_code($db, "a.b.c.d");
if(defined($country_code)) { fail("Test 6 - Lookup country code for invalid address.") }

my $as_number = &Location::lookup_asn($db, $address);
ok($as_number eq "204867", "Test 7 - Lookup Autonomous System Number for $address.");

$as_number = &Location::lookup_asn($db, "1.1.1.1");
if(defined($as_number)) { fail("Test 8 - Lookup Autonomous System Number for address not in Database.") }

$as_number = &Location::lookup_asn($db, "a.b.c.d");
if(defined($as_number)) { fail("Test 9 - Lookup Autonomous System Number for invalid address.") }

my $as_name = &Location::get_as_name($db, "204867");
ok($as_name eq "Lightning Wire Labs GmbH", "Test 10 - Get name for AS204867.");

my @locations = &Location::database_countries($db);
ok(@locations != 0, "Test 11 - Get database countries.");

my $network_flag_anycast = &Location::lookup_network_has_flag($db, $address, "LOC_NETWORK_FLAG_ANYCAST");
ok($network_flag_anycast, "Network has Anycast flag.");

my $country_name = &Location::get_country_name($db, "DE");
ok($country_name eq "Germany", "Test 13 - Got country name: $country_name");
