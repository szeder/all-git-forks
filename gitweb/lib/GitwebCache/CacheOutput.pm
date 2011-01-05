# gitweb - simple web interface to track changes in git repositories
#
# (C) 2010, Jakub Narebski <jnareb@gmail.com>
# (C) 2006, John 'Warthog9' Hawley <warthog19@eaglescrag.net>
#
# This program is licensed under the GPLv2

#
# Capturing and caching (gitweb) output
#

# Capture output, save it in cache and print it, or retrieve it from
# cache and print it.

package GitwebCache::CacheOutput;

use strict;
use warnings;

use File::Copy qw();
use Symbol qw(qualify_to_ref);
use CGI::Util qw(expires);

use Exporter qw(import);
our @EXPORT      = qw(cache_output);
our %EXPORT_TAGS = (all => [ @EXPORT ]);

# cache_output($cache, $capture, $key, $action_code, [ option => value ]);
#
# Attempts to get $key from $cache; if successful, prints the value.
# Otherwise, calls $action_code, capture its output using $capture,
# and use the captured output as the new value for $key in $cache,
# then print captured output.
#
# It is assumed that captured data is already converted and it is
# in ':raw' format (and thus restored in ':raw' from cache)
#
# Supported options:
# * -cache_errors => undef|0|1  - whether error output should be cached,
#                                 undef means cache if we are in detached process
sub cache_output {
	my ($cache, $capture, $key, $code, %opts) = @_;


	my $pid = $$;
	my ($fh, $filename);
	my ($capture_fh, $capture_filename);
	eval { # this `eval` is to catch rethrown error, so we can print captured output
		($fh, $filename) = $cache->compute_fh($key, sub {
			($capture_fh, $capture_filename) = @_;

			if (!defined $opts{'-cache_errors'}) {
				# cache errors if we are in detached process
				$opts{'-cache_errors'} = ($$ != $pid && getppid() != $pid);
			}

			# this `eval` is to be able to cache error output (up till 'die')
			eval { $capture->capture($code, $capture_fh); };

			# note that $cache can catch this error itself (like e.g. CHI);
			# use "die"-ing error handler to rethrow this exception to outside
			die $@ if ($@ && ! $opts{'-cache_errors'});
		});
	};
	my $error = $@;

	# if an exception was rethrown, and not caught by caching engine (by $cache)
	# then ->compute_fh will not set $fh nor $filename; use those used for capture
	if (!defined $fh) {
		$filename ||= $capture_filename;
	}

	if ($opts{'-http_output'}) {
		# we need filehandle; filename is not enough
		open $fh, '<', $filename unless defined $fh;

		# get HTTP headers first
		my (@headers, %norm_headers);
		while (my $line = <$fh>) {
			last if $line eq "\r\n";
			push @headers, $line;
			if ($line =~ /^([^:]+:)\s+(.*)$/) {
				(my $header = lc($1)) =~ s/_/-/;
				$norm_headers{$header} = $2;
			}
		}
		print join('', @headers);

		# extra headers
		if (!exists $norm_headers{lc('Expires')} &&
		    !exists $norm_headers{lc('Cache-Control')}) {
			my $expires_in = $cache->expires_in($key);
			print "Expires: " . expires($expires_in, 'http')."\r\n".
			      "Cache-Control: max-age=$expires_in\r\n";
		}
		if (!exists $norm_headers{lc('Content-Length')}) {
			my $length = (-s $fh) - (tell $fh);
			print "Content-Length: $length\r\n" if $length;
		}

		print "\r\n"; # separates headers from body
	}

	if (defined $fh || defined $filename) {
		# set binmode only if $fh is defined (is a filehandle)
		# File::Copy::copy opens files given by filename in binary mode
		binmode $fh,    ':raw' if (defined $fh);
		binmode STDOUT, ':raw';
		File::Copy::copy($fh || $filename, \*STDOUT);
	}

	# rethrow error if captured in outer `eval` (i.e. no -cache_errors),
	# removing temporary file (exception thrown out of cache)
	if ($error) {
		unlink $capture_filename
			if (defined $capture_filename && -e $capture_filename);
		die $error;
	}
	return;
}

1;
__END__
# end of package GitwebCache::CacheOutput
