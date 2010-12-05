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

use Exporter qw(import);
our @EXPORT      = qw(cache_output capture_stop);
our %EXPORT_TAGS = (all => [ @EXPORT ]);

# cache_output($cache, $capture, $key, $action_code);
#
# Attempts to get $key from $cache; if successful, prints the value.
# Otherwise, calls $action_code, capture its output using $capture,
# and use the captured output as the new value for $key in $cache,
# then print captured output.
#
# It is assumed that captured data is already converted and it is
# in ':raw' format (and thus restored in ':raw' from cache)

# default capture class (engine), if none provided
our $DEFAULT_CAPTURE_CLASS = 'GitwebCache::Capture::Simple';
sub cache_output {
	my ($cache, $capture, $key, $code) = @_;

	$capture = setup_capture($capture);

	# check if data is in the cache
	my $data = $cache->get($key);

	# capture and cache output, if there was nothing in the cache
	if (!defined $data) {
		$data = $capture->capture($code);
		$cache->set($key, $data) if defined $data;
	}

	# print cached data
	if (defined $data) {
		binmode STDOUT, ':raw';
		print $data;
	}

	return $data;
}

# capture_stop($cache, $capture);
#
# Stops capturing output; to be used in die_error, so that error pages
# are not cached (not captured and cached).
sub capture_stop {
	my ($cache, $capture) = @_;

	if (defined $capture) {
		return $capture->capture_stop();
	}
	return;
}

# ......................................................................
# helper subroutines

# setup capture engine
sub setup_capture {
	my $capture = shift;

	$capture ||= $DEFAULT_CAPTURE_CLASS;
	if (!ref($capture)) {
		eval "require $capture;" or die $@;
		$capture = $capture->new();
	}

	return $capture;
}

1;
__END__
# end of package GitwebCache::CacheOutput
