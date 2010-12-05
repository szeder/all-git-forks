#!/usr/bin/perl
use lib (split(/:/, $ENV{GITPERLLIB}));

use warnings;
use strict;

use Test::More;

# test source version
use lib $ENV{GITWEBLIBDIR} || "$ENV{GIT_BUILD_DIR}/gitweb/lib";


# Test creating a cache
#
BEGIN { use_ok('GitwebCache::SimpleFileCache'); }
diag("Using lib '$INC[0]'");
diag("Testing '$INC{'GitwebCache/SimpleFileCache.pm'}'");

my $cache = new_ok('GitwebCache::SimpleFileCache');

# Test that default values are defined
#
ok(defined $GitwebCache::SimpleFileCache::DEFAULT_CACHE_ROOT,
	'$DEFAULT_CACHE_ROOT defined');
ok(defined $GitwebCache::SimpleFileCache::DEFAULT_CACHE_DEPTH,
	'$DEFAULT_CACHE_DEPTH defined');

# Test accessors and default values for cache
#
SKIP: {
	skip 'default values not defined', 3
		unless ($GitwebCache::SimpleFileCache::DEFAULT_CACHE_ROOT &&
		        $GitwebCache::SimpleFileCache::DEFAULT_CACHE_DEPTH);

	is($cache->get_namespace(), '', "default namespace is ''");
	cmp_ok($cache->get_root(),  'eq', $GitwebCache::SimpleFileCache::DEFAULT_CACHE_ROOT,
		"default cache root is '$GitwebCache::SimpleFileCache::DEFAULT_CACHE_ROOT'");
	cmp_ok($cache->get_depth(), '==', $GitwebCache::SimpleFileCache::DEFAULT_CACHE_DEPTH,
		"default cache depth is $GitwebCache::SimpleFileCache::DEFAULT_CACHE_DEPTH");
}

# Test the getting, setting, and removal of a cached value
# (Cache::Cache interface)
#
my $key = 'Test Key';
my $value = 'Test Value';

subtest 'Cache::Cache interface' => sub {
	foreach my $method (qw(get set remove)) {
		can_ok($cache, $method);
	}

	$cache->set($key, $value);
	cmp_ok($cache->get_size($key), '>', 0, 'get_size after set, is greater than 0');
	is($cache->get($key), $value,          'get after set, returns cached value');
	$cache->remove($key);
	ok(!defined($cache->get($key)),        'get after remove, is undefined');

	eval { $cache->remove('Not-Existent Key'); };
	ok(!$@,                                'remove on non-existent key doesn\'t die');
	diag($@) if $@;

	done_testing();
};

# Test the getting and setting of a cached value
# (CHI interface)
#
my $call_count = 0;
sub get_value {
	$call_count++;
	return $value;
}
subtest 'CHI interface' => sub {
	can_ok($cache, qw(compute));

	is($cache->compute($key, \&get_value), $value, "compute 1st time (set) returns '$value'");
	is($cache->compute($key, \&get_value), $value, "compute 2nd time (get) returns '$value'");
	is($cache->compute($key, \&get_value), $value, "compute 3rd time (get) returns '$value'");
	cmp_ok($call_count, '==', 1, 'get_value() is called once from compute');

	done_testing();
};

# Test the getting and setting of a cached value
# (compute_fh interface)
#
$call_count = 0;
sub write_value {
	my $fh = shift;
	$call_count++;
	print {$fh} $value;
}
sub compute_fh_output {
	my ($cache, $key, $code_fh) = @_;

	my ($fh, $filename) = $cache->compute_fh($key, $code_fh);

	local $/ = undef;
	return <$fh>;
}
subtest 'compute_fh interface' => sub {
	can_ok($cache, qw(compute_fh));

	$cache->remove($key);
	is(compute_fh_output($cache, $key, \&write_value), $value,
	   "compute_fh 1st time (set) returns '$value'");
	is(compute_fh_output($cache, $key, \&write_value), $value,
	   "compute_fh 2nd time (get) returns '$value'");
	is(compute_fh_output($cache, $key, \&write_value), $value,
	   "compute_fh 3rd time (get) returns '$value'");
	cmp_ok($call_count, '==', 1, 'write_value() is called once from compute_fh');

	done_testing();
};

# Test cache expiration
#
subtest 'cache expiration' => sub {
	$cache->set_expires_in(60*60*24); # set expire time to 1 day
	cmp_ok($cache->get_expires_in(), '>', 0, '"expires in" is greater than 0');
	is($cache->get($key), $value,            'get returns cached value (not expired in 1d)');

	$cache->set_expires_in(-1); # set expire time to never expire
	is($cache->get_expires_in(), -1,         '"expires in" is set to never (-1)');
	is($cache->get($key), $value,            'get returns cached value (not expired)');

	$cache->set_expires_in(0);
	is($cache->get_expires_in(),  0,         '"expires in" is set to now (0)');
	$cache->set($key, $value);
	ok(!defined($cache->get($key)),          'cache is expired');

	done_testing();
};

# Test assertions for adaptive cache expiration
#
my $load = 0.0;
sub load { return $load; }
my $expires_min = 10;
my $expires_max = 30;
$cache->set_expires_in(-1);
$cache->set_expires_min($expires_min);
$cache->set_expires_max($expires_max);
$cache->set_check_load(\&load);
subtest 'adaptive cache expiration' => sub {
	cmp_ok($cache->get_expires_min(), '==', $expires_min,
	       '"expires min" set correctly');
	cmp_ok($cache->get_expires_max(), '==', $expires_max,
	       '"expires max" set correctly');

	$load = 0.0;
	cmp_ok($cache->get_expires_in(), '>=', $expires_min,
	       '"expires in" bound from down for load=0');
	cmp_ok($cache->get_expires_in(), '<=', $expires_max,
	       '"expires in" bound from up   for load=0');
	
	$load = 1_000;
	cmp_ok($cache->get_expires_in(), '>=', $expires_min,
	       '"expires in" bound from down for heavy load');
	cmp_ok($cache->get_expires_in(), '<=', $expires_max,
	       '"expires in" bound from up   for heavy load');

	done_testing();
};

$cache->set_expires_in(-1);

done_testing();
