=head1 NAME

Git::RepoRoot - A root directory containing Git repositories.

=cut


use strict;
use warnings;
use 5.6.0;


package Git::RepoRoot;

use File::Spec;

use Git::Repo;

our (@EXPORT, @EXPORT_OK);

use base qw( Exporter );

@EXPORT = qw( );
@EXPORT_OK = qw ( );

=head Git::RepoRoot

Git::RepoRoot represents a root directory in which mutiple
repositories are stored.

=item new ( OPTIONS )

Return a new Git::RepoRoot object.  The following options are
supported:

B<directory> - The directory holding all repositories.

Example:

    $repo_root = Git::RepoRoot->new(directory => '/pub/git');

=cut

sub new {
	my $class = shift;
	Git::Repo::assert_opts(@_);
	my $self = {@_};
	bless $self, $class;
	Git::Repo::assert defined($self->{directory});
	return $self;
}

=item repo ( OPTIONS )

Return a new Git::Repo object.  The following options are supported:

B<directory> - The path of the repository relative to the repository
root.

All other options are passed to Git::Repo unchanged.

=cut

sub repo {
	my $self = shift;
	Git::Repo::assert_opts(@_);
	my %opts = (%$self, @_);
	Git::Repo::assert $opts{directory};
	die "you passed an absolute path ($opts{directory})"
	    if substr($opts{directory}, 0, 1) eq '/';
	$opts{directory} = File::Spec->catfile($self->{directory}, $opts{directory});
	return Git::Repo->new(%opts);
}

1;

