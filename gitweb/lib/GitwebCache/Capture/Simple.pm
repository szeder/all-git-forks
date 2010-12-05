# gitweb - simple web interface to track changes in git repositories
#
# (C) 2010, Jakub Narebski <jnareb@gmail.com>
#
# This program is licensed under the GPLv2

#
# Simple output capturing via redirecting STDOUT to in-memory file.
#

# This is the same mechanism that Capture::Tiny uses, only simpler;
# we don't capture STDERR at all, we don't tee, we don't support
# capturing output of external commands.

package GitwebCache::Capture::Simple;

use strict;
use warnings;

use PerlIO;

# Constructor
sub new {
	my $class = shift;

	my $self = {};
	$self = bless($self, $class);

	return $self;
}

sub capture {
	my ($self, $code) = @_;

	$self->capture_start();
	$code->();
	return $self->capture_stop();
}

# ----------------------------------------------------------------------

# Start capturing data (STDOUT)
sub capture_start {
	my $self = shift;

	# save copy of real STDOUT via duplicating it
	my @layers = PerlIO::get_layers(\*STDOUT);
	open $self->{'orig_stdout'}, ">&", \*STDOUT
		or die "Couldn't dup STDOUT for capture: $!";

	# close STDOUT, so that it isn't used anymode (to have it fd0)
	close STDOUT;

	# reopen STDOUT as in-memory file
	$self->{'data'} = '';
	unless (open STDOUT, '>', \$self->{'data'}) {
		open STDOUT, '>&', fileno($self->{'orig_stdout'});
		die "Couldn't reopen STDOUT as in-memory file for capture: $!";
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

	# close in-memory file, and restore original STDOUT
	my @layers = PerlIO::get_layers(\*STDOUT);
	close STDOUT;
	open STDOUT, '>&', fileno($self->{'orig_stdout'});
	_relayer(\*STDOUT, \@layers);

	return $self->{'data'};
}

# taken from Capture::Tiny by David Golden, Apache License 2.0
# with debugging stripped out, and added filtering out 'scalar' layer
sub _relayer {
	my ($fh, $layers) = @_;

	my %seen = ( unix => 1, perlio => 1, scalar => 1 ); # filter these out
	my @unique = grep { !$seen{$_}++ } @$layers;

	binmode($fh, join(":", ":raw", @unique));
}


1;
__END__
# end of package GitwebCache::Capture::Simple
