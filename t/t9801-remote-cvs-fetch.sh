#!/bin/sh

test_description='git remote-cvs basic tests'
. ./test-lib.sh

if ! test_have_prereq PYTHON; then
	say 'skipping CVS foreign-vcs helper tests, python not available'
	test_done
fi

CVS_EXEC=cvs
CVS_OPTS="-f -q"
CVS="$CVS_EXEC $CVS_OPTS"

CVSROOT=$(pwd)/cvsroot
export CVSROOT
unset CVS_SERVER

CVSMODULE=cvsmodule
GITREMOTE=cvsremote

if ! type $CVS_EXEC >/dev/null 2>&1
then
	say 'skipping remote-cvs tests, $CVS_EXEC not found'
	test_done
fi

verify () {
	git log --reverse --format="--- %T%n%s%n%n%b" "$GITREMOTE/$1" >actual &&
	test_cmp "expect.$1" actual &&
	echo "verify $1" | git remote-cvs "$GITREMOTE"
}

test_expect_success 'setup CVS repo' '$CVS init'

test_expect_success 'create CVS module with initial commit' '

	mkdir "$CVSROOT/$CVSMODULE" &&
	$CVS co -d module-cvs $CVSMODULE &&
	(
		cd module-cvs &&
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
		$CVS commit -f -F message o_fortuna
	)
'

test_expect_success 'set up CVS repo/module as a foreign remote' '

	git config "user.name" "Test User"
	git config "user.email" "test@example.com"
	git config "remote.$GITREMOTE.vcs" cvs
	git config "remote.$GITREMOTE.cvsRoot" "$CVSROOT"
	git config "remote.$GITREMOTE.cvsModule" "$CVSMODULE"
	git config "remote.$GITREMOTE.fetch" \
		"+refs/cvs/$GITREMOTE/*:refs/remotes/$GITREMOTE/*"

'

test_expect_success 'initial fetch from CVS remote' '

	cat <<EOF >expect.HEAD &&
--- 0e06d780dedab23e683c686fb041daa9a84c936c
add "O Fortuna" lyrics

These public domain lyrics make an excellent sample text.

EOF
	git fetch "$GITREMOTE" &&
	verify HEAD

'

test_expect_success 'CVS commit' '

	(
		cd module-cvs &&
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
		$CVS commit -f -F message o_fortuna
	) &&
	cat <<EOF >>expect.HEAD &&
--- daa87269a5e00388135ad9542dc16ab6754466e5
translate to English

My Latin is terrible.

EOF
	git fetch "$GITREMOTE" &&
	verify HEAD

'

test_expect_success 'CVS commit with new file' '

	(
		cd module-cvs &&
		echo 1 >tick &&
		$CVS add tick &&
		$CVS commit -f -m 1 tick
	) &&
	cat <<EOF >>expect.HEAD &&
--- 486935b4fccecea9b64cbed3a797ebbcbe2b7461
1


EOF
	git fetch "$GITREMOTE" &&
	verify HEAD

'

test_expect_success 'fetch without CVS changes' '

	git fetch "$GITREMOTE" &&
	verify HEAD

'

test_expect_success 'add 2 CVS commits' '

	(
		cd module-cvs &&
		echo 2 >tick &&
		$CVS commit -f -m 2 tick &&
		echo 3 >tick &&
		$CVS commit -f -m 3 tick
	) &&
	cat <<EOF >>expect.HEAD &&
--- 83437ab3e57bf0a42915de5310e3419792b5a36f
2


--- 60fc50406a82dc6bd32dc6e5f7bd23e4c3cdf7ef
3


EOF
	git fetch "$GITREMOTE" &&
	verify HEAD

'

test_expect_success 'CVS commit with removed file' '

	(
		cd module-cvs &&
		$CVS remove -f tick &&
		$CVS commit -f -m "remove file" tick
	) &&
	cat <<EOF >>expect.HEAD &&
--- daa87269a5e00388135ad9542dc16ab6754466e5
remove file


EOF
	git fetch "$GITREMOTE" &&
	verify HEAD

'

test_expect_success 'CVS commit with several new files' '

	(
		cd module-cvs &&
		echo spam >spam &&
		echo sausage >sausage &&
		echo eggs >eggs &&
		$CVS add spam sausage eggs &&
		$CVS commit -f -m "spam, sausage, and eggs" spam sausage eggs
	) &&
	cat <<EOF >>expect.HEAD &&
--- 3190dfce44a6d5e9916b4870dbf8f37d1ca4ddaf
spam, sausage, and eggs


EOF
	git fetch "$GITREMOTE" &&
	verify HEAD

'

test_expect_success 'new CVS branch' '

	(
		cd module-cvs &&
		$CVS tag -b foo
	) &&
	cp expect.HEAD expect.foo &&
	git fetch "$GITREMOTE" &&
	verify HEAD &&
	verify foo

'

test_expect_success 'CVS commit on branch' '

	(
		cd module-cvs &&
		$CVS up -r foo &&
		echo "spam spam spam" >spam &&
		$CVS commit -f -m "commit on branch foo" spam
	) &&
	cat <<EOF >>expect.foo &&
--- 1aba123e5c83898ce3a8b976cc6064d60246aef4
commit on branch foo


EOF
	git fetch "$GITREMOTE" &&
	verify HEAD &&
	verify foo

'

test_expect_success 'create CVS tag' '

	(
		cd module-cvs &&
		$CVS tag bar
	) &&
	cp expect.foo expect.bar &&
	git fetch "$GITREMOTE" &&
	verify HEAD &&
	verify foo &&
	verify bar

'

test_expect_success 'another CVS commit on branch' '

	(
		cd module-cvs &&
		echo "spam spam spam spam spam spam" >> spam &&
		$CVS commit -f -m "another commit on branch foo" spam
	) &&
	cat <<EOF >>expect.foo &&
--- 15a2635e76e8e5a5a8746021643de317452f2340
another commit on branch foo


EOF
	git fetch "$GITREMOTE" &&
	verify HEAD &&
	verify foo &&
	verify bar

'

test_done
