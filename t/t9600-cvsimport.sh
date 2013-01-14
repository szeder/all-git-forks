#!/bin/sh

test_description='git cvsimport basic tests'
. ./lib-cvs.sh

test_expect_success 'setup cvsroot environment' '
	CVSROOT=$(pwd)/cvsroot &&
	export CVSROOT
'

test_expect_success 'setup cvsroot' '$CVS init'

test_expect_success 'setup a cvs module' '

	mkdir "$CVSROOT/module" &&
	$CVS co -d module-cvs module &&
	(cd module-cvs &&
	cat <<EOF >o_fortuna &&
O Fortuna
velut luna
statu variabilis,

semper crescis
aut decrescis;
vita detestabilis

nunc obdurat
et tunc curat
ludo mentis aciem,

egestatem,
potestatem
dissolvit ut glaciem.
EOF
	$CVS add o_fortuna &&
	cat <<EOF >message &&
add "O Fortuna" lyrics

These public domain lyrics make an excellent sample text.
EOF
	$CVS commit -F message
	)
'

case "$TEST_CVSPS_VERSION" in
3)	import_all= ;;
*)	import_all=-a ;;
esac &&

test_expect_success 'import a trivial module' '
	git cvsimport $import_all -R -z 0 -C module-git module &&
	test_cmp module-cvs/o_fortuna module-git/o_fortuna

'

test_expect_success 'pack refs' '(cd module-git && git gc)'

test_expect_success 'initial import has correct .git/cvs-revisions' '

	(cd module-git &&
	 git log --format="o_fortuna 1.1 %H" -1) > expected &&
	test_cmp expected module-git/.git/cvs-revisions
'

test_expect_success 'update cvs module' '
	(cd module-cvs &&
	cat <<EOF >o_fortuna &&
O Fortune,
like the moon
you are changeable,

ever waxing
and waning;
hateful life

first oppresses
and then soothes
as fancy takes it;

poverty
and power
it melts them like ice.
EOF
	cat <<EOF >message &&
translate to English

My Latin is terrible.
EOF
	$CVS commit -F message
	)
'

test_expect_success 'update git module' '

	(cd module-git &&
	git config cvsimport.trackRevisions true &&
	git cvsimport $import_all -z 0 module &&
	if test "$TEST_CVSPS_VERSION" = 2
	then
		git merge origin
	fi
	) &&
	test_cmp module-cvs/o_fortuna module-git/o_fortuna

'

test_expect_success 'update has correct .git/cvs-revisions' '

	(cd module-git &&
	 git log --format="o_fortuna 1.1 %H" -1 HEAD^ &&
	 git log --format="o_fortuna 1.2 %H" -1 HEAD) > expected &&
	test_cmp expected module-git/.git/cvs-revisions
'

test_expect_success 'update cvs module' '

	(cd module-cvs &&
		echo 1 >tick &&
		$CVS add tick &&
		$CVS commit -m 1
	)
'

test_expect_success 'cvsimport.module config works' '

	(cd module-git &&
		git config cvsimport.module module &&
		git config cvsimport.trackRevisions true &&
		git cvsimport $import_all -z0 &&
		if test "$TEST_CVSPS_VERSION" = 2
		then
			git merge origin
		fi
	) &&
	test_cmp module-cvs/tick module-git/tick

'

test_expect_success 'second update has correct .git/cvs-revisions' '

	(cd module-git &&
	 git log --format="o_fortuna 1.1 %H" -1 HEAD^^ &&
	 git log --format="o_fortuna 1.2 %H" -1 HEAD^
	 git log --format="tick 1.1 %H" -1 HEAD) > expected &&
	test_cmp expected module-git/.git/cvs-revisions
'

test_expect_success 'import from a CVS working tree' '

	$CVS co -d import-from-wt module &&
	(cd import-from-wt &&
		git config cvsimport.trackRevisions false &&
		git cvsimport $import_all -z0 &&
		echo 1 >expect &&
		git log -1 --pretty=format:%s%n >actual &&
		test_cmp actual expect
	)

'

test_expect_success 'no .git/cvs-revisions created by default' '

	! test -e import-from-wt/.git/cvs-revisions

'

test_expect_success 'test entire HEAD' 'test_cmp_branch_tree master'

test_done
