#!/bin/sh

test_description='detection and prevention of out-of-tree symlinks'
. ./test-lib.sh

if ! test_have_prereq SYMLINKS
then
	skip_all='skipping external symlink tests (missing SYMLINKS)'
	test_done
fi

create_symlink() {
	symlink=$1
	target=$2
	test_expect_success "create symlink ($symlink)" '
		sha1=$(printf "%s" "$target" | git hash-object -w --stdin) &&
		git update-index --add --cacheinfo "120000,$sha1,$symlink"
	'
}

check_symlink () {
	symlink=$1
	config=$2
	expect=$3

	test_expect_success "check symlink  ($symlink, $config)" "
		rm -f $symlink &&
		git -c core.allowExternalSymlinks=$config \
			checkout-index -- $symlink &&
		$expect $symlink
	"
}

expect_content () {
	echo content >expect &&
	test_cmp expect "$1"
}

expect_target () {
	git cat-file blob :"$1" >expect &&
	test_cmp expect "$1"
}

# we want to try breaking out of the repository,
# so let's work inside a sub-repository, and break
# out to the top-level trash directory
test_expect_success 'set up repository' '
	echo content >target &&
	git init subrepo &&
	cd subrepo &&
	test_commit base &&
	echo content >in-repo-target
'

create_symlink in-repo in-repo-target
check_symlink in-repo false expect_content

create_symlink subdir/in-repo ../in-repo-target
check_symlink subdir/in-repo false expect_content

create_symlink absolute "$TRASH_DIRECTORY/target"
check_symlink absolute true expect_content
check_symlink absolute false expect_target

create_symlink relative "../target"
check_symlink relative true expect_content
check_symlink relative false expect_target

create_symlink curdir .
check_symlink curdir false test_path_is_dir
create_symlink sneaky curdir/../target
check_symlink sneaky true expect_content
check_symlink sneaky false expect_target

test_expect_success 'applying a patch checks symlink config' '
	git diff-index -p --cached HEAD -- relative >patch &&
	rm -f relative &&
	git -c core.allowExternalSymlinks=true apply <patch &&
	expect_content relative &&
	rm -f relative &&
	git -c core.allowExternalSymlinks=false apply <patch &&
	expect_target relative
'

test_expect_success 'merge-recursive checks symlinks config' '
	git reset --hard &&

	# create rename situation which requires processing
	# outside of unpack_trees()
	ln -s ../foo one &&
	git add one &&
	git commit -m base &&

	ln -sf ../target one &&
	git commit -am modify &&

	git checkout -b side HEAD^ &&
	git mv one two &&
	git commit -am rename &&

	git -c core.allowExternalSymlinks=true merge master &&
	expect_content two &&

	git reset --hard HEAD^ &&
	git -c core.allowExternalSymlinks=false merge master &&
	expect_target two
'

test_done
