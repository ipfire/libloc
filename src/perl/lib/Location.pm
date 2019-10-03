package Location;

use 5.028001;
use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Location ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw() ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw();

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Location', $VERSION);

# Preloaded methods go here.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

Location - Provides a simple interface to libloc.

=head1 SYNOPSIS

  use Location;

=head1 DESCRIPTION

Location is a simple interface to libloc - A library to determine someones
location on the Internet. (https://git.ipfire.org/?p=location/libloc.git;a=summary)

=head2 EXPORT

None by default.

=head1 SEE ALSO

https://git.ipfire.org/?p=location/libloc.git;a=summary

=head1 AUTHOR

Stefan Schantl, stefan.schantl@ipfire.org

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2019 by Stefan Schantl

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.28.1 or,
at your option, any later version of Perl 5 you may have available.


=cut
