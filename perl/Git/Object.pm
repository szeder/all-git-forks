=head1 NAME

Git::Object - Object-oriented interface to Git objects (base class).

=cut


use strict;
use warnings;
use 5.6.0;


package Git::Object;

use Git::Repo qw( assert assert_sha1 assert_opts );  # todo: move those

our (@EXPORT, @EXPORT_OK);

use base qw( Exporter );

@EXPORT = qw( );
@EXPORT_OK = qw ( );

use overload
    '""' => \&stringify;

# Hash indices:
# tags, commits, trees
use constant _REPO => 'R';
use constant _SHA1 => 'H';

=item new ( REPO, SHA1 )

Create a new Git::Object object for the object with C<SHA1> in the
repository C<REPO> (Git::Repo).

Note that C<SHA1> must be the SHA1 of a commit object, not a tag
object, and that it must exist in the repository if you plan to use
any methods other than repo and SHA1.

=cut

sub new {
	my($class, $repo, $sha1) = @_;
	assert(ref $repo);
	assert_sha1($sha1);
	my $self = {_REPO() => $repo, _SHA1() => $sha1};
	bless $self, $class;
	return $self;
}

=item repo

Return the Git::Repo object this object was instantiated with.

=cut

sub repo {
	shift->{_REPO()}
}

=item sha1 ()

Return the SHA1 of this object.

=cut

sub sha1 {
	shift->{_SHA1()}
}

sub stringify {
	shift->sha1
}


1;
