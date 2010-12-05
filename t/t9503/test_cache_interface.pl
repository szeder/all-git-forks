#!/usr/bin/perl
use lib (split(/:/, $ENV{GITPERLLIB}));

use warnings;
use strict;

use POSIX qw(dup2);
use Fcntl qw(:DEFAULT);
use IO::Handle;
use IO::Select;
use IO::Pipe;

use Test::More;

# test source version
use lib $ENV{GITWEBLIBDIR} || "$ENV{GIT_BUILD_DIR}/gitweb/lib";


# Test creating a cache
#
BEGIN { use_ok('GitwebCache::FileCacheWithLocking'); }
diag("Using lib '$INC[0]'");
diag("Testing '$INC{'GitwebCache/FileCacheWithLocking.pm'}'");

my $cache = new_ok('GitwebCache::FileCacheWithLocking', [ {
	'max_lifetime' => 0, # turn it off
	'background_cache' => 0,
} ]);
isa_ok($cache, 'GitwebCache::SimpleFileCache');

# compute can fork, don't generate zombies
#local $SIG{CHLD} = 'IGNORE';

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

# ----------------------------------------------------------------------
# CACHE API

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
subtest 'compute_fh interface' => sub {
	can_ok($cache, qw(compute_fh));

	$cache->remove($key);
	is(cache_compute_fh($cache, $key, \&write_value), $value,
	   "compute_fh 1st time (set) returns '$value'");
	is(cache_compute_fh($cache, $key, \&write_value), $value,
	   "compute_fh 2nd time (get) returns '$value'");
	is(cache_compute_fh($cache, $key, \&write_value), $value,
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

# ----------------------------------------------------------------------
# CONCURRENT ACCESS
sub parallel_run (&); # forward declaration of prototype

# Test 'stampeding herd' / 'cache miss stampede' problem
#

my $slow_time = 1; # how many seconds to sleep in mockup of slow generation
sub get_value_slow {
	$call_count++;
	sleep $slow_time;
	return $value;
}
sub get_value_slow_fh {
	my $fh = shift;

	$call_count++;
	sleep $slow_time;
	print {$fh} $value;
}

sub get_value_die {
	$call_count++;
	die "get_value_die\n";
}

my $lock_file = "$0.$$.lock";
sub get_value_die_once {
	if (sysopen my $fh, $lock_file, (O_WRONLY | O_CREAT | O_EXCL)) {
		close $fh;
		die "get_value_die_once\n";
	} else {
		sleep $slow_time;
		return $value;
	}
}

my @output;
my $sep = '|';
my $total_count = 0;

note("Following tests contain artifical delay of $slow_time seconds");
subtest 'parallel access' => sub {
	$cache->remove($key);
	@output = parallel_run {
		$call_count = 0;
		my $data = cache_get_set($cache, $key, \&get_value_slow);
		print "$data$sep$call_count";
	};
	$total_count = 0;
	foreach (@output) {
		my ($child_out, $child_count) = split(quotemeta $sep, $_);
		#is($child_out, $value, "get/set (parallel) returns '$value'");
		$total_count += $child_count;
	}
	cmp_ok($total_count, '==', 2, 'parallel get/set: get_value_slow() called twice');

	$cache->remove($key);
	@output = parallel_run {
		$call_count = 0;
		my $data = cache_compute($cache, $key, \&get_value_slow);
		print "$data$sep$call_count";
	};
	$total_count = 0;
	foreach (@output) {
		my ($child_out, $child_count) = split(quotemeta $sep, $_);
		#is($child_out, $value, "compute (parallel) returns '$value'");
		$total_count += $child_count;
	}
	cmp_ok($total_count, '==', 1, 'parallel compute: get_value_slow() called once');

	$cache->remove($key);
	@output = parallel_run {
		$call_count = 0;
		my $data = cache_compute_fh($cache, $key, \&get_value_slow_fh);
		print "$data$sep$call_count";
	};
	$total_count = 0;
	foreach (@output) {
		my ($child_out, $child_count) = split(quotemeta $sep, $_);
		#is($child_out, $value, "compute_fh (parallel) returns '$value'");
		$total_count += $child_count;
	}
	cmp_ok($total_count, '==', 1, 'parallel compute_fh: get_value_slow_fh() called once');

	eval {
		local $SIG{ALRM} = sub { die "alarm\n"; };
		alarm 4*$slow_time;

		@output = parallel_run {
			$call_count = 0;
			my $data = eval { cache_compute($cache, 'No Key', \&get_value_die); };
			my $eval_error = $@;
			print "$data" if defined $data;
			print "$sep";
			print "$eval_error" if defined $eval_error;
		};
		is_deeply(
			\@output,
			[ ( "${sep}get_value_die\n" ) x 2 ],
			'parallel compute: get_value_die() died in both'
		);

		alarm 0;
	};
	ok(!$@, 'parallel compute: no alarm call (neither process hung)');
	diag($@) if $@;

	$cache->remove($key);
	unlink($lock_file);
	@output = parallel_run {
		my $data = eval { cache_compute($cache, $key, \&get_value_die_once); };
		my $eval_error = $@;
		print "$data" if defined $data;
		print "$sep";
		print "$eval_error" if defined $eval_error;
	};
	is_deeply(
		[sort @output],
		[sort ("$value$sep", "${sep}get_value_die_once\n")],
		'parallel compute: return correct value even if other process died'
	);
	unlink($lock_file);

	done_testing();
};

# Test that cache returns stale data in existing but expired cache situation
# (probably should be run only if GIT_TEST_LONG)
#
my $stale_value = 'Stale Value';

subtest 'serving stale data when (re)generating' => sub {
	# without background generation
	$cache->set_background_cache(0);

	$cache->set($key, $stale_value);
	$call_count = 0;
	$cache->set_expires_in(0);    # expire now
	$cache->set_max_lifetime(-1); # forever (always serve stale data)

	@output = parallel_run {
		my $data = cache_compute($cache, $key, \&get_value_slow);
		print $data;
	};
	# returning stale data works
	is_deeply(
		[sort @output],
		[sort ($value, $stale_value)],
		'no background: stale data returned by one process'
	);

	$cache->set_expires_in(-1); # never expire for next ->get
	is($cache->get($key), $value,
	   'no background: value got set correctly, even if stale data returned');


	# with background generation
	$cache->set_background_cache(1);
	$cache->set_generating_info_is_safe(1);

	$cache->set($key, $stale_value);
	$cache->set_expires_in(0);    # set value is now expired
	@output = parallel_run {
		my $data = cache_compute($cache, $key, \&get_value_slow);
		print $data;
	};
	# returning stale data works
	is_deeply(
		\@output,
		[ ($stale_value) x 2 ],
		'background: stale data returned by both process when expired'
	);

	$cache->set_expires_in(-1); # never expire for next ->get
	note('waiting for background process to have time to set data');
	sleep $slow_time; # wait for background process to have chance to set data
	is($cache->get($key), $value,
	   'background: value got set correctly by background process');


# 	$cache->set($key, $stale_value);
# 	unlink($lock_file);
# 	@output = parallel_run {
# 		my $data = eval { cache_compute($cache, $key, \&get_value_die_once); };
# 		my $eval_error = $@;
# 		print "$data" if defined $data;
# 		print "$sep";
# 		print "$eval_error" if defined $eval_error;
# 	};
#  TODO: {
# 		local $TODO = 'not implemented';
#
# 		is_deeply(
# 			[sort @output],
# 			[sort ("$value${sep}", "${sep}get_value_die_once\n")],
# 			'return non-stale value, even if process regenerating it died'
# 		);
#
# 		$cache->set_expires_in(-1); # never expire for next ->get
# 		is($cache->get($key), $value,
# 		   'value got regenerated, even if process regenerating it died');
# 	};
# 	unlink($lock_file);

	$cache->set($key, $stale_value);
	$cache->set_expires_in(0);   # expire now
	$cache->set_max_lifetime(0); # don't serve stale data

	@output = parallel_run {
		my $data = cache_compute($cache, $key, \&get_value_slow);
		print $data;
	};
	# no returning stale data
	ok(!scalar(grep { $_ eq $stale_value } @output),
	   'no stale data if configured');


	done_testing();
};
$cache->set_expires_in(-1);


# Test 'generating_info' feature
#
$cache->remove($key);
my $progress_info = "Generating...";
sub test_generating_info {
	local $| = 1;
	print "$progress_info";
}
$cache->set_generating_info(\&test_generating_info);

subtest 'generating progress info' => sub {
	my @progress;

	# without background generation
	$cache->set_background_cache(0);
	$cache->remove($key);

	@output = parallel_run {
		my $data = cache_compute($cache, $key, \&get_value_slow);
		print "$sep$data";
	};
	@progress = map { s/^(.*)\Q${sep}\E//o && $1 } @output;
	is_deeply(
		[sort @progress],
		[sort ('', $progress_info)],
		'no background: one process waiting for data prints progress info'
	);
	is_deeply(
		\@output,
		[ ($value) x 2 ],
		'no background: both processes return correct value'
	);


	# without background generation, with stale value
	$cache->set($key, $stale_value);
	$cache->set_expires_in(0);    # set value is now expired
	$cache->set_max_lifetime(-1); # stale data never expire
	@output = parallel_run {
		my $data = cache_compute($cache, $key, \&get_value_slow);
		print "$sep$data";
	};
	is_deeply(
		[sort @output],
	## no progress for generating process without background generation;
	#	[sort ("$progress_info$sep$value", "$sep$stale_value")],
		[sort ("$sep$value", "$sep$stale_value")],
		'no background, stale data: generating gets data, other gets stale data'
	) or diag('@output is ', join ", ", sort @output);
	$cache->set_expires_in(-1);


	# with background generation
	$cache->set_background_cache(1);
	$cache->remove($key);

	@output = parallel_run {
		my $data = cache_compute($cache, $key, \&get_value_slow);
		print "$sep$data";
	};
	@progress = map { s/^(.*)\Q${sep}\E//o && $1 } @output;
	is_deeply(
		\@progress,
		[ ($progress_info) x 2],
		'background: both process print progress info'
	);
	is_deeply(
		\@output,
		[ ($value) x 2 ],
		'background: both processes return correct value'
	);


	done_testing();
};
$cache->set_expires_in(-1);


done_testing();


#######################################################################
#######################################################################
#######################################################################

# use ->get($key) and ->set($key, $data) interface
sub cache_get_set {
	my ($cache, $key, $code) = @_;

	my $data = $cache->get($key);
	if (!defined $data) {
		$data = $code->();
		$cache->set($key, $data);
	}

	return $data;
}

# use ->compute($key, $code) interface
sub cache_compute {
	my ($cache, $key, $code) = @_;

	my $data = $cache->compute($key, $code);
	return $data;
}
# use ->compute_fh($key, $code_fh) interface
sub cache_compute_fh {
	my ($cache, $key, $code_fh) = @_;

	my ($fh, $filename) = $cache->compute_fh($key, $code_fh);

	local $/ = undef;
	return <$fh>;
}

# from http://aaroncrane.co.uk/talks/pipes_and_processes/
sub fork_child (&) {
	my ($child_process_code) = @_;

	my $pid = fork();
	die "Failed to fork: $!\n" if !defined $pid;

	return $pid if $pid != 0;

	# Now we're in the new child process
	$child_process_code->();
	exit;
}

sub parallel_run (&) {
	my $child_code = shift;
	my $nchildren = 2;

	my %children;
	my (%pid_for_child, %fd_for_child);
	my $sel = IO::Select->new();
	foreach my $child_idx (1..$nchildren) {
		my $pipe = IO::Pipe->new()
			or die "Failed to create pipe: $!\n";

		my $pid = fork_child {
			$pipe->writer()
				or die "$$: Child \$pipe->writer(): $!\n";
			dup2(fileno($pipe), fileno(STDOUT))
				or die "$$: Child $child_idx failed to reopen stdout to pipe: $!\n";
			close $pipe
				or die "$$: Child $child_idx failed to close pipe: $!\n";

			# From Test-Simple-0.96/t/subtest/fork.t
			#
			# Force all T::B output into the pipe (redirected to STDOUT),
			# for the parent builder as well as the current subtest builder.
			{
				no warnings 'redefine';
				*Test::Builder::output         = sub { *STDOUT };
				*Test::Builder::failure_output = sub { *STDOUT };
				*Test::Builder::todo_output    = sub { *STDOUT };
			}

			$child_code->();

			*STDOUT->flush();
			close(STDOUT);
		};

		$pid_for_child{$pid} = $child_idx;
		$pipe->reader()
			or die "Failed to \$pipe->reader(): $!\n";
		$fd_for_child{$pipe} = $child_idx;
		$sel->add($pipe);

		$children{$child_idx} = {
			'pid'    => $pid,
			'stdout' => $pipe,
			'output' => '',
		};
	}

	while (my @ready = $sel->can_read()) {
		foreach my $fh (@ready) {
			my $buf = '';
			my $nread = sysread($fh, $buf, 1024);

			exists $fd_for_child{$fh}
				or die "Cannot find child for fd: $fh\n";

			if ($nread > 0) {
				$children{$fd_for_child{$fh}}{'output'} .= $buf;
			} else {
				$sel->remove($fh);
			}
		}
	}

	while (%pid_for_child) {
		my $pid = waitpid -1, 0;
		warn "Child $pid_for_child{$pid} ($pid) failed with status: $?\n"
			if $? != 0;
		delete $pid_for_child{$pid};
	}

	return map { $children{$_}{'output'} } keys %children;
}

__END__
