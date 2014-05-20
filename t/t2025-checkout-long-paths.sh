#!/bin/sh

test_description='checkout long paths on Windows

Ensures that Git for Windows can deal with long paths (>260) enabled via core.longpaths'

. ./test-lib.sh

if test_have_prereq NOT_MINGW
then
	skip_all='skipping MINGW specific long paths test'
	test_done
fi

test_expect_success setup '
	p=longpathxx && # -> 10
	p=$p$p$p$p$p && # -> 50
	p=$p$p$p$p$p && # -> 250

	path=${p}/longtestfile && # -> 263 (MAX_PATH = 260)

	blob=$(echo foobar | git hash-object -w --stdin) &&

	printf "100644 %s 0\t%s\n" "$blob" "$path" |
	git update-index --add --index-info &&
	git commit -m initial -q
'

test_expect_success 'checkout of long paths without core.longpaths fails' '
	git config core.longpaths false &&
	test_must_fail git checkout -f 2>error &&
	grep -q "Filename too long" error &&
	test_path_is_missing longpa~1/longtestfile
'

test_expect_success 'checkout of long paths with core.longpaths works' '
	git config core.longpaths true &&
	git checkout -f &&
	test_path_is_file longpa~1/longtestfile
'

test_expect_success 'update of long paths' '
	echo frotz >> longpa~1/longtestfile &&
	echo $path > expect &&
	git ls-files -m > actual &&
	test_cmp expect actual &&
	git add $path &&
	git commit -m second &&
	git grep "frotz" HEAD -- $path
'

test_expect_success cleanup '
	# bash cannot delete the trash dir if it contains a long path
	# lets help cleaning up (unless in debug mode)
	test ! -z "$debug" || rm -rf longpa~1
'

# check that the template used in the test won't be too long:
abspath="$(pwd -W)"/testdir
test ${#abspath} -gt 240 ||
test_set_prereq SHORTABSPATH

test_expect_success SHORTABSPATH 'clean up path close to MAX_PATH' '
	p=/123456789abcdef/123456789abcdef/123456789abcdef/123456789abcdef &&
	p=y$p$p$p$p &&
	subdir="x$(echo "$p" | tail -c $((256 - ${#abspath})))" &&
	# Now, $abspath/$subdir has exactly 257 characters, and is inside CWD
	p2="$abspath/$subdir" &&
	test 257 = ${#p2} &&

	# Be careful to overcome path limitations of the MSys tools
	subdir2=${subdir#????????????????????????????????*/} &&
	subdir1="$abspath"/${subdir%/$subdir2} &&
	mkdir -p "$subdir1" &&
	i=0 &&
	while test $i -le 10
	do
		mkdir -p $subdir2 &&
		touch $subdir2/one-file &&
		mv ${subdir2%%/*} "$subdir1/" &&
		subdir2=z${subdir2} &&
		i=$(($i+1)) ||
		exit 1
	done &&

	# now check that git is able to clear the tree:
	(cd testdir &&
	 git init &&
	 git config core.longpaths yes &&
	 git clean -fdx) &&
	test ! -d "$subdir1"
'

test_done
