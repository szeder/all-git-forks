#!/usr/bin/perl
# Copyright (c) 2009, 2010 David Aguilar
# Copyright (c) 2012 Tim Henigan
#
# This is a wrapper around the GIT_EXTERNAL_DIFF-compatible
# git-difftool--helper script.
#
# This script exports GIT_EXTERNAL_DIFF and GIT_PAGER for use by git.
# The GIT_DIFF* variables are exported for use by git-difftool--helper.
#
# Any arguments that are unknown to this script are forwarded to 'git diff'.

use 5.008;
use strict;
use warnings;
use File::Basename qw(dirname);
use File::Copy;
use File::stat;
use File::Path qw(mkpath);
use File::Temp qw(tempdir);
use Getopt::Long qw(:config pass_through);
use Git;

my @working_tree;
my $rc;
my $repo = Git->repository();
my $repo_path = $repo->repo_path();

sub usage
{
	my $exitcode = shift;
	print << 'USAGE';
usage: git difftool [-t|--tool=<tool>]
                    [-x|--extcmd=<cmd>]
                    [-g|--gui] [--no-gui]
                    [--prompt] [-y|--no-prompt]
                    [-d|--dir-diff]
                    ['git diff' options]
USAGE
	exit($exitcode);
}

sub find_worktree
{
	# Git->repository->wc_path() does not honor changes to the working
	# tree location made by $ENV{GIT_WORK_TREE} or the 'core.worktree'
	# config variable.
	my $worktree;
	my $env_worktree = $ENV{GIT_WORK_TREE};
	my $core_worktree = Git::config('core.worktree');

	if (length($env_worktree) > 0) {
		$worktree = $env_worktree;
	} elsif (length($core_worktree) > 0) {
		$worktree = $core_worktree;
	} else {
		$worktree = $repo->wc_path();
	}

	return $worktree;
}

my $workdir = find_worktree();

sub setup_dir_diff
{
	# Run the diff; exit immediately if no diff found
	# 'Repository' and 'WorkingCopy' must be explicitly set to insure that
	# if $GIT_DIR and $GIT_WORK_TREE are set in ENV, they are actually used
	# by Git->repository->command*.
	my $diffrepo = Git->repository(Repository => $repo_path, WorkingCopy => $workdir);
	my $diffrtn = $diffrepo->command_oneline('diff', '--raw', '--no-abbrev', '-z', @ARGV);
	exit(0) if (length($diffrtn) == 0);

	# Setup temp directories
	my $tmpdir = tempdir('git-diffall.XXXXX', CLEANUP => 1, TMPDIR => 1);
	my $ldir = "$tmpdir/left";
	my $rdir = "$tmpdir/right";
	mkpath($ldir) or die $!;
	mkpath($rdir) or die $!;

	# Build index info for left and right sides of the diff
	my $submodule_mode = "160000";
	my $null_mode = "0" x 6;
	my $null_sha1 = "0" x 40;
	my $lindex = "";
	my $rindex = "";
	my %submodule;
	my @rawdiff = split('\0', $diffrtn);

	for (my $i=0; $i<$#rawdiff; $i+=2) {
		my ($lmode, $rmode, $lsha1, $rsha1, $status) = split(' ', substr($rawdiff[$i], 1));
		my $path = $rawdiff[$i + 1];

		if (($lmode eq $submodule_mode) or ($rmode eq $submodule_mode)) {
			$submodule{$path}{left} = $lsha1;
			if ($lsha1 ne $rsha1) {
				$submodule{$path}{right} = $rsha1;
			} else {
				$submodule{$path}{right} = "$rsha1-dirty";
			}
			next;
		}

		if ($lmode ne $null_mode) {
			$lindex .= "$lmode $lsha1\t$path\0";
		}

		if ($rmode ne $null_mode) {
			if ($rsha1 ne $null_sha1) {
				$rindex .= "$rmode $rsha1\t$path\0";
			} else {
				push(@working_tree, $path);
			}
		}
	}

	# If $GIT_DIR is not set prior to calling 'git update-index' and
	# 'git checkout-index', then those commands will fail if difftool
	# is called from a directory other than the repo root.
	my $must_unset_git_dir = 0;
	if (not defined($ENV{GIT_DIR})) {
		$must_unset_git_dir = 1;
		$ENV{GIT_DIR} = $repo_path;
	}

	# Populate the left and right directories based on each index file
	my ($inpipe, $ctx);
	$ENV{GIT_INDEX_FILE} = "$tmpdir/lindex";
	($inpipe, $ctx) = $repo->command_input_pipe(qw/update-index -z --index-info/);
	print($inpipe $lindex);
	$repo->command_close_pipe($inpipe, $ctx);
	$rc = system('git', 'checkout-index', '--all', "--prefix=$ldir/");
	exit($rc | ($rc >> 8)) if ($rc != 0);

	$ENV{GIT_INDEX_FILE} = "$tmpdir/rindex";
	($inpipe, $ctx) = $repo->command_input_pipe(qw/update-index -z --index-info/);
	print($inpipe $rindex);
	$repo->command_close_pipe($inpipe, $ctx);
	$rc = system('git', 'checkout-index', '--all', "--prefix=$rdir/");
	exit($rc | ($rc >> 8)) if ($rc != 0);

	# If $GIT_DIR was explicitly set just for the update/checkout
	# commands, then it should be unset before continuing.
	delete($ENV{GIT_DIR}) if ($must_unset_git_dir);
	delete($ENV{GIT_INDEX_FILE});

	# Changes in the working tree need special treatment since they are
	# not part of the index
	for my $file (@working_tree) {
		my $dir = dirname($file);
		unless (-d "$rdir/$dir") {
			mkpath("$rdir/$dir") or die $!;
		}
		copy("$workdir/$file", "$rdir/$file") or die $!;
		chmod(stat("$workdir/$file")->mode, "$rdir/$file") or die $!;
	}

	# Changes to submodules require special treatment. This loop writes a
	# temporary file to both the left and right directories to show the
	# change in the recorded SHA1 for the submodule.
	for my $path (keys %submodule) {
		if (defined($submodule{$path}{left})) {
			my $dir = dirname($path);
			unless (-d "$ldir/$dir") {
				mkpath("$ldir/$dir") or die $!;
			}
			open(my $fh, ">", "$ldir/$path") or die $!;
			print($fh "Subproject commit $submodule{$path}{left}");
			close($fh);
		}
		if (defined($submodule{$path}{right})) {
			my $dir = dirname($path);
			unless (-d "$rdir/$dir") {
				mkpath("$rdir/$dir") or die $!;
			}
			open(my $fh, ">", "$rdir/$path") or die $!;
			print($fh "Subproject commit $submodule{$path}{right}");
			close($fh);
		}
	}

	return ($ldir, $rdir);
}

# parse command-line options. all unrecognized options and arguments
# are passed through to the 'git diff' command.
my ($difftool_cmd, $dirdiff, $extcmd, $gui, $help, $prompt);
GetOptions('g|gui!' => \$gui,
	'd|dir-diff' => \$dirdiff,
	'h' => \$help,
	'prompt!' => \$prompt,
	'y' => sub { $prompt = 0; },
	't|tool:s' => \$difftool_cmd,
	'x|extcmd:s' => \$extcmd);

if (defined($help)) {
	usage(0);
}
if (defined($difftool_cmd)) {
	if (length($difftool_cmd) > 0) {
		$ENV{GIT_DIFF_TOOL} = $difftool_cmd;
	} else {
		print "No <tool> given for --tool=<tool>\n";
		usage(1);
	}
}
if (defined($extcmd)) {
	if (length($extcmd) > 0) {
		$ENV{GIT_DIFFTOOL_EXTCMD} = $extcmd;
	} else {
		print "No <cmd> given for --extcmd=<cmd>\n";
		usage(1);
	}
}
if ($gui) {
	my $guitool = "";
	$guitool = Git::config('diff.guitool');
	if (length($guitool) > 0) {
		$ENV{GIT_DIFF_TOOL} = $guitool;
	}
}

# In directory diff mode, 'git-difftool--helper' is called once
# to compare the a/b directories.  In file diff mode, 'git diff'
# will invoke a separate instance of 'git-difftool--helper' for
# each file that changed.
if (defined($dirdiff)) {
	my ($a, $b) = setup_dir_diff();
	if (defined($extcmd)) {
		$rc = system($extcmd, $a, $b);
	} else {
		$ENV{GIT_DIFFTOOL_DIRDIFF} = 'true';
		$rc = system('git', 'difftool--helper', $a, $b);
	}

	exit($rc | ($rc >> 8)) if ($rc != 0);

	# If the diff including working copy files and those
	# files were modified during the diff, then the changes
	# should be copied back to the working tree
	for my $file (@working_tree) {
		copy("$b/$file", "$workdir/$file") or die $!;
		chmod(stat("$b/$file")->mode, "$workdir/$file") or die $!;
	}
} else {
	if (defined($prompt)) {
		if ($prompt) {
			$ENV{GIT_DIFFTOOL_PROMPT} = 'true';
		} else {
			$ENV{GIT_DIFFTOOL_NO_PROMPT} = 'true';
		}
	}

	$ENV{GIT_PAGER} = '';
	$ENV{GIT_EXTERNAL_DIFF} = 'git-difftool--helper';

	# ActiveState Perl for Win32 does not implement POSIX semantics of
	# exec* system call. It just spawns the given executable and finishes
	# the starting program, exiting with code 0.
	# system will at least catch the errors returned by git diff,
	# allowing the caller of git difftool better handling of failures.
	my $rc = system('git', 'diff', @ARGV);
	exit($rc | ($rc >> 8));
}
