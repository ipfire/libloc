# Before 'make install' is performed this script should be runnable with
# 'make test'. After 'make install' it should work as 'perl Location.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

# Where to find the test database.
my $testdb = $ENV{'database'};

use Test::More tests => 5;
BEGIN { use_ok('Location') };

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

# Address which should be used for database lookup.
my $address = "2a07:1c44:5800::1";

# Connect to the database.
my $db = &Location::init("$testdb");

my $vendor = &Location::get_vendor($db);
ok($vendor eq "IPFire Project", "Test 1 - Get Database Vendor");

my $license = &Location::get_license($db);
ok($license eq "CC", "Test 2 - Get Database license");

my $description = &Location::get_description($db);
ok($description eq "This is a geo location database", "Test 3 - Get Database Description");

my $country_code = &Location::lookup_country_code($db, $address);
ok($country_code eq "DE", "Test 4 - Lookup country code for $address");
