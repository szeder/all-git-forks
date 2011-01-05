#!/usr/bin/perl
use lib (split(/:/, $ENV{GITPERLLIB}));

use warnings;
use strict;

use POSIX qw(dup2);
use Fcntl qw(:DEFAULT);
use IO::Handle;
use IO::Select;
use IO::Pipe;
use File::Basename;

use Test::More;

# test installed version or source version
use lib $ENV{GITWEBLIBDIR} || "$ENV{GIT_BUILD_DIR}/gitweb/lib";


# Test creating a cache
#
BEGIN { use_ok('GitwebCache::FileCacheWithLocking'); }
note("Using lib '$INC[0]'");
note("Testing '$INC{'GitwebCache/FileCacheWithLocking.pm'}'");

my $cache = new_ok('GitwebCache::FileCacheWithLocking', [
	'max_lifetime' => 0, # turn it off
	'background_cache' => 0,
]);

# ->compute_fh() can fork, don't generate zombies
#local $SIG{CHLD} = 'IGNORE';

# Test that default values are defined
#
ok(defined $GitwebCache::FileCacheWithLocking::DEFAULT_CACHE_ROOT,
	'$GitwebCache::FileCacheWithLocking::DEFAULT_CACHE_ROOT defined');
ok(defined $GitwebCache::FileCacheWithLocking::DEFAULT_CACHE_DEPTH,
	'$GitwebCache::FileCacheWithLocking::DEFAULT_CACHE_DEPTH defined');

# Test some accessors and some default values for cache
#
SKIP: {
	skip 'default values not defined', 2
		unless ($GitwebCache::FileCacheWithLocking::DEFAULT_CACHE_ROOT &&
		        $GitwebCache::FileCacheWithLocking::DEFAULT_CACHE_DEPTH);

	cmp_ok($cache->get_root(),  'eq', $GitwebCache::FileCacheWithLocking::DEFAULT_CACHE_ROOT,
		"default cache root is '$GitwebCache::FileCacheWithLocking::DEFAULT_CACHE_ROOT'");
	cmp_ok($cache->get_depth(), '==', $GitwebCache::FileCacheWithLocking::DEFAULT_CACHE_DEPTH,
		"default cache depth is $GitwebCache::FileCacheWithLocking::DEFAULT_CACHE_DEPTH");
}

# Test the getting and setting of a cached value,
# and removal of a cached value
#
my $key   = 'Test Key';
my $value = 'Test Value';

my $call_count = 0;
sub get_value_fh {
	my $fh = shift;
	$call_count++;
	print {$fh} $value;
}

# use ->compute_fh($key, $code_fh) interface
sub cache_compute_fh {
	my ($cache, $key, $code_fh) = @_;

	my ($fh, $filename) = $cache->compute_fh($key, $code_fh);
	return unless $fh;

	local $/ = undef;
	return <$fh>;
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

subtest 'compute_fh interface' => sub {
	foreach my $method (qw(remove compute_fh)) {
		can_ok($cache, $method);
	}

	eval { $cache->remove('Not-Existent Key'); };
	ok(!$@, 'remove on non-existent key doesn\'t die');
	diag($@) if $@;

	$cache->remove($key); # just in case
	is(cache_compute_fh($cache, $key, \&get_value_fh), $value,
	   "compute_fh 1st time (set) returns '$value'");
	is(cache_compute_fh($cache, $key, \&get_value_fh), $value,
	   "compute_fh 2nd time (get) returns '$value'");
	is(cache_compute_fh($cache, $key, \&get_value_fh), $value,
	   "compute_fh 3rd time (get) returns '$value'");
	cmp_ok($call_count, '==', 1, 'get_value_fh() is called once from compute_fh');

	done_testing();
};


# Test cache expiration
#
subtest 'cache expiration' => sub {
	$cache->set_expires_in(60*60*24); # set expire time to 1 day
	cmp_ok($cache->get_expires_in(), '>', 0, '"expires in" is greater than 0 (set to 1d)');
	$call_count = 0;
	cache_compute_fh($cache, $key, \&get_value_fh);
	cmp_ok($call_count, '==', 0, 'compute_fh didn\'t need to compute data (not expired in 1d)');
	is(cache_get_fh($cache, $key), $value, 'get_fh returns cached value (not expired in 1d)');

	$cache->set_expires_in(-1); # set expire time to never expire
	is($cache->get_expires_in(), -1,         '"expires in" is set to never (-1)');
	is(cache_get_fh($cache, $key), $value,   'get returns cached value (not expired)');

	$cache->set_expires_in(0);
	is($cache->get_expires_in(),  0,         '"expires in" is set to now (0)');
	ok(!defined(cache_get_fh($cache, $key)), 'cache is expired, get_fh returns undef');
	cache_compute_fh($cache, $key, \&get_value_fh);
	cmp_ok($call_count, '==', 1,             'compute_fh computed and set data');

	done_testing();
};


# ----------------------------------------------------------------------
# CONCURRENT ACCESS
sub parallel_run (&); # forward declaration of prototype

# Test 'stampeding herd' / 'cache miss stampede' problem
#
my $slow_time = 1; # how many seconds to sleep in mockup of slow generation
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
my $lock_file = "$0.$$.lock"; # if exists then get_value_die_once_fh was already called
sub get_value_die_once_fh {
	if (sysopen my $lock_fh, $lock_file, (O_WRONLY | O_CREAT | O_EXCL)) {
		close $lock_fh;
		die "get_value_die_once_fh\n";
	} else {
		get_value_slow_fh(@_);
	}
}

my @output;    # gathers output from concurrent invocations
my $sep = '|'; # separate different parts of data for tests
my $total_count = 0; # number of calls around all concurrent invocations

note("Following tests contain artifical delay of $slow_time seconds");
subtest 'parallel access' => sub {

	$cache->remove($key);
	@output = parallel_run {
		$call_count = 0;
		my $data = cache_compute_fh($cache, $key, \&get_value_slow_fh);
		print $data if defined $data;
		print "$sep$call_count";
	};
	$total_count = 0;
	foreach (@output) {
		my ($child_out, $child_count) = split(quotemeta $sep, $_);
		$total_count += $child_count;
	}
	cmp_ok($total_count, '==', 1, 'parallel compute_fh: get_value_slow_fh() called only once');
	# extract only data, without child count
	@output = map { s/\Q$sep\E.*$//; $_ } @output;
	is_deeply(
		\@output,
		[ ($value) x 2 ],
		"parallel compute_fh: both returned '$value'"
	);

	$cache->set_on_error(sub { die @_; });
	eval {
		local $SIG{ALRM} = sub { die "alarm\n"; };
		alarm 4*$slow_time;

		@output = parallel_run {
			$call_count = 0;
			my $data = eval { cache_compute_fh($cache, 'No Key', \&get_value_die); };
			my $eval_error = $@;
			print "$data" if defined $data;
			print "$sep";
			print "$eval_error" if $eval_error;
		};
		is_deeply(
			\@output,
			[ ( "${sep}get_value_die\n" ) x 2 ],
			'parallel compute_fh: get_value_die() died in both'
		);

		alarm 0;
	};
	ok(!$@, 'parallel compute_fh: no alarm call (neither process hung)');
	diag($@) if $@;

	$cache->remove($key);
	unlink($lock_file);
	@output = parallel_run {
		my $data = eval { cache_compute_fh($cache, $key, \&get_value_die_once_fh); };
		my $eval_error = $@;
		print "$data" if defined $data;
		print "$sep";
		print "$eval_error" if $eval_error;
	};
	is_deeply(
		[sort @output],
		[sort ("$value$sep", "${sep}get_value_die_once_fh\n")],
		'parallel compute_fh: return correct value even if other process died'
	);
	unlink($lock_file);

	done_testing();
};


# Test that cache returns stale data in existing but expired cache situation
#
my $stale_value = 'Stale Value';

subtest 'serving stale data when regenerating' => sub {
	$cache->remove($key);
	cache_set_fh($cache, $key, $stale_value);
	$cache->set_expires_in(-1);   # never expire, for next check
	is(cache_get_fh($cache, $key), $stale_value,
	   'stale value set (prepared) correctly');

	$call_count = 0;
	$cache->set_expires_in(0);    # expire now (so there are no fresh data)
	$cache->set_max_lifetime(-1); # stale data is valid forever

	# without background generation
	$cache->set_background_cache(0);

	@output = parallel_run {
		my $data = cache_compute_fh($cache, $key, \&get_value_slow_fh);
		print "$call_count$sep";
		print $data if defined $data;
	};
	# returning stale data works
	is_deeply(
		[sort @output],
		[sort ("0$sep$stale_value", "1$sep$value")],
		'no background: stale data returned by one process (the one not generating data)'
	);
	$cache->set_expires_in(-1); # never expire for next ->get
	is(cache_get_fh($cache, $key), $value,
	   'no background: value got set correctly, even if stale data returned');


	# with background generation
	$cache->set_background_cache(1);
	$call_count = 0;
	cache_set_fh($cache, $key, $stale_value);
	$cache->set_expires_in(0);  # expire now (so there are no fresh data)

	@output = parallel_run {
		my $data = cache_compute_fh($cache, $key, \&get_value_slow_fh);
		print "$call_count$sep";
		print $data if defined $data;
	};
	# returning stale data works
	is_deeply(
		[sort @output],
		[sort ("0$sep$stale_value", "0$sep$stale_value")],
		'background: stale data returned by both processes'
	);
	$cache->set_expires_in(-1); # never expire for next ->get
	note("waiting $slow_time sec. for background process to have time to set data");
	sleep $slow_time; # wait for background process to have chance to set data
	is(cache_get_fh($cache, $key), $value,
	   'background: value got set correctly by background process');
	$cache->set_expires_in(0);  # expire now (so there are no fresh data)


	cache_set_fh($cache, $key, $stale_value);
	$cache->set_expires_in(0);   # expire now
	$cache->set_max_lifetime(0); # don't serve stale data

	@output = parallel_run {
		my $data = cache_compute_fh($cache, $key, \&get_value_slow_fh);
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

	# without background generation, and without stale value
	$cache->set_background_cache(0);
	$cache->remove($key); # no data and no stale data
	$call_count = 0;

	@output = parallel_run {
		my $data = cache_compute_fh($cache, $key, \&get_value_slow_fh);
		print "$sep$call_count$sep";
		print $data if defined $data;
	};
	# split progress and output
	@progress = map { s/^(.*)\Q${sep}\E//o && $1 } @output;
	is_deeply(
		[sort @progress],
		[sort ("${sep}1", "$progress_info${sep}0")],
		'no background, no stale data: the process waiting for data prints progress info'
	);
	is_deeply(
		\@output,
		[ ($value) x 2 ],
		'no background, no stale data: both processes return correct value'
	);


	# without background generation, with stale value
	cache_set_fh($cache, $key, $stale_value);
	$cache->set_expires_in(0);    # set value is now expired
	$cache->set_max_lifetime(-1); # stale data never expire
	$call_count = 0;

	@output = parallel_run {
		my $data = cache_compute_fh($cache, $key, \&get_value_slow_fh);
		print "$sep$call_count$sep";
		print $data if defined $data;
	};
	@progress = map { s/^(.*?)\Q${sep}\E//o && $1 } @output;
	is_deeply(
		\@progress,
		[ ('') x 2],
		'no background, stale data: neither process prints progress info'
	);
	is_deeply(
		[sort @output],
		[sort ("1$sep$value", "0$sep$stale_value")],
		'no background, stale data: generating gets data, other gets stale data'
	);
	$cache->set_expires_in(-1);


	# with background generation
	$cache->set_background_cache(1);
	$cache->remove($key); # no data and no stale value
	$call_count = 0;

	@output = parallel_run {
		my $data = cache_compute_fh($cache, $key, \&get_value_slow_fh);
		print $sep;
		print $data if defined $data;
	};
	@progress = map { s/^(.*)\Q${sep}\E//o && $1 } @output;
	is_deeply(
		\@progress,
		[ ($progress_info) x 2],
		'background, no stale data: both process print progress info'
	);
	is_deeply(
		\@output,
		[ ($value) x 2 ],
		'background, no stale data: both processes return correct value'
	);


	# with background generation, with stale value
	cache_set_fh($cache, $key, $stale_value);
	$cache->set_expires_in(0);    # set value is now expired
	$cache->set_max_lifetime(-1); # stale data never expire
	$call_count = 0;

	@output = parallel_run {
		my $data = cache_compute_fh($cache, $key, \&get_value_slow_fh);
		print $sep;
		print $data if defined $data;
	};
	@progress = map { s/^(.*)\Q${sep}\E//o && $1 } @output;
	is_deeply(
		\@progress,
		[ ('') x 2],
		'background, stale data: neither process prints progress info'
	);
	note("waiting $slow_time sec. for background process to have time to set data");
	sleep $slow_time; # wait for background process to have chance to set data


	done_testing();
};
$cache->set_expires_in(-1);

done_testing();


#######################################################################
#######################################################################
#######################################################################

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
