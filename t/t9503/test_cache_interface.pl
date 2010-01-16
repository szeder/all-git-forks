#!/usr/bin/perl
use lib (split(/:/, $ENV{GITPERLLIB}));

use warnings;
use strict;

use Test::More;

# test source version; there is no installation target for gitweb
my $cache_pm = "$ENV{TEST_DIRECTORY}/../gitweb/cache.pm";

unless (-f "$cache_pm") {
	plan skip_all => "gitweb/cache.pm not found";
}

# it is currently not a proper Perl module, so we use 'do FILE'
#ok(eval { do "$cache_pm"; 1 or die $!; }, "loading gitweb/cache.pm");
my $return = do "$cache_pm";
ok(!$@,              "parse gitweb/cache.pm")
	or diag("parse error:\n", $@);
ok(defined $return,  "do    gitweb/cache.pm");
ok($return,          "run   gitweb/cache.pm");
# instead of: BEGIN { use_ok('GitwebCache::SimpleFileCache') }

# Test creating a cache
#
my $cache = new_ok('GitwebCache::SimpleFileCache',
	[ { 'cache_root' => 'cache', 'cache_depth' => 2 } ]);

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
	is($cache->get_root(), $GitwebCache::SimpleFileCache::DEFAULT_CACHE_ROOT,
		"default cache root is '$GitwebCache::SimpleFileCache::DEFAULT_CACHE_ROOT'");
	cmp_ok($cache->get_depth(), '==', $GitwebCache::SimpleFileCache::DEFAULT_CACHE_DEPTH,
		"default cache depth is $GitwebCache::SimpleFileCache::DEFAULT_CACHE_DEPTH");
}

# Test the getting, setting, and removal of a cached value
# (Cache::Cache interface)
#
my $key = 'Test Key';
my $value = 'Test Value';
can_ok($cache, qw(get set remove));
#ok(!defined($cache->get($key)),        'get before set')
#	or diag("get returned '", $cache->get($key), "' for $key");
$cache->set($key, $value);
is($cache->get($key), $value,          'get after set, returns cached value');
$cache->remove($key);
ok(!defined($cache->get($key)),        'get after remove, is undefined');

# Test the getting and setting of a cached value
# (CHI interface)
#
my $call_count = 0;
sub get_value {
	$call_count++;
	return $value;
}
can_ok($cache, qw(compute));
is($cache->compute($key, \&get_value), $value, 'compute 1st time (set)');
is($cache->compute($key, \&get_value), $value, 'compute 2nd time (get)');
is($cache->compute($key, \&get_value), $value, 'compute 3rd time (get)');
cmp_ok($call_count, '==', 1, 'get_value() is called once');

# Test cache expiration for 'expire now'
#
$cache->set_expires_in(0);
is($cache->get_expires_in(), 0,        '"expires in" is set to now (0)');
$cache->set($key, $value);
ok(!defined($cache->get($key)),        'cache is expired');


done_testing();
