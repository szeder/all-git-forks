#!/usr/bin/perl
use lib (split(/:/, $ENV{GITPERLLIB}));

use warnings;
use strict;

use Test::More;

use CGI qw(:standard);

# test source version
use lib $ENV{GITWEBLIBDIR} || "$ENV{GIT_BUILD_DIR}/gitweb/lib";

# ....................................................................

# prototypes must be known at compile time, otherwise they do not work
BEGIN { use_ok('GitwebCache::CacheOutput'); }

require_ok('GitwebCache::FileCacheWithLocking');
require_ok('GitwebCache::Capture::ToFile');

note("Using lib '$INC[0]'");
note("Testing '$INC{'GitwebCache/CacheOutput.pm'}'");
note("Testing '$INC{'GitwebCache/FileCacheWithLocking.pm'}'");
note("Testing '$INC{'GitwebCache/Capture/ToFile.pm'}'");


# Test setting up $cache and $capture
my ($cache, $capture);
subtest 'setup' => sub {
	$cache   = new_ok('GitwebCache::FileCacheWithLocking' => [], 'The $cache  ');
	$capture = new_ok('GitwebCache::Capture::ToFile'      => [], 'The $capture');

	done_testing();
};

# ......................................................................

# Prepare for testing cache_output
my $key = 'Key';
my $action_output = <<'EOF';
# This is data to be cached and shown
EOF
my $cached_output = <<"EOF";
$action_output# (version recovered from cache)
EOF
my $call_count = 0;
sub action {
	$call_count++;
	print $action_output;
}

my $die_output = <<"EOF";
$action_output# (died)
EOF
sub die_action {
	print $die_output;
	die "die_action\n";
}

# Catch output printed by cache_output
sub capture_output_of_cache_output {
	my ($code, @args) = @_;

	GitwebCache::Capture::ToFile->new()->capture(sub {
		cache_output($cache, $capture, $key, $code, @args);
	}, 'actual');

	return get_actual();
}

sub get_actual {
	open my $fh, '<', 'actual' or return;
	local $/ = undef;
	my $result = <$fh>;
	close $fh;
	return $result;
}

# use ->get_fh($key) interface
sub cache_get_fh {
	my ($cache, $key) = @_;

	my ($fh, $filename) = $cache->get_fh($key);
	return unless $fh;

	local $/ = undef;
	return <$fh>;
}

# use ->set_coderef_fh($key, $code_fh) to set $key to $value
sub cache_set_fh {
	my ($cache, $key, $value) = @_;

	$cache->set_coderef_fh($key, sub { print {$_[0]} $value });
	return $value;
}


# ......................................................................

# clean state
$cache->set_expires_in(-1);
$cache->remove($key);
my $test_data;

# first time (if there is no cache) generates cache entry
subtest '1st time (generate data)' => sub {
	$call_count = 0;
	$test_data = capture_output_of_cache_output(\&action);
	is($test_data,                 $action_output, 'action() output is printed');
	is(cache_get_fh($cache, $key), $action_output, 'action() output is saved in cache');
	cmp_ok($call_count, '==', 1, 'action() was called to generate data');

	done_testing();
};

# second time (if cache is set/valid) reads from cache
subtest '2nd time (retreve from cache)' => sub {
	cache_set_fh($cache, $key, $cached_output);
	$call_count = 0;
	$test_data = capture_output_of_cache_output(\&action);
	is(cache_get_fh($cache, $key), $cached_output, 'correct value is prepared in cache');
	is($test_data,                 $cached_output, 'output is printed from cache');
	cmp_ok($call_count, '==', 0, 'action() was not called');

	done_testing();
};

# caching output and error handling
subtest 'errors (exceptions) are not cached by default' => sub {
	$cache->remove($key);
	ok(!defined cache_get_fh($cache, $key), 'cache is prepared correctly (no data in cache)');
	eval {
		$test_data = capture_output_of_cache_output(\&die_action);
	};
	my $error = $@;
	$test_data = get_actual();
	is($test_data, $die_output,             'output of an error is printed');
	ok(!defined cache_get_fh($cache, $key), 'output is not captured and not cached');
	like($error, qr/^die_action\n/m,        'exception made it to outside, correctly');

	done_testing();
};

subtest 'errors are cached with -cache_errors => 1' => sub {
	$cache->remove($key);
	ok(!defined cache_get_fh($cache, $key), 'cache is prepared correctly (no data in cache)');
	eval {
		$test_data = capture_output_of_cache_output(\&die_action, -cache_errors => 1);
	};
	my $error = $@;
	$test_data = get_actual();
	is($test_data,                 $die_output, 'output of an error is printed');
	is(cache_get_fh($cache, $key), $die_output, 'output is captured and cached');
	ok(! $error, 'exception didn\'t made it to outside');
	diag($error) if $error;

	done_testing();
};


# caching HTTP output
subtest 'HTTP output' => sub {
	$cache->remove($key);
	$cache->set_expires_in(60);

	my $header =
		header(-status=>'200 OK', -type=>'text/plain', -charset => 'utf-8');
	my $data = "1234567890";
	$action_output = $header.$data;
	$test_data = capture_output_of_cache_output(\&action, '-http_output' => 1);

	$header =~ s/\r\n$//;
	my $length = do { use bytes; length($data); };
	like($test_data, qr/^\Q$header\E/, 'http: starts with provided http header');
	like($test_data, qr/\Q$data\E$/,   'http: ends with body (payload)');
	like($test_data, qr/^Expires: /m,  'http: some "Expires:" header added');
	like($test_data, qr/^Cache-Control: max-age=\d+\r\n/m,
	                                   'http: "Cache-Control:" with max-age added');
	like($test_data, qr/^Content-Length: $length\r\n/m,
	                                   'http: "Content-Length:" header with correct value'); 
};


done_testing();
__END__
