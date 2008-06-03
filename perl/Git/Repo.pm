=head1 NAME

Git::Repo - Perl low-level access to the Git version control system.

WARNING: This module is in active development -- do not use it in any
production code as the API may change at any time.

=cut


use strict;
use warnings;
use 5.006002;


package Git::Repo;

use Scalar::Util qw( reftype );
use Carp qw( carp );
use IPC::Open2 qw( open2 );
use IO::Handle;
# for debugging while this module is in development:
#use Carp::Always;  # leave commented out since it's not in core
use Data::Dumper;

our (@EXPORT, @EXPORT_OK);

use base qw( Exporter );

@EXPORT = qw( );
@EXPORT_OK = qw ( assert_sha1 assert assert_opts );


=head1 SYNOPSIS

  use Git::Repo;

=cut


sub assert_opts {
	die "must have an even number of arguments for named options"
	    unless $#_ % 2;
}

sub assert {
	die 'assertion failed' unless shift;
}

sub assert_sha1 {
	for my $sha1 (@_) {
		die 'no SHA1 given' unless defined $sha1;
		die "'$sha1' is not a SHA1 (need to use get_sha1?)"
		    unless $sha1 =~ /^[a-f0-9]{40}$/;
	}
}

=item new ( OPTIONS )

Return a new Git::Repo object.  The following options are supported:

B<directory> - The directory of the repository.

Examples:

    $repo = Git::Repo->new(directory => "/path/to/repository.git");
    $repo = Git::Repo->new(directory => "/path/to/working_copy/.git");

=cut

sub new {
	my $class = shift;
	assert_opts(@_);
	my $self = {@_};
	bless $self, $class;
	assert defined($self->{directory});
	return $self;
}

=item repo_dir

Return the directory of the repository (.../.git in case of a working
copy).

=cut

sub repo_dir {
	shift->{directory}
}

=item git_binary

Return the name of or path to the git binary (used with exec).

=cut

sub git_binary {
	shift->{git_binary}
}

=head2 Calling the Git binary

=item cmd_output ( OPTIONS )

Return the output of the given git command as a string, or as a list
of lines in array context.  Valid options are:

B<cmd> - An array of arguments to pass to git.

B<max_exit_code> - Die if the return value is greater than
C<max_return>.  (default: 0)

To do: Implement base path for git binary (like /usr/local/bin).

To do: According to Git.pm, this might not work with ActiveState Perl
on Win 32.  Need to check or wait for reports.

=cut

sub cmd_output {
	my $self = shift;
	assert_opts @_;
	my %opts = (max_exit_code => 0, @_);
	# We don't support string-commands here unless someone makes a
	# case for them -- they are too dangerous.
	assert(reftype($opts{cmd}) eq 'ARRAY');
	my @cmd = ($self->_get_git_cmd, @{$opts{cmd}});
	open my $fh, '-|', @cmd
	    or die 'cannot open pipe';
	my($output, @lines);
	if (wantarray) {
		@lines = <$fh>;
	} else {
		local $/;
		$output = <$fh>;
	}
	if (not close $fh) {
		if ($!) {
			# Close failed.  Git.pm says it is OK to not
			# die here.
			carp "error closing pipe: $!";
		} elsif ($? >> 8) {
			my $exit_code = $? >> 8;
			die "Command died with exit code $exit_code: " . join(" ", @cmd)
			    if $exit_code > $opts{max_exit_code};
		}
	}
	return @lines if wantarray;
	return $output;
}

=item cmd_oneline ( OPTIONS )

Like cmd_output, but only return the first line, without newline.

=cut

sub cmd_oneline {
	my @lines = cmd_output(@_);
	chomp($lines[0]);
	return $lines[0];
}

=item get_bidi_pipe ( OPTIONS )

Open a new bidirectional pipe and return the its STDIN and STDOUT file
handles.  Valid options are:

B<cmd> - An array of arguments to pass to git.

B<reuse> - Reuse a previous pipe with the same command line and whose
reuse option was true (default: false).

=cut

sub get_bidi_pipe {
	my $self = shift;
	assert_opts @_;
	my %opts = @_;
	die 'missing or empty cmd option' unless $opts{cmd} and @{$opts{cmd}};
	my($stdin, $stdout);
	my $cmd_str = join ' ', @{$opts{cmd}};  # key for reusing pipes
	if ($opts{reuse}) {
		my $pair = $self->{bidi_pipes}->{$cmd_str};
		return @$pair if $pair;
	}
	my @cmd = ($self->_get_git_cmd, @{$opts{cmd}});
	open2($stdout, $stdin, @cmd)
	    or die 'cannot open pipe';
	if ($opts{reuse}) {
		$self->{bidi_pipes}->{$cmd_str} = [$stdin, $stdout];
	}
	return ($stdin, $stdout);
}

# Return the first items of the git command line, for instance
# qw(/usr/bin/git --git-dir=/path/to/repo.git).
sub _get_git_cmd {
	my $self = shift;
	return ($self->git_binary || 'git', '--git-dir=' . $self->repo_dir);
}

=item version

Return the output of 'git --version'.

=cut

sub version{
	my $self = shift;
	return $self->cmd_oneline(cmd => ['--version']);
}

=head2 Inspecting the Repository

=item get_sha1 ( EXTENDED_OBJECT_IDENTIFIER )

Look up the object referred to by C<EXTENDED_OBJECT_IDENTIFER> and
return its SHA1 hash in scalar context or its ($sha1, $type, $size) in
list context, or undef or () if the lookup failed.  When passed a SHA1
hash, return undef or () if it doesn't exist in the repository.

C<EXTENDED_OBJECT_IDENTIFER> can refer to a commit, file, tree, or tag
object; see "git help rev-parse", section "Specifying Revisions".

=cut

sub get_sha1 {
	my($self, $object_id) = @_;
	my ($sha1, $type, $size) = $self->cat_file_batch_check($object_id);
	return unless $sha1;  # so you can write '@a = get_sha1($obj) or die'
	return wantarray ? ($sha1, $type, $size) : $sha1;
}

=item get_sha1s ( \@EXTENDED_OBJECT_IDENTIFIERS )

Look up all objects and return a reference to hash from object
identifiers to [$sha1, $type, $size] or [] array references.  This is
a trivial map operation by default but can be more efficient if there
is a caching layer.

=cut

sub get_sha1s {
	my ($self, $object_id_list) = @_;
	return { (map { ($_, [$self->get_sha1($_)]) } @$object_id_list) };
}

=item cat_file ( SHA1 )

Return the ($type, $contents) of the object identified by C<SHA1>, or
undef if C<SHA1> doesn't exist in the repository.

This is a low-level function.

=cut

# TODO: Add optional $file_handle parameter.  Guard against getting
# huge blobs back when we don't expect it (for instance, we could
# limit the size and send SIGPIPE to git-cat-file if we get a blob
# that is too large).

sub cat_file {
	my($self, $sha1) = @_;
	assert_sha1($sha1);

	my($in, $out) = $self->get_bidi_pipe(
		cmd => ['cat-file','--batch'], reuse => 1);
	print $in "$sha1\n" or die 'cannot write to pipe';
	my($ret_sha1, $type, $size) = split ' ', $out->getline;
	return undef if $type eq 'missing';
	$out->read(my $contents, $size);
	$out->getline;  # eat trailing newline
	return ($type, $contents);
}

=item get_commit ( COMMIT_SHA1 )

Return a new Git::Commit object with SHA1 C<COMMIT_SHA1>.

=cut

sub get_commit {
	my($self, $sha1) = @_;
	Git::Commit->new($self, $sha1);
}

=item get_tag ( TAG_SHA1 )

Return a new Git::Tag object with SHA1 C<TAG_SHA1>.

=cut

sub get_tag {
	my($self, $sha1) = @_;
	Git::Tag->new($self, $sha1);
}

=item get_path ( TREE_SHA1, BLOB_SHA1 )

Return the path of the blob identified by C<BLOB_SHA1> in the tree
identified by C<TREE_SHA1>, or undef if the blob does not exist in
the given tree.

=cut

sub get_path {
	my($self, $tree, $blob_sha1) = @_;
	assert_sha1($tree, $blob_sha1);

	# TODO: Turn this into a line-by-line pipe and/or reimplement
	# in terms of recursive ls_tree calls.
	my @lines = split "\n", $self->cmd_output(cmd => ['ls-tree', '-r', '-t', $tree]);
	for (@lines) {
		if (/^[0-9]+ [a-z]+ $blob_sha1\t(.+)$/) {
			return $1;
		}
	}
	return undef;
}

=item ls_tree ( TREE_SHA1 )

Return a reference to an array of five-element arrays [$mode, $type,
$sha1, $blob_size, $name].  $blob_size is an integer for blobs or
undef for tree or commit entries.

=cut

sub ls_tree {
	my($self, $tree) = @_;
	assert_sha1($tree);

	my @lines = split "\n", $self->cmd_output(cmd => ['ls-tree', '--long', $tree]);
	return [map { /([0-9]+) ([a-z]+) ([0-9a-f]{40})\s+([0-9-]+)\t(.+)/;
		      [$1, $2, $3, $4 eq '-' ? undef : int($4), $5] } @lines];
}

=item get_refs ( [PATTERN] )

Return a reference to an array of [$sha1, $object_type, $ref_name]
triples.  If C<PATTERN> is given, only refs matching the pattern are
returned; see "git help for-each-ref" for details.

=cut

sub get_refs {
	my($self, $pattern) = @_;

	return [ map [ split ], $self->cmd_output(
			 cmd => [ 'for-each-ref',
				  defined $pattern ? $pattern : () ]) ];
}

=item name_rev ( COMMITTISH_SHA1 [, TAGS_ONLY] )

Return a symbolic name for the commit identified by
C<COMMITTISH_SHA1>, or undef if no name can be found; see "git help
name-rev" for details.  If C<TAGS_ONLY> is true, no branch names are
used to name the commit.

=cut

# TODO: Use --stdin with bidi pipe.

sub name_rev {
	my($self, $sha1, $tags_only) = @_;
	assert_sha1($sha1);

	my $name = $self->cmd_oneline(
		cmd => [ 'name-rev', $tags_only ? '--tags' : (), '--name-only',
			 $sha1 ]);
	return $name eq 'undefined' ? undef : $name;
}




# TODO: Underscore-prefix the following methods, and exclude them from
# perldoc documentation, so we can change the API in the future?

=head2 Access to low-level Git binary output

=item cat_file_batch_check ( EXTENDED_OBJECT_IDENTIFIER )

Return an array of ($sha1, $type, $size) as it is output by cat-file
--batch-check, or an empty array if the given object cannot be found.

=cut

sub cat_file_batch_check {
	my($self, $object_id) = @_;
	return () if $object_id =~ /\n/;
	my ($in, $out) = $self->get_bidi_pipe(
		cmd => ['cat-file','--batch-check'], reuse => 1);
	print $in "$object_id\n" or die 'cannot write to pipe';
	chomp(my $output = <$out>);
	die 'no output from pipe' unless $output;
	if ($output =~ /missing$/) {
		return ();
	} else {
		$output =~ /^([0-9a-f]{40}) ([a-z]+) ([0-9]+)$/
		    or die "invalid response: $output";
		return ($1, $2, $3);
	}
}


1;
