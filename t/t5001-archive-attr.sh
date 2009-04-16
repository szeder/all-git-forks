#!/bin/sh

test_description='git archive attribute tests'

. ./test-lib.sh

SUBSTFORMAT=%H%n

test_expect_success 'setup' '
	echo ignored >ignored &&
	echo ignored export-ignore >>.git/info/attributes &&
	git add ignored &&

	echo ignored by tree >ignored-by-tree &&
	echo ignored-by-tree export-ignore >.gitattributes &&
	git add ignored-by-tree .gitattributes &&

	echo ignored by worktree >ignored-by-worktree &&
	echo ignored-by-worktree export-ignore >.gitattributes &&
	git add ignored-by-worktree &&

	printf "A\$Format:%s\$O" "$SUBSTFORMAT" >nosubstfile &&
	printf "A\$Format:%s\$O" "$SUBSTFORMAT" >substfile1 &&
	printf "A not substituted O" >substfile2 &&
	echo "substfile?" export-subst >>.git/info/attributes &&
	git add nosubstfile substfile1 substfile2 &&

	git commit -m.
'

test_expect_success 'git archive' '
	git archive HEAD >archive.tar &&
	(mkdir archive && cd archive && "$TAR" xf -) <archive.tar
'

test_expect_success 'git archive with worktree attributes' '
	git archive --fix-attributes HEAD >worktree.tar &&
	(mkdir worktree && cd worktree && "$TAR" xf -) <worktree.tar
'

test_expect_success 'export-ignore' '
	test ! -e archive/ignored &&
	test ! -e archive/ignored-by-tree &&
	test   -e archive/ignored-by-worktree &&
	test ! -e worktree/ignored &&
	test   -e worktree/ignored-by-tree &&
	test ! -e worktree/ignored-by-worktree
'

test_expect_success 'export-subst' '
	git log "--pretty=format:A${SUBSTFORMAT}O" HEAD >substfile1.expected &&
	test_cmp nosubstfile archive/nosubstfile &&
	test_cmp substfile1.expected archive/substfile1 &&
	test_cmp substfile2 archive/substfile2
'

test_expect_success 'git tar-tree vs. git archive with worktree attributes' '
	git tar-tree HEAD >tar-tree.tar &&
	test_cmp worktree.tar tar-tree.tar
'

test_done
