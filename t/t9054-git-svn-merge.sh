#!/bin/sh

test_description="git remote-svn merge"
. ./lib-git-remote-svn.sh

test_expect_success 'setup branches' '
	echo "#!/bin/sh" > askpass &&
	echo "echo pass" >> askpass &&
	chmod +x askpass &&
	git config core.askpass "$PWD/askpass" &&
	git config "credential.$svnurl.username" committer &&
	git config --add remote.svn.map Trunk:refs/heads/trunk &&
	git config --add remote.svn.map Branches/*:refs/heads/* &&
	git config svn.emptymsg emptymsg &&
	cd svnco &&
	svn_cmd mkdir Branches &&
	svn_cmd mkdir Trunk &&
	echo "init trunk" >> Trunk/file.txt &&
	svn_cmd add Trunk/file.txt &&
	svn_cmd ci -m "svn init" &&
	cd ..
'

test_expect_success 'fetch merge' '
	cd svnco &&
	svn_cmd copy Trunk Branches/feature &&
	svn_cmd ci -m "create feature branch" &&
	echo "feature work" >> Branches/feature/file.txt &&
	svn_cmd ci -m "work on feature" &&
	cd Trunk &&
	svn_cmd merge $svnurl/Branches/feature &&
	svn_cmd ci -m "pull feature into trunk" &&
	cd .. &&
	git fetch -v svn
'
