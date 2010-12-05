# gitweb - simple web interface to track changes in git repositories
#
# (C) 2006, John 'Warthog9' Hawley <warthog19@eaglescrag.net>
# (C) 2010, Jakub Narebski <jnareb@gmail.com>
#
# This program is licensed under the GPLv2

#
# Gitweb caching engine, simple file-based cache
#

# Minimalistic cache that stores data in the filesystem, without serialization
# and currently without any kind of cache expiration (all keys last forever till
# they got explicitely removed).
#
# It follows Cache::Cache and CHI interfaces (but does not implement it fully)

package GitwebCache::SimpleFileCache;

use strict;
use warnings;

use Carp;
use File::Path qw(mkpath);
use File::Temp qw(tempfile);
use Digest::MD5 qw(md5_hex);

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

	my ($root, $depth, $ns);
	my ($expires_min, $expires_max, $increase_factor, $check_load);
	my ($on_error);
	if (%opts) {
		$root =
			$opts{'cache_root'} ||
			$opts{'root_dir'};
		$depth =
			$opts{'cache_depth'} ||
			$opts{'depth'};
		$ns = $opts{'namespace'};
		$expires_min =
			$opts{'expires_min'} ||
			$opts{'default_expires_in'} ||
			$opts{'expires_in'};
		$expires_max =
			$opts{'expires_max'};
		$increase_factor = $opts{'expires_factor'};
		$check_load      = $opts{'check_load'};
		$on_error =
			$opts{'on_error'} ||
			$opts{'on_get_error'} ||
			$opts{'on_set_error'} ||
			$opts{'error_handler'};
	}
	$root  = $DEFAULT_CACHE_ROOT  unless defined($root);
	$depth = $DEFAULT_CACHE_DEPTH unless defined($depth);
	$ns    = $DEFAULT_NAMESPACE   unless defined($ns);
	$expires_min = -1 unless defined($expires_min);
	$expires_max = $expires_min
		if (!defined($expires_max) && exists $opts{'expires_in'});
	$expires_max = -1 unless (defined($expires_max));
	$increase_factor = 60 unless defined($increase_factor);
	$on_error = "die"
		unless (defined $on_error &&
		        (ref($on_error) eq 'CODE' || $on_error =~ /^die|warn|ignore$/));

	$self->set_root($root);
	$self->set_depth($depth);
	$self->set_namespace($ns);
	$self->set_expires_min($expires_min);
	$self->set_expires_max($expires_max);
	$self->set_increase_factor($increase_factor);
	$self->set_check_load($check_load);
	$self->set_on_error($on_error);

	return $self;
}


# ......................................................................
# accessors

# http://perldesignpatterns.com/perldesignpatterns.html#AccessorPattern

# creates get_depth() and set_depth($depth) etc. methods
foreach my $i (qw(depth root namespace
                  expires_min expires_max increase_factor check_load
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

# ......................................................................
# pseudo-accessors

# returns adaptive lifetime of cache entry for given $key [seconds]
sub get_expires_in {
	my ($self) = @_;

	# short-circuit
	if (!defined $self->{'check_load'} ||
	    $self->{'expires_max'} <= $self->{'expires_min'}) {
		return $self->{'expires_min'};
	}

	my $expires_in =
		#$self->{'expires_min'} +
		$self->{'increase_factor'} * $self->check_load();

	if ($expires_in < $self->{'expires_min'}) {
		return $self->{'expires_min'};
	} elsif ($expires_in > $self->{'expires_max'}) {
		return $self->{'expires_max'};
	}

	return $expires_in;
}

# sets expiration time to $duration, turns off adaptive cache lifetime
sub set_expires_in {
	my ($self, $duration) = @_;

	$self->{'expires_min'} = $self->{'expires_max'} = $duration;
}

# runs 'check_load' subroutine, for adaptive cache lifetime.
# Note: check in caller that 'check_load' exists.
sub check_load {
	my $self = shift;
	return $self->{'check_load'}->();
}

# ----------------------------------------------------------------------
# utility functions and methods

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

sub read_file {
	my ($self, $filename) = @_;

	# Fast slurp, adapted from File::Slurp::read, with unnecessary options removed
	# via CHI::Driver::File (from CHI-0.33)
	my $buf = '';
	open my $read_fh, '<', $filename
		or return;
	binmode $read_fh, ':raw';

	my $size_left = -s $read_fh;

	while ($size_left > 0) {
		my $read_cnt = sysread($read_fh, $buf, $size_left, length($buf));
		return unless defined $read_cnt;

		last if $read_cnt == 0;
		$size_left -= $read_cnt;
		#last if $size_left <= 0;
	}

	close $read_fh
		or $self->_handle_error("Couldn't close file '$filename' opened for reading: $!");
	return $buf;
}

sub write_fh {
	my ($self, $write_fh, $filename, $data) = @_;

	# Fast spew, adapted from File::Slurp::write, with unnecessary options removed
	# via CHI::Driver::File (from CHI-0.33)
	binmode $write_fh, ':raw';

	my $size_left = length($data);
	my $offset = 0;

	while ($size_left > 0) {
		my $write_cnt = syswrite($write_fh, $data, $size_left, $offset);
		return unless defined $write_cnt;

		$size_left -= $write_cnt;
		$offset += $write_cnt; # == length($data);
	}

	close $write_fh
		or $self->_handle_error("Couldn't close file '$filename' opened for writing: $!");
}

sub ensure_path {
	my $self = shift;
	my $dir = shift || return;

	if (!-d $dir) {
		# mkpath will croak()/die() if there is an error
		eval {
			mkpath($dir, 0, 0777);
			1;
		} or $self->_handle_error($@);
	}
}

# ----------------------------------------------------------------------
# "private" utility functions and methods

# take a file path to cache entry, and its directory
# return filehandle and filename of open temporary file,
# like File::Temp::tempfile
sub _tempfile_to_path {
	my ($self, $file, $dir) = @_;

	# tempfile will croak() if there is an error
	my ($temp_fh, $tempname);
	eval {
		($temp_fh, $tempname) = tempfile("${file}_XXXXX",
			#DIR => $dir,
			'UNLINK' => 0, # ensure that we don't unlink on close; file is renamed
			'SUFFIX' => '.tmp');
	} or $self->_handle_error($@);
	return ($temp_fh, $tempname);
}

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
# worker methods

sub fetch {
	my ($self, $key) = @_;

	my $file = $self->path_to_key($key);
	return unless (defined $file && -f $file);

	return $self->read_file($file);
}

sub store {
	my ($self, $key, $data) = @_;

	my $dir;
	my $file = $self->path_to_key($key, \$dir);
	return unless (defined $file && defined $dir);

	# ensure that directory leading to cache file exists
	$self->ensure_path($dir);

	# generate a temporary file
	my ($temp_fh, $tempname) = $self->_tempfile_to_path($file, $dir);
	chmod 0666, $tempname
		or warn "Couldn't change permissions to 0666 / -rw-rw-rw- for '$tempname': $!";

	$self->write_fh($temp_fh, $tempname, $data);

	rename($tempname, $file)
		or $self->_handle_error("Couldn't rename temporary file '$tempname' to '$file': $!");
}

# get size of an element associated with the $key (not the size of whole cache)
sub get_size {
	my ($self, $key) = @_;

	my $path = $self->path_to_key($key)
		or return undef;
	if (-f $path) {
		return -s $path;
	}
	return 0;
}


# ......................................................................
# interface methods

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
		or $self->_handle_error("Couldn't remove file '$file': $!");
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
		or $self->_handle_error("Couldn't stat file '$path': $!");
	# cache entry is invalid if it is size 0 (in bytes)
	return 0 unless ((stat(_))[7] > 0);

	# expire time can be set to never
	$expires_in = defined $expires_in ? $expires_in : $self->get_expires_in();
	return 1 unless (defined $expires_in && $expires_in >= 0);

	# is file expired?
	my $now = time();

	return (($now - $mtime) < $expires_in);
}

# Getting and setting

# $cache->set($key, $data);
#
# Associates $data with $key in the cache, overwriting any existing entry.
# Returns $data.
sub set {
	my ($self, $key, $data) = @_;

	return unless (defined $key && defined $data);

	$self->store($key, $data);

	return $data;
}

# $data = $cache->get($key);
#
# Returns the data associated with $key.  If $key does not exist
# or has expired, returns undef.
sub get {
	my ($self, $key) = @_;

	return unless $self->is_valid($key);

	return $self->fetch($key);;
}

# $data = $cache->compute($key, $code);
#
# Combines the get and set operations in a single call.  Attempts to
# get $key; if successful, returns the value.  Otherwise, calls $code
# and uses the return value as the new value for $key, which is then
# returned.
sub compute {
	my ($self, $key, $code) = @_;

	my $data = $self->get($key);
	if (!defined $data) {
		$data = $code->();
		$self->set($key, $data);
	}

	return $data;
}

# ......................................................................
# nonstandard interface methods

sub fetch_fh {
	my ($self, $key) = @_;

	my $path = $self->path_to_key($key);
	return unless (defined $path);

	open my $fh, '<', $path or return;
	return ($fh, $path);
}


sub get_fh {
	my ($self, $key) = @_;

	return unless ($self->is_valid($key));

	return $self->fetch_fh($key);
}

sub set_coderef_fh {
	my ($self, $key, $code) = @_;

	my $path = $self->path_to_key($key, \my $dir);
	return unless (defined $path && defined $dir);

	# ensure that directory leading to cache file exists
	$self->ensure_path($dir);

	# generate a temporary file
	my ($fh, $tempfile) = $self->_tempfile_to_path($path, $dir);

	# code writes to filehandle or file
	$code->($fh, $tempfile);

	close $fh;
	rename($tempfile, $path)
		or $self->_handle_error("Couldn't rename temporary file '$tempfile' to '$path': $!");

	open $fh, '<', $path or return;
	return ($fh, $path);
}

# ($fh, $filename) = $cache->compute_fh($key, $code);
#
# Combines the get and set operations in a single call.  Attempts to
# get $key; if successful, returns the filehandle it can be read from.
# Otherwise, calls $code passing filehandle to write to as a
# parameter; contents of this file is then used as the new value for
# $key; returns filehandle from which one can read newly generated data.
sub compute_fh {
	my ($self, $key, $code) = @_;

	my ($fh, $filename) = $self->get_fh($key);
	if (!defined $fh) {
		($fh, $filename) = $self->set_coderef_fh($key, $code);
	}

	return ($fh, $filename);
}

1;
__END__
# end of package GitwebCache::SimpleFileCache;
