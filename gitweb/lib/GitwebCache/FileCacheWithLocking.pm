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
sub new {
	my $class = shift;
	my %opts = ref $_[0] ? %{ $_[0] } : @_;

	my $self = $class->SUPER::new(\%opts);

	my ($max_lifetime);
	if (%opts) {
		$max_lifetime =
			$opts{'max_lifetime'} ||
			$opts{'max_cache_lifetime'};
	}
	$max_lifetime = -1 unless defined($max_lifetime);

	$self->set_max_lifetime($max_lifetime);

	return $self;
}

# ......................................................................
# accessors

# http://perldesignpatterns.com/perldesignpatterns.html#AccessorPattern

# creates get_depth() and set_depth($depth) etc. methods
foreach my $i (qw(max_lifetime)) {
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
			# acquired writers lock
			@result = $set_code->();

			# closing lockfile releases lock
			close $lock_fh
				or die "Could't close lockfile '$lockfile': $!";

		} else {
			# try to retrieve stale data
			@result = $fetch_code->()
				if $self->is_valid($key, $self->get_max_lifetime());
			return @result if @result;

			# get readers lock (wait for writer)
			# if there is no stale data to serve
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
