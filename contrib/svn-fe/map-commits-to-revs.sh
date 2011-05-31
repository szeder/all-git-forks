#!/bin/sh
. "$(git --exec-path)/git-sh-setup"

awk '
	BEGIN {
		print "commit refs/notes/svnrev";
		printf "committer ";
		system("git var GIT_COMMITTER_IDENT");
		print "data <<EOT";
		print "Automatically generated commits-to-revs mapping.";
		print "EOT";
	}
	{
		num = 0 + substr($1, 2);
		commitname = $2;
		if (num < 1024 * 1024 * 1024) {
			print "N inline " commitname
			print "data <<EOT";
			print "r" num;
			print "EOT";
		}
	}
' "$GIT_DIR"/info/fast-import/svnrev |
git fast-import
