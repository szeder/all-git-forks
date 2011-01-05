# gitweb - simple web interface to track changes in git repositories
#
# (C) 2010, Jakub Narebski <jnareb@gmail.com>
#
# This program is licensed under the GPLv2

#
# Simple output capturing via redirecting STDOUT to given file.
#

# This is the same mechanism that Capture::Tiny uses, only simpler;
# we don't capture STDERR at all, we don't tee, we capture to
# explicitely provided file (or filehandle).

package GitwebCache::Capture::ToFile;

use strict;
use warnings;

use PerlIO;
use Symbol qw(qualify_to_ref);

# Constructor
sub new {
	my $class = shift;

	my $self = {};
	$self = bless($self, $class);

	return $self;
}

sub capture {
	my $self = shift;
	my $code = shift;

	$self->capture_start(@_); # pass rest of params
	eval { $code->(); 1; };
	my $exit_code = $?; # save this for later
	my $error = $@;     # save this for later

	my $got_out = $self->capture_stop();
	$? = $exit_code;
	die $error if $error;

	return $got_out;
}

# ----------------------------------------------------------------------

# Start capturing data (STDOUT)
sub capture_start {
	my $self = shift;
	my $to   = shift;

	# save copy of real STDOUT via duplicating it
	my @layers = PerlIO::get_layers(\*STDOUT);
	open $self->{'orig_stdout'}, ">&", \*STDOUT
		or die "Couldn't dup STDOUT for capture: $!";

	# close STDOUT, so that it isn't used anymode (to have it fd0)
	close STDOUT;

	$self->{'to'} = $to;
	my $fileno = fileno(qualify_to_ref($to)); 
	if (defined $fileno) {
		# if $to is filehandle, redirect
		open STDOUT, '>&', $fileno;
	} elsif (! ref($to)) {
		# if $to is name of file, open it
		open STDOUT, '>',  $to;
	}
	_relayer(\*STDOUT, \@layers);

	# started capturing
	$self->{'capturing'} = 1;
}

# Stop capturing data (required for die_error)
sub capture_stop {
	my $self = shift;

	# return if we didn't start capturing
	return unless delete $self->{'capturing'};

	# close capture file, and restore original STDOUT
	my @layers = PerlIO::get_layers(\*STDOUT);
	close STDOUT;
	open STDOUT, '>&', fileno($self->{'orig_stdout'});
	_relayer(\*STDOUT, \@layers);

	return exists $self->{'to'} ? $self->{'to'} : $self->{'data'};
}

# taken from Capture::Tiny by David Golden, Apache License 2.0
# with debugging stripped out
sub _relayer {
	my ($fh, $layers) = @_;

	my %seen = ( unix => 1, perlio => 1); # filter these out
	my @unique = grep { !$seen{$_}++ } @$layers;

	binmode($fh, join(":", ":raw", @unique));
}


1;
__END__
# end of package GitwebCache::Capture::ToFile
