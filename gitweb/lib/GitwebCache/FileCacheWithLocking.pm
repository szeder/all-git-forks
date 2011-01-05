# gitweb - simple web interface to track changes in git repositories
#
# (C) 2006, John 'Warthog9' Hawley <warthog19@eaglescrag.net>
# (C) 2010, Jakub Narebski <jnareb@gmail.com>
#
# This program is licensed under the GPLv2

#
# Gitweb caching engine, file-based cache with flock-based entry locking
#

# Minimalistic cache that stores data in the filesystem, without serialization.
# It uses file locks (flock) to have only one process generating data and
# writing to cache, when using CHI-like interface ->compute_fh() method.

package GitwebCache::FileCacheWithLocking;

use strict;
use warnings;

use Carp;
use File::Path qw(mkpath);
use Digest::MD5 qw(md5_hex);
use Fcntl qw(:flock);
use POSIX qw(setsid);

# by default, the cache nests all entries on the filesystem single
# directory deep, i.e. '60/b725f10c9c85c70d97880dfe8191b3' for
# key name (key digest) 60b725f10c9c85c70d97880dfe8191b3.
#
our $DEFAULT_CACHE_DEPTH = 1;

# by default, the root of the cache is located in 'cache'.
#
our $DEFAULT_CACHE_ROOT = "cache";

# by default we don't use cache namespace (empty namespace);
# empty namespace does not allow for simple implementation of clear() method.
#
our $DEFAULT_NAMESPACE = '';

# anything less than 0 means to not expire
#
our $NEVER_EXPIRE = -1;

# cache expiration of 0 means that entry is expired
#
our $EXPIRE_NOW = 0;

# ......................................................................
# constructor

# The options are set by passing in hash or a reference to a hash containing
# any of the following keys:
#  * 'namespace'
#    The namespace associated with this cache.  This allows easy separation of
#    multiple, distinct caches without worrying about key collision.  Defaults
#    to $DEFAULT_NAMESPACE.  Might be empty string.
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
#  * 'max_lifetime' [seconds]
#    If it is greater than 0, and cache entry is expired but not older
#    than it, serve stale data when waiting for cache entry to be 
#    regenerated (refreshed).  Non-adaptive.
#  * 'background_cache' (boolean)
#    This enables/disables regenerating cache in background process.
#    Defaults to true.
#  * 'generating_info'
#    Subroutine (code) called when process has to wait for cache entry
#    to be (re)generated (when there is no not-too-stale data to serve
#    instead), for other process (or bacground process).  It is passed
#    $cache instance, $key, and $wait_code subroutine (code reference)
#    to invoke (to call) to wait for cache entry to be ready.
#    Unset by default (which means no activity indicator).
#  * 'on_error' (similar to CHI 'on_get_error'/'on_set_error')
#    How to handle runtime errors occurring during cache gets and cache
#    sets, which may or may not be considered fatal in your application.
#    Options are:
#    * "die" (the default) - call die() with an appropriate message
#    * "warn" - call warn() with an appropriate message
#    * "ignore" - do nothing
#    * <coderef> - call this code reference with an appropriate message
sub new {
	my $class = shift;
	my %opts = ref $_[0] ? %{ $_[0] } : @_;

	my $self = {};
	$self = bless($self, $class);

	$self->{'root'} =
		exists $opts{'cache_root'} ? $opts{'cache_root'} :
		exists $opts{'root_dir'}   ? $opts{'root_dir'} :
		$DEFAULT_CACHE_ROOT;
	$self->{'depth'} =
		exists $opts{'cache_depth'} ? $opts{'cache_depth'} :
		exists $opts{'depth'}       ? $opts{'depth'} :
		$DEFAULT_CACHE_DEPTH;
	$self->{'namespace'} =
		exists $opts{'namespace'} ? $opts{'namespace'} :
		$DEFAULT_NAMESPACE;
	$self->{'expires_in'} =
		exists $opts{'default_expires_in'} ? $opts{'default_expires_in'} :
		exists $opts{'expires_in'}         ? $opts{'expires_in'} :
		$NEVER_EXPIRE;
	$self->{'max_lifetime'} =
		exists $opts{'max_lifetime'}       ? $opts{'max_lifetime'} :
		exists $opts{'max_cache_lifetime'} ? $opts{'max_cache_lifetime'} :
		$NEVER_EXPIRE;
	$self->{'background_cache'} =
		exists $opts{'background_cache'} ? $opts{'background_cache'} :
		1;
	$self->{'generating_info'} = $opts{'generating_info'}
		if exists $opts{'generating_info'};
	$self->{'on_error'} =
		exists $opts{'on_error'}      ? $opts{'on_error'} :
		exists $opts{'on_get_error'}  ? $opts{'on_get_error'} :
		exists $opts{'on_set_error'}  ? $opts{'on_set_error'} :
		exists $opts{'error_handler'} ? $opts{'error_handler'} :
		'die';

	# validation could be put here

	return $self;
}


# ......................................................................
# accessors

# http://perldesignpatterns.com/perldesignpatterns.html#AccessorPattern

# creates get_depth() and set_depth($depth) etc. methods
foreach my $i (qw(depth root namespace expires_in max_lifetime
                  background_cache generating_info
                  on_error)) {
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

# $cache->generating_info($wait_code);
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

# $path = $self->path_to_namespace();
#
# Return root dir for namespace (lazily built, cached)
sub path_to_namespace {
	my ($self) = @_;

	if (!exists $self->{'path_to_namespace'}) {
		if (defined $self->{'namespace'} &&
		    $self->{'namespace'} ne '') {
			$self->{'path_to_namespace'} = "$self->{'root'}/$self->{'namespace'}";
		} else {
			$self->{'path_to_namespace'} =  $self->{'root'};
		}
	}
	return $self->{'path_to_namespace'};
}

# $path = $cache->path_to_key($key);
# $path = $cache->path_to_key($key, \$dir);
#
# Take an human readable key, and return file path.
# Puts dirname of file path in second argument, if it is provided.
sub path_to_key {
	my ($self, $key, $dir_ref) = @_;

	my @paths = ( $self->path_to_namespace() );

	# Create a unique (hashed) key from human readable key
	my $filename = md5_hex($key); # or $digester->add($key)->hexdigest();

	# Split filename so that it have DEPTH subdirectories,
	# where each subdirectory has a two-letter name
	push @paths, unpack("(a2)[$self->{'depth'}] a*", $filename);
	$filename = pop @paths;

	# Join paths together, computing dir separately if $dir_ref was passed.
	my $filepath;
	if (defined $dir_ref && ref($dir_ref)) {
		my $dir = join('/', @paths);
		$filepath = "$dir/$filename";
		$$dir_ref = $dir;
	} else {
		$filepath = join('/', @paths, $filename);
	}

	return $filepath;
}

# $self->ensure_path($dir);
#
# create $dir (directory) if it not exists, thus ensuring that path exists
sub ensure_path {
	my $self = shift;
	my $dir = shift || return;

	if (!-d $dir) {
		# mkpath will croak()/die() if there is an error
		mkpath($dir, 0, 0777);
	}
}

# $filename = $self->get_lockname($key);
#
# Take an human readable key, and return path to be used for lockfile
# Ensures that file can be created, if needed.
sub get_lockname {
	my ($self, $key) = @_;

	my $lockfile = $self->path_to_key($key, \my $dir) . '.lock';

	# ensure that directory leading to lockfile exists
	$self->ensure_path($dir);

	return $lockfile;
}

# ----------------------------------------------------------------------
# "private" utility functions and methods

# ($fh, $filename) = $self->_tempfile_to_path($path_for_key, $dir_for_key);
#
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

# ($fh, $filename) = $self->_wait_for_data($key, $code);
#
# Wait for data to be available using (blocking) $code,
# then return filehandle and filename to read from for $key.
sub _wait_for_data {
	my ($self, $key, $sync_coderef) = @_;
	my @result;

	# provide "generating page..." info, if exists
	$self->generating_info($key, $sync_coderef);
	# generating info may exit, so we can not get there

	# wait for data to be available
	$sync_coderef->();
	# fetch data
	@result = $self->fetch_fh($key);

	return @result;
}

sub _set_maybe_background {
	my ($self, $key, $code) = @_;

	my ($pid, $detach);
	my (@result, @stale_result);

	if ($self->{'background_cache'}) {
		# try to retrieve stale data
		@stale_result = $self->get_fh($key,
			'expires_in' => $self->get_max_lifetime());

		# fork if there is stale data, for background process
		# to regenerate/refresh the cache (generate data),
		# or if main process would show progress indicator
		$detach = @stale_result;
		$pid = fork()
			if (@stale_result || $self->{'generating_info'});
	}

	if ($pid) {
		## forked and are in parent process
		# reap child, which spawned grandchild process (detaching it)
		waitpid $pid, 0
			if $detach;

	} else {
		## didn't fork, or are in background process

		# daemonize background process, detaching it from parent
		# see also Proc::Daemonize, Apache2::SubProcess
		if (defined $pid && $detach) {
			## in background process
			POSIX::setsid(); # or setpgrp(0, 0);
			fork() && CORE::exit(0);
		}

		@result = $self->set_coderef_fh($key, $code);

		if (defined $pid) { # && !$pid
			## in background process; parent or grandparent
			## will serve stale data, or just generated data

			# lockfile will be automatically closed on exit,
			# and therefore lockfile would be unlocked
			CORE::exit(0);
		}
	}

	return @result > 0 ? @result : @stale_result;
}

# $self->_handle_error($raw_error)
#
# based on _handle_get_error and _dispatch_error_msg from CHI::Driver
sub _handle_error {
	my ($self, $error) = @_;

	for ($self->get_on_error()) {
		(ref($_) eq 'CODE') && do { $_->($error) };
		/^ignore$/ && do { };
		/^warn$/   && do { carp $error };
		/^die$/    && do { croak $error };
	}
}

# ----------------------------------------------------------------------
# nonstandard worker and semi-interface methods

# ($fh, $filename) = $self->fetch_fh($key);
#
# Get filehandle to read from for given $key, and filename of cache file.
# Doesn't check if entry expired.
sub fetch_fh {
	my ($self, $key) = @_;

	my $path = $self->path_to_key($key);
	return unless (defined $path);

	open my $fh, '<', $path or return;
	return ($fh, $path);
}

# ($fh, $filename) = $self->get_fh($key, [option => value, ...])
#
# Returns filehandle to read from for given $key, and filename of cache file.
# Returns empty list if entry expired.
#
# $key may be followed by one or more name/value parameters:
# * expires_in [DURATION] - override global expiration time
sub get_fh {
	my ($self, $key, %opts) = @_;

	return unless ($self->is_valid($key, $opts{'expires_in'}));

	return $self->fetch_fh($key);
}

# [($fh, $filename) =] $self->set_coderef_fh($key, $code_fh);
#
# Runs $code_fh, passing to it $fh and $filename of file to write to;
# the contents of this file would be contents of cache entry.
# Returns what $self->fetch_fh($key) would return.
sub set_coderef_fh {
	my ($self, $key, $code) = @_;

	my $path = $self->path_to_key($key, \my $dir);
	return unless (defined $path && defined $dir);

	# ensure that directory leading to cache file exists
	$self->ensure_path($dir);

	# generate a temporary file / file to write to
	my ($fh, $tempfile) = $self->_tempfile_to_path($path, $dir);

	# code writes to filehandle or file
	$code->($fh, $tempfile);

	close $fh;
	rename($tempfile, $path)
		or die "Couldn't rename temporary file '$tempfile' to '$path': $!";

	open $fh, '<', $path or return;
	return ($fh, $path);
}

# ======================================================================
# ......................................................................
# interface methods
#
# note that only those methods use 'on_error' handler;
# all the rest just use "die"

# Removing and expiring

# $cache->remove($key)
#
# Remove the data associated with the $key from the cache.
sub remove {
	my ($self, $key) = @_;

	my $file = $self->path_to_key($key)
		or return;
	return unless -f $file;
	unlink($file)
		or $self->_handle_error("Couldn't remove cache entry file '$file' for key '$key': $!");
}

# $cache->is_valid($key[, $expires_in])
#
# Returns a boolean indicating whether $key exists in the cache
# and has not expired.  Uses global per-cache expires time, unless
# passed optional $expires_in argument.
sub is_valid {
	my ($self, $key, $expires_in) = @_;

	my $path = $self->path_to_key($key);

	# does file exists in cache?
	return 0 unless -f $path;
	# get its modification time
	my $mtime = (stat(_))[9] # _ to reuse stat structure used in -f test
		or $self->_handle_error("Couldn't stat file '$path' for key '$key': $!");

	# expire time can be set to never
	$expires_in = defined $expires_in ? $expires_in : $self->get_expires_in();
	return 1 unless (defined $expires_in && $expires_in >= 0);

	# is file expired?
	my $now = time();

	return (($now - $mtime) < $expires_in);
}

# $cache->expires_in($key)
#
# Returns number of seconds an entry would be valid, or undef
# if cache entry for given $key does not exists.
sub expires_in {
	my ($self, $key) = @_;

	my $path = $self->path_to_key($key);

	# does file exists in cache?
	return undef unless -f $path;
	# get its modification time
	my $mtime = (stat(_))[9] # _ to reuse stat structure used in -f test
		or $self->_handle_error("Couldn't stat file '$path' for key '$key': $!");

	my $expires_in = $self->get_expires_in();

	my $now = time();
	print STDERR __PACKAGE__."now=$now; mtime=$mtime; ".
		"expires_in=$expires_in; diff=".($now - $mtime)."\n";

	return $expires_in == 0 ? 0 : ($self->get_expires_in() - ($now - $mtime));
}

# Getting and setting

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

	my @result = eval { $self->get_fh($key) };
	return @result if @result;
	$self->_handle_error($@) if $@;

	my $lockfile = $self->get_lockname($key);

	# this loop is to protect against situation where process that
	# acquired exclusive lock (writer) dies or exits
	# before writing data to cache
	my $lock_state; # needed for loop condition
	do {
		open my $lock_fh, '+>', $lockfile
			or $self->_handle_error("Could't open lockfile '$lockfile': $!");

		$lock_state = flock($lock_fh, LOCK_EX | LOCK_NB);
		if ($lock_state) {
			## acquired writers lock, have to generate data
			eval { @result = $self->_set_maybe_background($key, $code_fh) };
			$self->_handle_error($@) if $@;

			# closing lockfile releases writer lock
			#flock($lock_fh, LOCK_UN); # it would unlock here and in background process
			close $lock_fh
				or $self->_handle_error("Could't close lockfile '$lockfile': $!");

			if (!@result) {
				# wait for background process to finish generating data
				open $lock_fh, '<', $lockfile
					or $self->_handle_error("Couldn't reopen (for reading) lockfile '$lockfile': $!");

				eval {
					@result = $self->_wait_for_data($key, sub {
						flock($lock_fh, LOCK_SH);
						# or 'waitpid -1, 0;', or 'wait;', as we don't detach now in this situation
					});
				};
				$self->_handle_error($@) if $@;

				# closing lockfile releases readers lock used to wait for data
				flock($lock_fh, LOCK_UN);
				close $lock_fh
					or $self->_handle_error("Could't close reopened lockfile '$lockfile': $!");

				# we didn't detach, so wait for the child to reap it
				# (it should finish working, according to lock status)
				wait;
			}

		} else {
			## didn't acquire writers lock, get stale data or wait for regeneration

			# try to retrieve stale data
			eval {
				@result = $self->get_fh($key,
					'expires_in' => $self->get_max_lifetime());
			};
			return @result if @result;
			$self->_handle_error($@) if $@;

			# wait for regeneration if no stale data to serve,
			# using shared / readers lock to sync (wait for data)
			eval {
				@result = $self->_wait_for_data($key, sub {
					flock($lock_fh, LOCK_SH);
				});
			};
			$self->_handle_error($@) if $@;

			# closing lockfile releases readers lock
			flock($lock_fh, LOCK_UN);
			close $lock_fh
				or $self->_handle_error("Could't close lockfile '$lockfile': $!");

		}
	} until (@result || $lock_state);
	# repeat until we have data, or we tried generating data oneself and failed
	return @result;
}


1;
__END__
# end of package GitwebCache::FileCacheWithLocking;
