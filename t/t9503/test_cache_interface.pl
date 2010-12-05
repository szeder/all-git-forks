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

done_testing();
