#!/bin/sh

test_description='checkout can handle submodules'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-submodule-update.sh

test_expect_success 'setup' '
	mkdir submodule &&
	(
		cd submodule &&
		git init &&
		test_commit first
	) &&
	echo first >file &&
	git add file submodule &&
	test_tick &&
	git commit -m superproject &&
	(
		cd submodule &&
		test_commit second
	) &&
	echo second > file &&
	git add file submodule &&
	test_tick &&
	git commit -m updated.superproject
'

test_expect_success '"reset <submodule>" updates the index' '
	git update-index --refresh &&
	git diff-files --quiet &&
	git diff-index --quiet --cached HEAD &&
	git reset HEAD^ submodule &&
	test_must_fail git diff-files --quiet &&
	git reset submodule &&
	git diff-files --quiet
'

test_expect_success '"checkout <submodule>" updates the index only' '
	git update-index --refresh &&
	git diff-files --quiet &&
	git diff-index --quiet --cached HEAD &&
	git checkout HEAD^ submodule &&
	test_must_fail git diff-files --quiet &&
	git checkout HEAD submodule &&
	git diff-files --quiet &&
	git diff-index --quiet --cached HEAD
'

test_expect_success '"checkout <submodule>" honors diff.ignoreSubmodules' '
	git config diff.ignoreSubmodules dirty &&
	echo x> submodule/untracked &&
	git checkout HEAD >actual 2>&1 &&
	! test -s actual
'

test_expect_success '"checkout <submodule>" honors submodule.*.ignore from .gitmodules' '
	git config diff.ignoreSubmodules none &&
	git config -f .gitmodules submodule.submodule.path submodule &&
	git config -f .gitmodules submodule.submodule.ignore untracked &&
	git checkout HEAD >actual 2>&1 &&
	! test -s actual
'

test_expect_success '"checkout <submodule>" honors submodule.*.ignore from .git/config' '
	git config -f .gitmodules submodule.submodule.ignore none &&
	git config submodule.submodule.path submodule &&
	git config submodule.submodule.ignore all &&
	git checkout HEAD >actual 2>&1 &&
	! test -s actual
'

submodule_creation_must_succeed() {
	# checkout base ($1)
	git checkout -f --recurse-submodules $1 &&
	git diff-files --quiet &&
	git diff-index --quiet --cached $1 &&

	# checkout target ($2)
	if test -d submodule; then
		echo change >>submodule/first.t &&
		test_must_fail git checkout --recurse-submodules $2 &&
		git checkout -f --recurse-submodules $2
	else
		git checkout --recurse-submodules $2
	fi &&
	test -e submodule/.git &&
	test -f submodule/first.t &&
	test -f submodule/second.t &&
	git diff-files --quiet &&
	git diff-index --quiet --cached $2
}

test_expect_success 'setup the submodule config' '
	git config -f .gitmodules submodule.submodule.path submodule &&
	git config -f .gitmodules submodule.submodule.url ./submodule.bare &&
	git -C submodule clone --bare . ../submodule.bare &&
	echo submodule.bare >>.gitignore &&
	git add .gitignore .gitmodules submodule &&
	git commit -m "submodule registered with a gitmodules file" &&
	git config submodule.submodule.url ./submodule.bare
'

test_expect_success '"checkout --recurse-submodules" migrates submodule git dir before deleting' '
	git checkout -b base &&
	git checkout -b delete_submodule &&
	git update-index --force-remove submodule &&
	git config -f .gitmodules --unset submodule.submodule.path &&
	git config -f .gitmodules --unset submodule.submodule.url &&
	git add .gitmodules &&
	git commit -m "submodule deleted" &&
	git checkout base 2>output.out &&
	git -C submodule clean -dfx &&
	git checkout --recurse-submodules delete_submodule 2>>output.out &&
	test_i18ngrep "Migrating git directory" output.out &&
	! test -d submodule
'

test_expect_success '"check --recurse-submodules" removes deleted submodule' '
	# Make sure we have the submodule here and ready.
	git checkout base &&
	git submodule update -f . &&
	test -e submodule/.git &&
	git diff-files --quiet &&
	git diff-index --quiet --cached base &&
	git status --porcelain -uno --ignore-submodules=none >status.out &&
	test_must_be_empty status.out &&

	# Check if the checkout deletes the submodule.
	echo change >>submodule/first.t &&
	test_must_fail git checkout --recurse-submodules delete_submodule &&
	grep change submodule/first.t &&
	git checkout -f --recurse-submodules delete_submodule &&
	git diff-files --quiet &&
	git diff-index --quiet --cached delete_submodule &&
	! test -d submodule
'

test_expect_success '"checkout --recurse-submodules" repopulates submodule' '
	submodule_creation_must_succeed delete_submodule base
'

test_expect_success '"checkout --recurse-submodules" repopulates submodule in existing directory' '
	git checkout -f --recurse-submodules delete_submodule &&
	mkdir submodule &&
	submodule_creation_must_succeed delete_submodule base
'

test_expect_success '"checkout --recurse-submodules" replaces submodule with files' '
	git checkout -f base &&
	git checkout -b replace_submodule_with_file &&
	git rm -f submodule &&
	echo "file instead" >submodule &&
	git add submodule &&
	git commit -m "submodule replaced" &&
	git checkout -f base &&
	git submodule update -f . &&
	git checkout --recurse-submodules replace_submodule_with_file &&
	test -f submodule
'

test_expect_success '"checkout --recurse-submodules" removes files and repopulates submodule' '
	submodule_creation_must_succeed replace_submodule_with_file base
'

test_expect_success '"checkout --recurse-submodules" replaces submodule with a directory' '
	git checkout -f base &&
	git checkout -b replace_submodule_with_dir &&
	git rm -f submodule &&
	mkdir -p submodule/dir &&
	echo content >submodule/dir/file &&
	git add submodule &&
	git commit -m "submodule replaced with a directory (file inside)" &&
	git checkout -f base &&
	git submodule update -f . &&
	git checkout --recurse-submodules replace_submodule_with_dir &&
	test -d submodule &&
	! test -e submodule/.git &&
	! test -f submodule/first.t &&
	! test -f submodule/second.t &&
	test -d submodule/dir
'

test_expect_success '"checkout --recurse-submodules" removes the directory and repopulates submodule' '
	submodule_creation_must_succeed replace_submodule_with_dir base
'

test_expect_success SYMLINKS '"checkout --recurse-submodules" replaces submodule with a link' '
	git checkout -f base &&
	git checkout -b replace_submodule_with_link &&
	git rm -f submodule &&
	ln -s submodule &&
	git add submodule &&
	git commit -m "submodule replaced with a link" &&
	git checkout -f base &&
	git submodule update -f . &&
	git checkout --recurse-submodules replace_submodule_with_link &&
	test -L submodule
'

test_expect_success SYMLINKS '"checkout --recurse-submodules" removes the link and repopulates submodule' '
	submodule_creation_must_succeed replace_submodule_with_link base
'

test_expect_success '"checkout --recurse-submodules" updates the submodule' '
	# checkout the base with force to have a safe starting point
	git checkout -f --recurse-submodules base &&
	git -C submodule rev-parse HEAD >sub_HEAD_expected &&

	# setup a new branch with a commit advancing the submodule
	git checkout -b updated_submodule &&
	echo x >>submodule/first.t &&
	git -C submodule add first.t &&
	test_commit -C submodule third &&
	git add submodule &&
	test_tick &&
	git commit -m updated.superproject &&
	git -C submodule rev-parse HEAD >sub_advanced_HEAD_expected &&

	# checkout base again and test for the expectation
	git checkout --recurse-submodules base &&
	git status --porcelain -uno --ignore-submodules=none >out &&
	test_must_be_empty out &&
	git -C submodule rev-parse HEAD >sub_HEAD_actual &&
	test_cmp sub_HEAD_expected sub_HEAD_actual &&

	# switch back again and test
	git checkout  --recurse-submodules updated_submodule &&
	git status --porcelain -uno --ignore-submodules=none >out &&
	test_must_be_empty out &&
	git -C submodule rev-parse HEAD >sub_advanced_HEAD_actual &&
	test_cmp sub_advanced_HEAD_expected sub_advanced_HEAD_actual
'

# In 293ab15eea34 we considered untracked ignored files in submodules
# expendable, we may want to revisit this decision by adding user as
# well as command specific configuration for it.
# When building in-tree the untracked ignored files are probably ok to remove
# in a case as tested here, but e.g. when git.git is a submodule, then a user
# may not want to lose a well crafted (but ignored by default) "config.mak"
# Commands like "git rm" may care less about untracked files in a submodule
# as the checkout command that removes a submodule as well.
test_expect_success 'untracked file is not deleted' '
	git checkout -f --recurse-submodules base &&
	echo important >submodule/untracked &&
	git checkout --recurse-submodules delete_submodule &&
	grep important submodule/untracked
'

test_expect_success 'ignored file works just fine' '
	git checkout -f --recurse-submodules base &&
	echo important >submodule/ignored &&
	echo ignored >.git/modules/submodule/info/exclude &&
	git checkout --recurse-submodules delete_submodule
'

test_expect_success 'dirty file file is not deleted' '
	git checkout -f --recurse-submodules base &&
	echo important >submodule/first.t &&
	test_must_fail git checkout --recurse-submodules delete_submodule &&
	git checkout -f --recurse-submodules delete_submodule
'

test_expect_success 'added to index is not deleted' '
	git checkout -f --recurse-submodules base &&
	echo important >submodule/to_index &&
	git -C submodule add to_index &&
	test_must_fail git checkout --recurse-submodules delete_submodule &&
	git checkout -f --recurse-submodules delete_submodule
'

test_expect_success 'different commit is ok' '
	git checkout -f --recurse-submodules base &&
	echo important >submodule/to_index &&
	git -C submodule add to_index &&
	git -C submodule commit -m "make a different commit" &&
	git checkout --recurse-submodules delete_submodule
'

#~ test_expect_success '"checkout --recurse-submodules" needs -f to update a modifed submodule commit' '
	#~ git -C submodule checkout --recurse-submodules HEAD^ &&
	#~ test_must_fail git checkout --recurse-submodules master &&
	#~ test_must_fail git diff-files --quiet submodule &&
	#~ git diff-files --quiet file &&
	#~ git checkout --recurse-submodules -f master &&
	#~ git diff-files --quiet &&
	#~ git diff-index --quiet --cached HEAD
#~ '

test_expect_success 'setup modification in submodule file in last commit' '
	git checkout -f --recurse-submodules base &&
	echo "new content" >submodule/second.t &&
	git -C submodule commit second.t -m "modify second" &&
	git commit submodule -m "submodule just modified second file."
'

test_expect_success '"checkout --recurse-submodules" needs -f to update modifed submodule content' '
	echo modified >expected &&
	cp expected submodule/second.t &&
	test_must_fail git checkout --recurse-submodules HEAD^ &&
	test_cmp expected submodule/second.t &&
	git checkout --recurse-submodules -f HEAD^ &&
	git diff-files --quiet &&
	git diff-index --quiet --cached HEAD &&
	git checkout --recurse-submodules -f master &&
	git diff-files --quiet &&
	git diff-index --quiet --cached HEAD
'

test_expect_success 'setup unrelated modification in submodule file in last commit' '
	git checkout -f --recurse-submodules base
'

test_expect_success '"checkout --recurse-submodules" ignores modified submodule content that would not be changed' '
	cp submodule/first.t expected2 &&
	echo modified >expected &&
	cp expected submodule/first.t &&
	git checkout --recurse-submodules HEAD^ &&
	test_cmp expected submodule/first.t &&
	git checkout --recurse-submodules -f HEAD &&
	test_cmp expected2 submodule/first.t &&
	git diff-files --quiet &&
	git diff-index --quiet --cached HEAD
'

test_expect_success '"checkout --recurse-submodules" does not care about untracked submodule content' '
	test_when_finished "rm submodule/untracked" &&
	echo untracked >submodule/untracked &&
	git checkout --recurse-submodules master &&
	git diff-files --quiet --ignore-submodules=untracked &&
	git diff-index --quiet --cached HEAD
'

test_expect_success '"checkout --recurse-submodules" needs -f when submodule commit is not present (but does fail anyway)' '
	git checkout --recurse-submodules -b bogus_commit master &&
	git update-index --cacheinfo 160000 0123456789012345678901234567890123456789 submodule &&
	BOGUS_TREE=$(git write-tree) &&
	BOGUS_COMMIT=$(echo "bogus submodule commit" | git commit-tree $BOGUS_TREE) &&
	git commit -m "bogus submodule commit" &&
	git checkout --recurse-submodules -f master &&
	test_must_fail git checkout --recurse-submodules bogus_commit &&
	git diff-files --quiet &&
	test_must_fail git checkout --recurse-submodules -f bogus_commit &&
	test_must_fail git diff-files --quiet submodule &&
	git diff-files --quiet file &&
	git diff-index --quiet --cached HEAD &&
	git checkout --recurse-submodules -f master
'

test_expect_success '-----------------------------' ' true && true '

test_submodule_switch "git checkout"

test_submodule_forced_switch "git checkout -f"

test_done
