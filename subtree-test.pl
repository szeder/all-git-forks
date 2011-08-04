#!/usr/bin/perl
#
# subtree-test.pl: Create simple repositories and test git subtree commands
#

use File::Path 'rmtree';
use File::Basename;

$start_time = 1312582380;
$increment_time = 60*60+60+1; #1 hr, 1 min, 1 sec

# NAME, PARENT(S), PATH(S), BRANCH
@commits = (
	['A',	[], 			['foo.txt', 'A.txt']],
	['B', 	['A'], 			['foo.txt', 'B.txt', 'folder1/B.txt']],
	['C', 	['A'], 			['foo.txt', 'C.txt', 'folder1/C.txt']],
	['D', 	['B', 'C'],		['foo.txt', 'D.txt', 'folder1/D.txt'], 'both-branch-subfolder-change'],
	['E', 	['D'],			['foo.txt', 'E.txt', 'folder2/E.txt']],
	['F', 	['D'],			['foo.txt', 'F.txt']],
	['G', 	['E', 'F'],		['foo.txt', 'G.txt', 'folder2/G.txt'], 'no-subfolder-change-except-merge'],
	['H',	['E', 'F'],		['foo.txt', 'H.txt'], 'no-subfolder-change'],
	['I',	['H'],			['foo.txt', 'I.txt', 'folder2/I.txt']],
	['J',	['I'],			['foo.txt', 'J.txt', 'folder1/J.txt', 'folder2/J.txt'], 'master'],
	
	['A',	[], 			['foo.txt', 'A.txt']],
	['B', 	['A'], 			['foo.txt', 'B.txt', 'folder1/B.txt']],
	['C', 	['A'], 			['foo.txt', 'C.txt', 'folder1/C.txt']],
	['D', 	['B'],			['foo.txt', 'D.txt']],
	['E', 	['D', 'C'],		['foo.txt', 'E.txt', 'folder1/E.txt']],
	['F', 	['D'],			['foo.txt', 'F.txt', 'folder1/F.txt']],
	['G', 	['E'],			['foo.txt', 'G.txt']],
	['H',	['F', 'G'],		['foo.txt', 'H.txt'], 'merge-both-ways'],
	['I', 	['D'],			['foo.txt', 'I.txt']],
	['J',	['I', 'G'],		['foo.txt', 'J.txt']],
	['K', 	['J'],			['foo.txt', 'K.txt', 'folder1/K.txt'], 'merge-both-ways-no-changes'],
	);

my %commit_shas;

# TODO: Remove this, or at least prompt?
rmtree([ 'subtree-test' ]);

mkdir 'subtree-test' or die $!;
chdir 'subtree-test';
system('git init');
system('git config core.autocrlf false');
system('git config core.safecrlf false');

# Create the commits according to the @commits structure	
for $i ( 0 .. $#commits ) {
	$start_time = $start_time + $increment_time;
	$name = $commits[$i][0];
	$parents = $commits[$i][1];
	$folders = $commits[$i][2];
	$branch = $commits[$i][3];
	
	$commit_msg = "$name\n\n";
	print "Create: $name\n";
	
	$parent_list = "";
	$parent_index = 0;
	foreach my $parent (@$parents)
	{
		$parent_sha = $commit_shas{$parent};
		print "\tParent $parent -> $parent_sha\n";
		$commit_msg = $commit_msg . "parent: $parent\n";
		$parent_list = "$parent_list -p $parent_sha";
		
		if ($parent_index == 0)
		{
			`git checkout -q -f $parent_sha`;
		}
		else
		{
			`git merge $parent_sha --no-ff -srecursive -Xours`;
		}
		
		$parent_index++;
	}
	if ($parent_index == 0)
	{
		`git rm -rf *`;
	}
	
	foreach my $filename (@$folders)
	{
		print "\tCreate: $filename\n";
		$commit_msg = $commit_msg . "file: $filename\n";
		
		mkdir dirname($filename);
		open FILE, ">>$filename" or die $!;
		print FILE "Commit $name\n";
		close FILE;
		system("git add $filename");
	}
	
	$tree_sha = `git write-tree`;
	chomp( $tree_sha );	
	print "\tCreated tree $tree_sha\n";
		
	$ENV{'GIT_AUTHOR_NAME'}		= 'Subtree Test';
	$ENV{'GIT_AUTHOR_EMAIL'}	= 'Subtree Test';
	$ENV{'GIT_AUTHOR_DATE'}		= $start_time;
	$ENV{'GIT_COMMITTER_NAME'}	= $ENV{'GIT_AUTHOR_NAME'};	
	$ENV{'GIT_COMMITTER_EMAIL'}	= $ENV{'GIT_AUTHOR_EMAIL'};
	$ENV{'GIT_COMMITTER_DATE'}	= $ENV{'GIT_AUTHOR_DATE'};
	
	$commit_sha = `echo "$commit_msg" | git commit-tree $tree_sha $parent_list`;
	chomp( $commit_sha );
	print "\tCreated commit $commit_sha\n";
	$commit_shas{$name} = $commit_sha;
	
	if ($branch)
	{
		`git branch $branch $commit_sha -f`;
	}
}