#!/usr/bin/perl

# Copyright (C) 2007 Johannes E. Schindelin
#
# A while ago some funny guy asked if you can set the mtime of all files
# to the timestamp of the commit which touched them last.
#
# This is, of course, possible, even if it is highly unlikely that the
# operation makes any sense.  Since I think this was a typical XY problem
# (the guy asking for a solution, not stating the problem, which would
# most likely have another solution, a proper one at that) I call this
# script "xy-status".
#
# But the challenge was not lost on me, so here it goes.

%attributions = ();
@files = ();

open IN, "git ls-tree -r HEAD |";
while (<IN>) {
	if (/^\S+\s+blob \S+\s+(\S+)$/) {
		$files[$#files + 1] = $1;
		$attributions{$1} = -1;
	}
}
close IN;

$remaining = $#files + 1;

open IN, "git log -r --root --raw --no-abbrev --pretty=format:%h~%an~%ad~ |";
while (<IN>) {
	if (/^([^:~]+)~(.*)~([^~]+)~$/) {
		($commit, $author, $date) = ($1, $2, $3);
	} elsif (/^:\S+\s+1\S+\s+\S+\s+\S+\s+\S\s+(.*)$/) {
		if ($attributions{$1} == -1) {
			$attributions{$1} = "$author, $date ($commit)";
			$remaining--;
			if ($remaining <= 0) {
				break;
			}
		}
	}
}
close IN;

foreach $f (@files) {
	print "$f	$attributions{$f}\n";
}

