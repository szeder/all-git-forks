#!/usr/bin/perl
use lib (split(/:/, $ENV{GITPERLLIB}));

use warnings;
use strict;

use Test::More qw(no_plan);
use Test::Exception;
use Carp::Always;

use Cwd;
use File::Basename;
use File::Temp;
use File::Spec;
use Data::Dumper; # for debugging

BEGIN { use_ok('Git::Repo') }

our $old_stderr;
sub discard_stderr {
	open our $old_stderr, ">&", STDERR or die "cannot save STDERR";
	close STDERR;
}
sub restore_stderr {
	open STDERR, ">&", $old_stderr or die "cannot restore STDERR";
}

# set up
our $repo_dir = "trash directory";
our $abs_wc_dir = Cwd->cwd;
die "this must be run by calling the t/t97* shell script(s)\n"
    if basename(Cwd->cwd) ne $repo_dir;
ok(our $r = Git::Repo->new(directory => "./.git"), 'open repository');
sub rev_parse {
	my $name = shift;
	chomp(my $sha1 = `git rev-parse $name 2> /dev/null`);
	$sha1 or undef;
}

my @revisions = split /\s/, `git-rev-list --first-parent HEAD`;
my $head = $revisions[0];

# command methods
is($r->cmd_output(cmd => ['cat-file', '-t', 'HEAD']), "commit\n", 'cmd_output: basic');
discard_stderr;
dies_ok { $r->cmd_output(cmd => ['bad-cmd']); } 'cmd_output: die on error';
restore_stderr;
my $bad_output;
lives_ok { $bad_output = $r->cmd_output(
		   cmd => ['rev-parse', '--verify', '--quiet', 'badrev'],
		   max_exit_code => 1); }
    'cmd_output: max_error';
is($bad_output, '', 'cmd_output: return string on non-zero exit');
is($r->cmd_oneline(cmd => ['cat-file', '-t', 'HEAD']), "commit", 'cmd_oneline: basic');
# untested: get_bidi_pipe

# get_sha1
is($r->get_sha1('HEAD'), $head, 'get_sha1: scalar');
is($r->get_sha1('HEAD'), $head, 'get_sha1: scalar, repeated');
my($sha1, $type, $head_size) = $r->get_sha1('HEAD');
is($sha1, $head, 'get_sha1: array (SHA1)');
is($type, 'commit', 'get_sha1: array (commit)');
ok($head_size > 0, 'get_sha1: array (size)');

# get_sha1s
is_deeply($r->get_sha1s(['HEAD', 'nonexistent']),
	  {'HEAD' => [$head, 'commit', $head_size], 'nonexistent' => [] },
	  'get_sha1s: basic');

# cat_file
is_deeply([$r->cat_file($r->get_sha1("$revisions[-1]:file1"))], ['blob', "test file 1\n"], 'cat_file: blob');
is_deeply([$r->cat_file($r->get_sha1("$revisions[-1]:file1"))], ['blob', "test file 1\n"], 'cat_file: blob, repeated');
is($r->cat_file('0' x 40), undef, 'cat_file: non-existent sha1');

# get_commit
isa_ok($r->get_commit($revisions[-1]), 'Git::Commit',
       'get_commit: returns Git::Commit object');

# get_tag
isa_ok($r->get_tag($r->get_sha1('tag-object-1')), 'Git::Tag',
       'get_tag: returns Git::Tag object');

# get_path
is($r->get_path($head, $r->get_sha1('HEAD:directory1/file')),
   'directory1/file', 'get_path: file');
is($r->get_path($head, $r->get_sha1('HEAD:directory1')),
   'directory1', 'get_path: directory');
is($r->get_path($head, '0' x 40), undef, 'get_path: nonexistent');

# ls_tree
our @lstree = @{$r->ls_tree($revisions[-1])};
is_deeply([map { $_->[4] } @lstree],
	  [qw( directory1 directory2 file1 file2 )],
	  'ls_tree: order');
like($lstree[1]->[2], qr/^[0-9a-f]{40}$/, 'ls_tree: sha1');
$lstree[0]->[2] = $lstree[2]->[2] = 'SHA1';
is_deeply($lstree[0], ['040000', 'tree', 'SHA1', undef, 'directory1'],
	  'ls_tree: structure (directories)');
is_deeply($lstree[2], ['100644', 'blob', 'SHA1', 12, 'file1'],
	  'ls_tree: structure (files)');

# get_refs
my @refs = @{$r->get_refs()};
is((grep { $_->[2] eq 'refs/heads/branch-2' } @refs), 1,
   'get_refs: branch existence and uniqueness');
my @branch2_info = @{(grep { $_->[2] eq 'refs/heads/branch-2' } @refs)[0]};
is_deeply([@branch2_info], [$revisions[-2], 'commit', 'refs/heads/branch-2'],
	  'get_heads: sub-array contents');
@refs = @{$r->get_refs('refs/tags')};
ok(@refs, 'get_refs: pattern');
is((grep { $_->[2] eq 'refs/heads/branch-2' } @refs), 0, 'get_refs: pattern');

# name_rev
is($r->name_rev($revisions[-2]), 'branch-2', 'name_rev: branch');
is($r->name_rev($head, 1), undef, 'name_rev: branch, tags only');
is($r->name_rev($revisions[-1]), 'tags/tag-object-1^0', 'name_rev: tag object');
is($r->name_rev($revisions[-1], 1), 'tag-object-1^0', 'name_rev: tag object, tags only');



# Git::Commmit
print "# Git::Commit:\n";

BEGIN { use_ok('Git::Commit') }

my $invalid_commit = Git::Commit->new($r, '0' x 40);
is($invalid_commit->sha1, '0' x 40, 'new, sha1: accept invalid SHA1');
dies_ok { $invalid_commit->tree } 'die on accessing properties of invalid SHA1s';

$invalid_commit = Git::Commit->new($r, $r->get_sha1('HEAD:')); # tree, not commit
dies_ok { $invalid_commit->tree } 'die on accessing properties of non-commit objects';

my $c = Git::Commit->new($r, $revisions[-2]);
is($c->repo, $r, 'repo: basic');
is($c->sha1, $revisions[-2], 'sha1: basic');
is($c->{Git::Commit::_PARENTS}, undef,
   'lazy loading: not loaded after reading SHA1');
is($c->tree, $r->get_sha1("$revisions[-2]:"), 'tree: basic');
ok($c->{Git::Commit::_PARENTS}, 'lazy loading: loaded after reading tree');
is_deeply([$c->parents], [$revisions[-1]], 'parents: basic');
like($c->author, qr/A U Thor <author\@example.com> [0-9]+ \+0000/, 'author: basic');
like($c->committer, qr/C O Mitter <committer\@example.com> [0-9]+ \+0000/, 'committer: basic');
is($c->encoding, undef, 'encoding: undef');
is($c->message, "second commit\n", 'message: basic');
is($c, $c->sha1, 'stringify: basic');

# error handling
dies_ok { Git::Commit->new($r, $r->get_sha1('tag-object-3'))->_load }
    'new: pass tag SHA1 (dies)';
dies_ok { Git::Commit->new($r, '0' x 40)->_load }
    'new: pass invalid SHA1 (dies)';


# Git::Tag
print "# Git::Tag:\n";

BEGIN { use_ok('Git::Tag') }

# We don't test functionality inherited from Git::Object that we
# already tested in the Git::Commit tests.

my $t = Git::Tag->new($r, $r->get_sha1('tag-object-1'));
is($t->tag, 'tag-object-1', 'tag: basic');
is($t->object, $revisions[-1], 'object: basic');
is($t->type, 'commit', 'tag: type');
like($t->tagger, qr/C O Mitter <committer\@example.com> [0-9]+ \+0000/, 'tagger: basic');
is($t->encoding, undef, 'encoding: undef');
is($t->message, "tag message 1\n", 'message: basic');

# error handling
dies_ok { Git::Tag->new($r, $head)->_load } 'new: pass commit SHA1 (dies)';
dies_ok { Git::Tag->new($r, '0' x 40)->_load } 'new: pass invalid SHA1 (dies)';


# Git::RepoRoot
print "# Git::RepoRoot:\n";

BEGIN { use_ok('Git::RepoRoot'); }

my $reporoot = Git::RepoRoot->new(
	directory => File::Spec->catfile($abs_wc_dir, '..'));
is($reporoot->repo(directory => File::Spec->catfile($repo_dir, '.git'))
   ->get_sha1('HEAD'), $head, 'repo: basic');
