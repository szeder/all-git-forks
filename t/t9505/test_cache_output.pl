#!/usr/bin/perl
use lib (split(/:/, $ENV{GITPERLLIB}));

use warnings;
use strict;

use Test::More;
use Capture::Tiny qw(capture);

# test source version
use lib $ENV{GITWEBLIBDIR} || "$ENV{GIT_BUILD_DIR}/gitweb/lib";

# ....................................................................

# prototypes must be known at compile time, otherwise they do not work
BEGIN { use_ok('GitwebCache::CacheOutput'); }

require_ok('GitwebCache::SimpleFileCache');
require_ok('GitwebCache::Capture::Simple');

diag("Using lib '$INC[0]'");
diag("Testing '$INC{'GitwebCache/CacheOutput.pm'}'");
diag("Testing '$INC{'GitwebCache/SimpleFileCache.pm'}'");
diag("Testing '$INC{'GitwebCache/Capture/Simple.pm'}'");


# Test setting up $cache and $capture
my $cache   = new_ok('GitwebCache::SimpleFileCache' => [], 'The $cache  ');
my $capture = new_ok('GitwebCache::Capture::Simple' => [], 'The $capture');

# ......................................................................

# Prepare for testing cache_output
my $key = 'Key';
my $action_output = <<'EOF';
# This is data to be cached and shown
EOF
my $cached_output = <<"EOF";
$action_output# (version recovered from cache)
EOF
sub action {
	print $action_output;
}

my $no_capture_output = <<"EOF";
$action_output# (no capture)
EOF
sub no_capture {
	capture_stop($cache, $capture);
	print $no_capture_output;
}

# Catch output printed by cache_fetch
# (only for 'print <sth>' and 'printf <sth>')
sub capture_output_of_cache_output {
	my $code = shift;

	my ($stdout, $stderr) = capture {
		cache_output($cache, $capture, $key, $code);
	};
	print STDERR $stderr;
	return $stdout;
}

# clean state
$cache->set_expires_in(-1);
$cache->remove($key);
my $test_data;

# first time (if there is no cache) generates cache entry
$test_data = capture_output_of_cache_output(\&action);
is($test_data, $action_output,        'action output is printed (generated)');
is($cache->get($key), $action_output, 'action output is saved in cache (generated)');

# second time (if cache is set/valid) reads from cache
$cache->set($key, $cached_output);
$test_data = capture_output_of_cache_output(\&action);
is($test_data, $cached_output,        'action output is printed (from cache)');

# test using capture_stop
$cache->remove($key);
$test_data = capture_output_of_cache_output(\&no_capture);
is($test_data, $no_capture_output,    'no_capture output is printed (generated)');
ok(! $cache->get($key),               'no_capture output is not captured and not cached');

done_testing();
