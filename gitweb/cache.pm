# gitweb - simple web interface to track changes in git repositories
#
# (C) 2006, John 'Warthog9' Hawley <warthog19@eaglescrag.net>
#
# This program is licensed under the GPLv2

#
# Gitweb caching engine
#

{
# Minimalistic cache that stores data in the filesystem, without serialization
# and currently without any kind of cache expiration (all keys last forever till
# they got explicitely removed)
#
# It follows Cache::Cache and CHI interface (but does not implement it fully)

package GitwebCache::SimpleFileCache;

use strict;
use warnings;

use File::Path qw(make_path);  # requires version >= 2.0
use File::Spec;
use File::Temp;
use Digest::MD5 qw(md5_hex);

# by default, the cache nests all entries on the filesystem two
# directories deep

our $DEFAULT_CACHE_DEPTH = 2;

# by default, the root of the cache is located in 'cache'.

our $DEFAULT_CACHE_ROOT = "cache";

# ......................................................................
# constructor

# The options are set by passing in a reference to a hash containing
# any of the following keys:
#  * 'namespace'
#    The namespace associated with this cache.  This allows easy separation of
#    multiple, distinct caches without worrying about key collision.  Defaults
#    to '' (which does not allow for simple implementation of clear() method).
#  * 'cache_root'
#    The location in the filesystem that will hold the root of the cache.
#    Defaults to 'cache', relative to gitweb.cgi directory.
#  * 'cache_depth'
#    The number of subdirectories deep to cache object item.  This should be
#    large enough that no cache directory has more than a few hundred objects.
#    Defaults to 2 unless explicitly set.
#  * 'default_expires_in' (Cache::Cache compatibile),
#    'expires_in' (CHI compatibile) [seconds]
#    The expiration time for objects place in the cache.
#    Defaults to $EXPIRES_NEVER if not explicitly set.
sub new {
	my ($proto, $p_options_hash_ref) = @_;

	my $class = ref($proto) || $proto;
	my $self  = {};
	$self = bless($self, $class);

	my ($root, $depth, $ns);
	my ($expires_min, $expires_max, $increase_factor, $check_load);
	if (defined $p_options_hash_ref) {
		$root  = $p_options_hash_ref->{'cache_root'};
		$depth = $p_options_hash_ref->{'cache_depth'};
		$ns    = $p_options_hash_ref->{'namespace'};
		$expires_min =
			$p_options_hash_ref->{'expires_min'} ||
			$p_options_hash_ref->{'default_expires_in'} ||
			$p_options_hash_ref->{'expires_in'};
		$expires_max =
			$p_options_hash_ref->{'expires_max'};
		$increase_factor = $p_options_hash_ref->{'expires_factor'};
		$check_load      = $p_options_hash_ref->{'check_load'};
	}
	$root  = $DEFAULT_CACHE_ROOT  unless defined($root);
	$depth = $DEFAULT_CACHE_DEPTH unless defined($depth);
	$ns    = '' unless defined($ns);
	$expires_min =   20 unless defined($expires_min);
	$expires_max = 1200 unless defined($expires_max);
	$increase_factor = 60 unless defined($increase_factor);
	$check_load = \&main::get_loadavg unless defined($check_load);

	$self->set_root($root);
	$self->set_depth($depth);
	$self->set_namespace($ns);
	$self->set_expires_min($expires_min);
	$self->set_expires_max($expires_max);
	$self->set_increase_factor($increase_factor);
	$self->set_check_load($check_load);

	return $self;
}

# ......................................................................
# accessors

sub get_depth {
	my ($self) = @_;

	return $self->{'_Depth'};
}

sub set_depth {
	my ($self, $depth) = @_;

	$self->{'_Depth'} = $depth;
}

sub get_root {
	my ($self) = @_;

	return $self->{'_Root'};
}


sub set_root {
	my ($self, $root) = @_;

	$self->{'_Root'} = $root;
}

sub get_namespace {
	my ($self) = @_;

	return $self->{'_Namespace'};
}


sub set_namespace {
	my ($self, $namespace) = @_;

	$self->{'_Namespace'} = $namespace;
}


sub get_expires_min {
	my ($self) = @_;

	return $self->{'_Expires_Min'};
}

sub set_expires_min {
	my ($self, $expires_min) = @_;

	$self->{'_Expires_Min'} = $expires_min;
}

sub get_expires_max {
	my ($self) = @_;

	return $self->{'_Expires_Max'};
}

sub set_expires_max {
	my ($self, $expires_max) = @_;

	$self->{'_Expires_Max'} = $expires_max;
}

sub get_increase_factor {
	my ($self) = @_;

	return $self->{'_Increase_Factor'};
}

sub set_increase_factor {
	my ($self, $increase_factor) = @_;

	$self->{'_Increase_Factor'} = $increase_factor;
}

sub get_check_load {
	my ($self) = @_;

	return $self->{'_Check_Load'};
}

sub set_check_load {
	my ($self, $sub) = @_;

	$self->{'_Check_Load'} = $sub;
}

# ......................................................................

sub get_expires_in {
	my ($self) = @_;
	my $expires_in =
		#$self->get_expires_min() +
		$self->get_increase_factor() * $self->get_check_load()->();

	if ($expires_in < $self->get_expires_min()) {
		return $self->get_expires_min();
	} elsif ($expires_in > $self->get_expires_max()) {
		return $self->get_expires_max();
	}

	return $expires_in;
}

# ----------------------------------------------------------------------
# (private) utility functions and methods

# Take an human readable key, and create a unique (hashed) key from it
sub _Build_Hashed_Key {
	my ($p_key) = @_;

	return md5_hex($p_key);
}

# Take an human readable key, and return file path
sub _path_to_key {
	my ($self, $p_namespace, $p_key) = @_;

	return $self->_path_to_hashed_key($p_namespace,
	                                    _Build_Hashed_Key($p_key));
}

# Take hashed key, and return file path
sub _path_to_hashed_key {
	my ($self, $p_namespace, $p_hashed_key) = @_;

	return File::Spec->catfile($self->get_root(), $p_namespace,
	                           _Split_Word($p_hashed_key, $self->get_depth()));
}

# Split word into N components, where each component but last is two-letter word
# e.g. _Split_Word("06b90e786e304a18fdfbd7c7bcc41a6b", 2) == qw(06 b90e786e304a18fdfbd7c7bcc41a6b);
#      _Split_Word("06b90e786e304a18fdfbd7c7bcc41a6b", 3) == qw(06 b9 0e786e304a18fdfbd7c7bcc41a6b);
sub _Split_Word {
	my ($p_word, $p_depth) = @_;

	$p_depth--; # now it is number of leading 2-letter components
	return unpack("(a2)$p_depth a*", $p_word);
}

sub _Read_File {
	my ($p_path) = @_;

	-e $p_path
		or return undef;

	open(my $fh, '<', $p_path)
		or return undef;

	local $/ = undef;
	my $data = <$fh>;

	close($fh);

	return $data;
}

# write a file atomically, assuming that path leading to file exists
sub _Write_File {
	my ($p_path, $p_data) = @_;

	my ($volume, $directory, $filename) = File::Spec->splitpath($p_path);
	if (defined $directory and defined $volume) {
		$directory = File::Spec->catpath($volume, $directory, '');
	}

	my $temp = File::Temp->new(DIR => $directory,
	                          TEMPLATE => "${filename}_XXXXX",
	                          SUFFIX => '.tmp');
	binmode($temp);
	print {$temp} $p_data;
	close($temp);

	rename($temp, $p_path);
}

# ensures that directory leading to path exists, or dies
sub _Make_Path {
	my ($p_path, $p_dir) = @_;

	my ($volume, $directory, $filename) = File::Spec->splitpath($p_path);
	if (defined $directory and defined $volume) {
		$directory = File::Spec->catpath($volume, $directory, "");
	}

	return
		unless (defined $directory and not -d $directory);

	my $numdirs = make_path($directory,
	                        { mode => 0777, error => \my $mkdirerr });
	if (@$mkdirerr) {
		my $mkdirerrmsg = "";
		for my $diag (@$mkdirerr) {
			my ($file, $message) = %$diag;
			if ($file eq '' ){
				$mkdirerrmsg .= "general error: $message\n";
			} else {
				$mkdirerrmsg .= "problem unlinking $file: $message\n";
			}
		}
		#die_error(500, "Could not create cache directory | $mkdirerrmsg");
	}
}

sub _Remove_File {
	my ($p_path) = @_;

	if (-f $p_path) {
		unlink($p_path);
	}
}

# _read_data and _write_data methods do deserialization/serialization
# in original implementation in Cache::Cache distribution

sub _read_data {
	my ($self, $p_path) = @_;

	return _Read_File($p_path);
}

sub _write_data {
	my ($self, $p_path, $p_data) = @_;

	_Make_Path($p_path);
	_Write_File($p_path, $p_data);
}

# ----------------------------------------------------------------------
# worker methods (explicit namespace)

sub restore {
	my ($self, $p_namespace, $p_key) = @_;

	return $self->_read_data($self->_path_to_key($p_namespace, $p_key));
}

sub store {
	my ($self, $p_namespace, $p_key, $p_data) = @_;

	$self->_write_data($self->_path_to_key($p_namespace, $p_key),
	                   $p_data);
}

sub delete_key {
	my ($self, $p_namespace, $p_key) = @_;

	_Remove_File($self->_path_to_key($p_namespace, $p_key));
}

sub get_size {
	my ($self, $p_namespace, $p_key) = @_;

	my $path = $self->_path_to_key($p_namespace, $p_key);
	if (-e $path) {
		return -s $path;
	}
	return 0;
}

# ......................................................................
# interface methods

# Removing and expiring

sub remove {
	my ($self, $p_key) = @_;

	$self->delete_key($self->get_namespace(), $p_key);
}

# exists in cache and is not expired
sub is_valid {
	my ($self, $p_key) = @_;

	# should there be namespace variant of this function?
	my $path = $self->_path_to_key($self->get_namespace(), $p_key);

	# does file exists in cache?
	return 0 unless -f $path;

	# expire time can be set to never
	my $expires_in = $self->get_expires_in();
	return 1 unless (defined $expires_in && $expires_in >= 0);

	# is file expired?
	my $mtime = (stat(_))[9];
	my $now = time();

	return (($now - $mtime) < $expires_in);
}

# Getting and setting

sub set {
	my ($self, $p_key, $p_data) = @_;

	$self->store($self->get_namespace(), $p_key, $p_data);
}

sub get {
	my ($self, $p_key) = @_;

	return undef unless $self->is_valid($p_key);
	my $data = $self->restore($self->get_namespace(), $p_key)
		or return undef;

	return $data;
}

sub compute {
	my ($self, $p_key, $p_coderef) = @_;

	my $data = $self->get($p_key);
	if (!defined $data) {
		$data = $p_coderef->($self, $p_key);
		$self->set($p_key, $data);
	}

	return $data;
}

1;
} # end of package GitwebCache::SimpleFileCache;

# human readable key identifying gitweb output
sub gitweb_output_key {
	return href(-replay => 1, -full => 0, -path_info => 0);
}

sub cache_fetch {
	my ($cache, $action) = @_;

	my $key = gitweb_output_key();
	if ($cache->can('compute')) {
		cache_fetch_compute($cache, $action, $key);
	} else {
		cache_fetch_get_set($cache, $action, $key);
	}
}

# calculate data to regenrate cache
sub cache_calculate {
	my ($action) = @_;

	my $data;
	open my $data_fh, '>', \$data
		or die "Can't open memory file: $!";
	# matches "binmode STDOUT, ':uft8'" at beginning
	binmode $data_fh, ':utf8';

	$out = $data_fh || \*STDOUT;
	$actions{$action}->();

	close $data_fh;

	return $data;
}

# for $cache which can ->compute($key, $code)
sub cache_fetch_compute {
	my ($cache, $action, $key) = @_;

	my $data = $cache->compute($key, sub { cache_calculate($action) });

	if (defined $data) {
		# print cached data
		binmode STDOUT, ':raw';
		print STDOUT $data;
	}
}

# for $cache which can ->get($key) and ->set($key, $data)
sub cache_fetch_get_set {
	my ($cache, $action, $key) = @_;

	my $data = $cache->get($key);

	if (defined $data) {
		# print cached data
		binmode STDOUT, ':raw';
		print STDOUT $data;

	} else {
		$data = cache_calculate($action);

		if (defined $data) {
			$cache->set($key, $data);
			binmode STDOUT, ':raw';
			print STDOUT $data;
		}
	}
}

1;
