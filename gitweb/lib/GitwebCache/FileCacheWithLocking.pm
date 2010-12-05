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
# constructor is inherited from GitwebCache::SimpleFileCache

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
	    $get_code, $set_code, $get_locked) = @_;

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
			# get readers lock (wait for writer)
			flock($lock_fh, LOCK_SH);
			# closing lockfile releases lock
			if ($get_locked) {
				@result = $get_code->();
				close $lock_fh
					or die "Could't close lockfile '$lockfile': $!";
			} else {
				close $lock_fh
					or die "Could't close lockfile '$lockfile': $!";
				@result = $get_code->();
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
			return $self->set_coderef_fh($key, $code_fh);
		},
		1 # $self->get_fh($key); just opens file
	);
}

1;
__END__
# end of package GitwebCache::FileCacheWithLocking
