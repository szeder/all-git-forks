# gitweb - simple web interface to track changes in git repositories
#
# (C) 2006, John 'Warthog9' Hawley <warthog19@eaglescrag.net>
# (C) 2010, Jakub Narebski <jnareb@gmail.com>
#
# This program is licensed under the GPLv2

#
# Gitweb caching engine, simple file-based cache, with locking
#

# Based on GitwebCache::SimpleFileCache, minimalistic cache that
# stores data in the filesystem, without serialization.
#
# It uses file locks (flock) to have only one process generating data
# and writing to cache, when using CHI interface ->compute() method.

package GitwebCache::FileCacheWithLocking;
use base qw(GitwebCache::SimpleFileCache);

use strict;
use warnings;

use File::Path qw(mkpath);
use Fcntl qw(:flock);
use POSIX qw(setsid);

# ......................................................................
# constructor

# The options are set by passing in a reference to a hash containing
# any of the following keys:
#  * 'namespace'
#    The namespace associated with this cache.  This allows easy separation of
#    multiple, distinct caches without worrying about key collision.  Defaults
#    to $DEFAULT_NAMESPACE.
#  * 'cache_root' (Cache::FileCache compatibile),
#    'root_dir' (CHI::Driver::File compatibile),
#    The location in the filesystem that will hold the root of the cache.
#    Defaults to $DEFAULT_CACHE_ROOT.
#  * 'cache_depth' (Cache::FileCache compatibile),
#    'depth' (CHI::Driver::File compatibile),
#    The number of subdirectories deep to cache object item.  This should be
#    large enough that no cache directory has more than a few hundred objects.
#    Defaults to $DEFAULT_CACHE_DEPTH unless explicitly set.
#  * 'default_expires_in' (Cache::Cache compatibile),
#    'expires_in' (CHI compatibile) [seconds]
#    The expiration time for objects place in the cache.
#    Defaults to -1 (never expire) if not explicitly set.
#    Sets 'expires_min' to given value.
#  * 'expires_min' [seconds]
#    The minimum expiration time for objects in cache (e.g. with 0% CPU load).
#    Used as lower bound in adaptive cache lifetime / expiration.
#    Defaults to 20 seconds; 'expires_in' sets it also.
#  * 'expires_max' [seconds]
#    The maximum expiration time for objects in cache.
#    Used as upper bound in adaptive cache lifetime / expiration.
#    Defaults to 1200 seconds, if not set; 
#    defaults to 'expires_min' if 'expires_in' is used.
#  * 'check_load'
#    Subroutine (code) used for adaptive cache lifetime / expiration.
#    If unset, adaptive caching is turned off; defaults to unset.
#  * 'increase_factor' [seconds / 100% CPU load]
#    Factor multiplying 'check_load' result when calculating cache lietime.
#    Defaults to 60 seconds for 100% SPU load ('check_load' returning 1.0).
#
# (all the above are inherited from GitwebCache::SimpleFileCache)
#
#  * 'max_lifetime' [seconds]
#    If it is greater than 0, and cache entry is expired but not older
#    than it, serve stale data when waiting for cache entry to be 
#    regenerated (refreshed).  Non-adaptive.
#    Defaults to -1 (never expire / always serve stale).
#  * 'background_cache' (boolean)
#    This enables/disables regenerating cache in background process.
#    Defaults to true.
#  * 'generating_info'
#    Subroutine (code) called when process has to wait for cache entry
#    to be (re)generated (when there is no not-too-stale data to serve
#    instead), for other process (or bacground process).  It is passed
#    $cache instance, $key, and opened $lock_fh filehandle to lockfile.
#    Unset by default (which means no activity indicator).
#  * 'generating_info_is_safe' (boolean)
#    If true, run 'generating_info' subroutine also in the project that
#    is generating data.  This has effect only when 'background_cache'
#    is true (both 'background_cache' and 'generating_info_is_safe' must
#    be true for project generating data to run 'generating_info').
#    Defaults to false for safety.
sub new {
	my $class = shift;
	my %opts = ref $_[0] ? %{ $_[0] } : @_;

	my $self = $class->SUPER::new(\%opts);

	my ($max_lifetime, $background_cache, $generating_info, $gen_info_is_safe);
	if (%opts) {
		$max_lifetime =
			$opts{'max_lifetime'} ||
			$opts{'max_cache_lifetime'};
		$background_cache = $opts{'background_cache'};
		$generating_info  = $opts{'generating_info'};
		$gen_info_is_safe = $opts{'generating_info_is_safe'};
	}
	$max_lifetime = -1 unless defined($max_lifetime);
	$background_cache = 1 unless defined($background_cache);
	$gen_info_is_safe = 0 unless defined($gen_info_is_safe);

	$self->set_max_lifetime($max_lifetime);
	$self->set_background_cache($background_cache);
	$self->set_generating_info($generating_info);
	$self->set_generating_info_is_safe($gen_info_is_safe);

	return $self;
}

# ......................................................................
# accessors

# http://perldesignpatterns.com/perldesignpatterns.html#AccessorPattern

# creates get_depth() and set_depth($depth) etc. methods
foreach my $i (qw(max_lifetime background_cache
                  generating_info generating_info_is_safe)) {
	my $field = $i;
	no strict 'refs';
	*{"get_$field"} = sub {
		my $self = shift;
		return $self->{$field};
	};
	*{"set_$field"} = sub {
		my ($self, $value) = @_;
		$self->{$field} = $value;
	};
}

# $cache->generating_info($key, $lock);
# runs 'generating_info' subroutine, for activity indicator,
# checking if it is defined first.
sub generating_info {
	my $self = shift;

	if (defined $self->{'generating_info'}) {
		$self->{'generating_info'}->($self, @_);
	}
}

# ----------------------------------------------------------------------
# utility functions and methods

# Take an human readable key, and return path to be used for lockfile
# Ensures that file can be created, if needed.
sub get_lockname {
	my ($self, $key) = @_;

	my $lockfile = $self->path_to_key($key, \my $dir) . '.lock';

	# ensure that directory leading to lockfile exists
	if (!-d $dir) {
		eval { mkpath($dir, 0, 0777); 1 }
			or die "Couldn't mkpath '$dir' for lockfile: $!";
	}

	return $lockfile;
}

# ----------------------------------------------------------------------
# "private" utility functions and methods

# take a file path to cache entry, and its directory
# return filehandle and filename of open temporary file,
# like File::Temp::tempfile
sub _tempfile_to_path {
	my ($self, $file, $dir) = @_;

	my $tempname = "$file.tmp";
	open my $temp_fh, '>', $tempname
		or die "Couldn't open temporary file '$tempname' for writing: $!";

	return ($temp_fh, $tempname);
}

# ......................................................................
# interface methods

sub _wait_for_data {
	my ($self, $key,
	    $lock_fh, $lockfile,
	    $fetch_code, $fetch_locked) = @_;
	my @result;

	# provide "generating page..." info, if exists
	$self->generating_info($key, $lock_fh);
	# generating info may exit, so we can not get there

	# get readers lock, i.e. wait for writer,
	# which might be background process
	flock($lock_fh, LOCK_SH);
	# closing lockfile releases lock
	if ($fetch_locked) {
		@result = $fetch_code->();
		close $lock_fh
			or die "Could't close lockfile '$lockfile': $!";
	} else {
		close $lock_fh
			or die "Could't close lockfile '$lockfile': $!";
		@result = $fetch_code->();
	}

	return @result;
}

sub _set_maybe_background {
	my ($self, $key, $fetch_code, $set_code) = @_;

	my $pid;
	my (@result, @stale_result);

	if ($self->{'background_cache'}) {
		# try to retrieve stale data
		@stale_result = $fetch_code->()
			if $self->is_valid($key, $self->get_max_lifetime());

		# fork if there is stale data, for background process
		# to regenerate/refresh the cache (generate data),
		# or if main process would show progress indicator
		$pid = fork()
			if (@stale_result ||
			    ($self->{'generating_info'} && $self->{'generating_info_is_safe'}));
	}

	if ($pid) {
		## forked and are in parent process
		# reap child, which spawned grandchild process (detaching it)
		waitpid $pid, 0;

	}	else {
		## didn't fork, or are in background process

		# daemonize background process, detaching it from parent
		# see also Proc::Daemonize, Apache2::SubProcess
		if (defined $pid) {
			## in background process
			POSIX::setsid(); # or setpgrp(0, 0);
			fork() && CORE::exit(0);
		}

		@result = $set_code->();

		if (defined $pid) {
			## in background process; parent will serve stale data

			# lockfile will be automatically closed on exit,
			# and therefore lockfile would be unlocked
			CORE::exit(0);
		}
	}

	return @result > 0 ? @result : @stale_result;
}

sub _compute_generic {
	my ($self, $key,
	    $get_code, $fetch_code, $set_code, $fetch_locked) = @_;

	my @result = $get_code->();
	return @result if @result;

	my $lockfile = $self->get_lockname($key);

	# this loop is to protect against situation where process that
	# acquired exclusive lock (writer) dies or exits (die_error)
	# before writing data to cache
	my $lock_state; # needed for loop condition
	do {
		open my $lock_fh, '+>', $lockfile
			or die "Could't open lockfile '$lockfile': $!";

		$lock_state = flock($lock_fh, LOCK_EX | LOCK_NB);
		if ($lock_state) {
			## acquired writers lock, have to generate data
			@result = $self->_set_maybe_background($key, $fetch_code, $set_code);

			# closing lockfile releases writer lock
			close $lock_fh
				or die "Could't close lockfile '$lockfile': $!";

			if (!@result) {
				# wait for background process to finish generating data
				open $lock_fh, '<', $lockfile
					or die "Couldn't reopen (for reading) lockfile '$lockfile': $!";

				@result = $self->_wait_for_data($key, $lock_fh, $lockfile,
				                                $fetch_code, $fetch_locked);
			}

		} else {
			## didn't acquire writers lock, get stale data or wait for regeneration

			# try to retrieve stale data
			@result = $fetch_code->()
				if $self->is_valid($key, $self->get_max_lifetime());
			return @result if @result;

			# wait for regeneration
			@result = $self->_wait_for_data($key, $lock_fh, $lockfile,
			                                $fetch_code, $fetch_locked);

		}
	} until (@result || $lock_state);
	# repeat until we have data, or we tried generating data oneself and failed
	return @result;
}

# $data = $cache->compute($key, $code);
#
# Combines the get and set operations in a single call.  Attempts to
# get $key; if successful, returns the value.  Otherwise, calls $code
# and uses the return value as the new value for $key, which is then
# returned.
#
# Uses file locking to have only one process updating value for $key
# to avoid 'cache miss stampede' (aka 'stampeding herd') problem.
sub compute {
	my ($self, $key, $code) = @_;

	return ($self->_compute_generic($key,
		sub {
			return $self->get($key);
		},
		sub {
			return $self->fetch($key);
		},
		sub {
			my $data = $code->();
			$self->set($key, $data);
			return $data;
		},
		0 # $self->get($key); is outside LOCK_SH critical section
	))[0]; # return single value: $data
}

# ($fh, $filename) = $cache->compute_fh($key, $code);
#
# Combines the get and set operations in a single call.  Attempts to
# get $key; if successful, returns the filehandle it can be read from.
# Otherwise, calls $code passing filehandle to write to as a
# parameter; contents of this file is then used as the new value for
# $key; returns filehandle from which one can read newly generated data.
#
# Uses file locking to have only one process updating value for $key
# to avoid 'cache miss stampede' (aka 'stampeding herd') problem.
sub compute_fh {
	my ($self, $key, $code_fh) = @_;

	return $self->_compute_generic($key,
		sub {
			return $self->get_fh($key);
		},
		sub {
			return $self->fetch_fh($key);
		},
		sub {
			return $self->set_coderef_fh($key, $code_fh);
		},
		1 # $self->fetch_fh($key); just opens file
	);
}

1;
__END__
# end of package GitwebCache::FileCacheWithLocking
