use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'Location',
    VERSION_FROM      => 'lib/Location.pm',
    PREREQ_PM         => {},
    ABSTRACT_FROM     => 'lib/Location.pm',
    AUTHOR            => 'Stefan Schantl <stefan.schantl@ipfire.org>',
    LICENSE           => 'lgpl',
    LIBS              => ['-lloc'],
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I. -I../../',
	# Un-comment this if you add C files to link with later:
    # OBJECT            => '$(O_FILES)', # link all the C files too
);
