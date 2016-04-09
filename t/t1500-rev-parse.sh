#!/bin/sh

test_description='test git rev-parse'
. ./test-lib.sh

test_expect_success 'toplevel: is-bare-repository' '
	test false = "$(git rev-parse --is-bare-repository)"
'

test_expect_success 'toplevel: is-inside-git-dir' '
	test false = "$(git rev-parse --is-inside-git-dir)"
'

test_expect_success 'toplevel: is-inside-work-tree' '
	test true = "$(git rev-parse --is-inside-work-tree)"
'

test_expect_success 'toplevel: prefix' '
	echo >expected &&
	git rev-parse --show-prefix >actual &&
	test_cmp expected actual
'

test_expect_success 'toplevel: git-dir' '
	echo .git >expected &&
	git rev-parse --git-dir >actual &&
	test_cmp expected actual
'

test_expect_success 'toplevel: absolute-git-dir' '
	echo "$(pwd)/.git" >expected &&
	git rev-parse --absolute-git-dir >actual &&
	test_cmp expected actual
'

test_expect_success '.git/: is-bare-repository' '
	(cd .git && test false = "$(git rev-parse --is-bare-repository)")
'

test_expect_success '.git/: is-inside-git-dir' '
	(cd .git && test true = "$(git rev-parse --is-inside-git-dir)")
'

test_expect_success '.git/: is-inside-work-tree' '
	(cd .git && test false = "$(git rev-parse --is-inside-work-tree)")
'

test_expect_success '.git/: prefix' '
	(
		cd .git &&
		echo >expected &&
		git rev-parse --show-prefix >actual &&
		test_cmp expected actual
	)
'

test_expect_success '.git/: git-dir' '
	(
		cd .git &&
		echo . >expected &&
		git rev-parse --git-dir >actual &&
		test_cmp expected actual
	)
'

test_expect_success '.git/: absolute-git-dir' '
	(
		ROOT=$(pwd) &&
		cd .git &&
		echo "$ROOT/.git" >expected &&
		git rev-parse --absolute-git-dir >actual &&
		test_cmp expected actual
	)
'

test_expect_success '.git/objects/: is-bare-repository' '
	(cd .git/objects && test false = "$(git rev-parse --is-bare-repository)")
'

test_expect_success '.git/objects/: is-inside-git-dir' '
	(cd .git/objects && test true = "$(git rev-parse --is-inside-git-dir)")
'

test_expect_success '.git/objects/: is-inside-work-tree' '
	(cd .git/objects && test false = "$(git rev-parse --is-inside-work-tree)")
'

test_expect_success '.git/objects/: prefix' '
	(
		cd .git/objects &&
		echo >expected &&
		git rev-parse --show-prefix >actual &&
		test_cmp expected actual
	)
'

test_expect_success '.git/objects/: git-dir' '
	(
		ROOT=$(pwd) &&
		cd .git/objects &&
		echo $ROOT/.git >expected &&
		git rev-parse --git-dir >actual &&
		test_cmp expected actual
	)
'

test_expect_success '.git/objects/: absolute-git-dir' '
	(
		ROOT=$(pwd) &&
		cd .git/objects &&
		echo "$ROOT/.git" >expected &&
		git rev-parse --absolute-git-dir >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'subdirectory: is-bare-repository' '
	mkdir -p sub/dir &&
	test_when_finished "rm -rf sub" &&
	(cd sub/dir && test false = "$(git rev-parse --is-bare-repository)")
'

test_expect_success 'subdirectory: is-inside-git-dir' '
	mkdir -p sub/dir &&
	test_when_finished "rm -rf sub" &&
	(cd sub/dir && test false = "$(git rev-parse --is-inside-git-dir)")
'

test_expect_success 'subdirectory: is-inside-work-tree' '
	mkdir -p sub/dir &&
	test_when_finished "rm -rf sub" &&
	(cd sub/dir && test true = "$(git rev-parse --is-inside-work-tree)")
'

test_expect_success 'subdirectory: prefix' '
	mkdir -p sub/dir &&
	test_when_finished "rm -rf sub" &&
	(cd sub/dir && test sub/dir/ = "$(git rev-parse --show-prefix)")
'

test_expect_success 'subdirectory: git-dir' '
	mkdir -p sub/dir &&
	test_when_finished "rm -rf sub" &&
	(
		ROOT=$(pwd) &&
		cd sub/dir &&
		echo $ROOT/.git >expected &&
		git rev-parse --git-dir >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'subdirectory: absolute-git-dir' '
	mkdir -p sub/dir &&
	test_when_finished "rm -rf sub" &&
	(
		ROOT=$(pwd) &&
		cd sub/dir &&
		echo $ROOT/.git >expected &&
		git rev-parse --absolute-git-dir >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'core.bare = true: is-bare-repository' '
	git config core.bare true &&
	test_when_finished "git config core.bare false" &&
	test true = "$(git rev-parse --is-bare-repository)"
'

test_expect_success 'core.bare = true: is-inside-git-dir' '
	git config core.bare true &&
	test_when_finished "git config core.bare false" &&
	test false = "$(git rev-parse --is-inside-git-dir)"
'

test_expect_success 'core.bare = true: is-inside-work-tree' '
	git config core.bare true &&
	test_when_finished "git config core.bare false" &&
	test false = "$(git rev-parse --is-inside-work-tree)"
'

test_expect_success 'core.bare undefined: is-bare-repository' '
	git config --unset core.bare &&
	test_when_finished "git config core.bare false" &&
	test false = "$(git rev-parse --is-bare-repository)"
'

test_expect_success 'core.bare undefined: is-inside-git-dir' '
	git config --unset core.bare &&
	test_when_finished "git config core.bare false" &&
	test false = "$(git rev-parse --is-inside-git-dir)"
'

test_expect_success 'core.bare undefined: is-inside-work-tree' '
	git config --unset core.bare &&
	test_when_finished "git config core.bare false" &&
	test true = "$(git rev-parse --is-inside-work-tree)"
'

test_expect_success 'GIT_DIR=../.git, core.bare = false: is-bare-repository' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config
		git config core.bare false &&
		test false = "$(git rev-parse --is-bare-repository)"
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare = false: is-inside-git-dir' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config core.bare false &&
		test false = "$(git rev-parse --is-inside-git-dir)"
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare = false: is-inside-work-tree' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config core.bare false &&
		test true = "$(git rev-parse --is-inside-work-tree)"
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare = false: prefix' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config core.bare false &&
		echo >expected &&
		git rev-parse --show-prefix >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare = false: git-dir' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config core.bare false &&
		echo ../.git >expected &&
		git rev-parse --git-dir >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare = false: absolute-git-dir' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		ROOT=$(pwd) &&
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config core.bare false &&
		echo $ROOT/.git >expected &&
		git rev-parse --absolute-git-dir >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare = true: is-bare-repository' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config core.bare true &&
		test true = "$(git rev-parse --is-bare-repository)"
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare = true: is-inside-git-dir' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config core.bare true &&
		test false = "$(git rev-parse --is-inside-git-dir)"
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare = true: is-inside-work-tree' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config core.bare true &&
		test false = "$(git rev-parse --is-inside-work-tree)"
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare = true: prefix' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config core.bare true &&
		echo >expected &&
		git rev-parse --show-prefix >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare undefined: is-bare-repository' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config --unset core.bare &&
		test false = "$(git rev-parse --is-bare-repository)"
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare undefined: is-inside-git-dir' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config --unset core.bare &&
		test false = "$(git rev-parse --is-inside-git-dir)"
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare undefined: is-inside-work-tree' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config --unset core.bare &&
		test true = "$(git rev-parse --is-inside-work-tree)"
	)
'

test_expect_success 'GIT_DIR=../.git, core.bare undefined: prefix' '
	mkdir work &&
	test_when_finished "rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../.git &&
		export GIT_CONFIG="$(pwd)"/../.git/config &&
		git config --unset core.bare &&
		echo >expected &&
		git rev-parse --show-prefix >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare = false: is-bare-repository' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config
		git config core.bare false &&
		test false = "$(git rev-parse --is-bare-repository)"
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare = false: is-inside-git-dir' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config core.bare false &&
		test false = "$(git rev-parse --is-inside-git-dir)"
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare = false: is-inside-work-tree' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config core.bare false &&
		test true = "$(git rev-parse --is-inside-work-tree)"
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare = false: prefix' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config core.bare false &&
		echo >expected &&
		git rev-parse --show-prefix >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare = false: git-dir' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config core.bare false &&
		echo ../repo.git >expected &&
		git rev-parse --git-dir >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare = false: absolute-git-dir' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		ROOT=$(pwd) &&
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config core.bare false &&
		echo $ROOT/repo.git >expected &&
		git rev-parse --absolute-git-dir >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare = true: is-bare-repository' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config core.bare true &&
		test true = "$(git rev-parse --is-bare-repository)"
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare = true: is-inside-git-dir' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config core.bare true &&
		test false = "$(git rev-parse --is-inside-git-dir)"
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare = true: is-inside-work-tree' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config core.bare true &&
		test false = "$(git rev-parse --is-inside-work-tree)"
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare = true: prefix' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config core.bare true &&
		echo >expected &&
		git rev-parse --show-prefix >actual &&
		test_cmp expected actual
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare undefined: is-bare-repository' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config --unset core.bare &&
		test false = "$(git rev-parse --is-bare-repository)"
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare undefined: is-inside-git-dir' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config --unset core.bare &&
		test false = "$(git rev-parse --is-inside-git-dir)"
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare undefined: is-inside-work-tree' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config --unset core.bare &&
		test true = "$(git rev-parse --is-inside-work-tree)"
	)
'

test_expect_success 'GIT_DIR=../repo.git, core.bare undefined: prefix' '
	mkdir work &&
	cp -r .git repo.git &&
	test_when_finished "rm -r repo.git && rm -rf work && git config core.bare false" &&
	(
		cd work &&
		export GIT_DIR=../repo.git &&
		export GIT_CONFIG="$(pwd)"/../repo.git/config &&
		git config --unset core.bare &&
		echo >expected &&
		git rev-parse --show-prefix >actual &&
		test_cmp expected actual
	)
'

test_done
