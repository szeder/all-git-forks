#!/bin/sh
#
# Copyright (c) 2006 Shawn Pearce
#

test_description='Test git update-ref and basic ref logging'
. ./test-lib.sh

Z=$_z40

test_expect_success setup '

	for name in A B C D E F
	do
		test_tick &&
		T=$(git write-tree) &&
		sha1=$(echo $name | git commit-tree $T) &&
		eval $name=$sha1
	done

'

m=refs/heads/master
n_dir=refs/heads/gu
n=$n_dir/fixes

test_expect_success \
	"create $m" \
	"git update-ref $m $A &&
	 test $A"' = $(cat .git/'"$m"')'
test_expect_success \
	"create $m" \
	"git update-ref $m $B $A &&
	 test $B"' = $(cat .git/'"$m"')'
test_expect_success "fail to delete $m with stale ref" '
	test_must_fail git update-ref -d $m $A &&
	test $B = "$(cat .git/$m)"
'
test_expect_success "delete $m" '
	git update-ref -d $m $B &&
	! test -f .git/$m
'
rm -f .git/$m

test_expect_success "delete $m without oldvalue verification" "
	git update-ref $m $A &&
	test $A = \$(cat .git/$m) &&
	git update-ref -d $m &&
	! test -f .git/$m
"
rm -f .git/$m

test_expect_success \
	"fail to create $n" \
	"touch .git/$n_dir &&
	 test_must_fail git update-ref $n $A >out 2>err"
rm -f .git/$n_dir out err

test_expect_success \
	"create $m (by HEAD)" \
	"git update-ref HEAD $A &&
	 test $A"' = $(cat .git/'"$m"')'
test_expect_success \
	"create $m (by HEAD)" \
	"git update-ref HEAD $B $A &&
	 test $B"' = $(cat .git/'"$m"')'
test_expect_success "fail to delete $m (by HEAD) with stale ref" '
	test_must_fail git update-ref -d HEAD $A &&
	test $B = $(cat .git/$m)
'
test_expect_success "delete $m (by HEAD)" '
	git update-ref -d HEAD $B &&
	! test -f .git/$m
'
rm -f .git/$m

test_expect_success \
	"create $m (by HEAD)" \
	"git update-ref HEAD $A &&
	 test $A"' = $(cat .git/'"$m"')'
test_expect_success \
	"pack refs" \
	"git pack-refs --all"
test_expect_success \
	"move $m (by HEAD)" \
	"git update-ref HEAD $B $A &&
	 test $B"' = $(cat .git/'"$m"')'
test_expect_success "delete $m (by HEAD) should remove both packed and loose $m" '
	git update-ref -d HEAD $B &&
	! grep "$m" .git/packed-refs &&
	! test -f .git/$m
'
rm -f .git/$m

cp -f .git/HEAD .git/HEAD.orig
test_expect_success "delete symref without dereference" '
	git update-ref --no-deref -d HEAD &&
	! test -f .git/HEAD
'
cp -f .git/HEAD.orig .git/HEAD

test_expect_success "delete symref without dereference when the referred ref is packed" '
	echo foo >foo.c &&
	git add foo.c &&
	git commit -m foo &&
	git pack-refs --all &&
	git update-ref --no-deref -d HEAD &&
	! test -f .git/HEAD
'
cp -f .git/HEAD.orig .git/HEAD
git update-ref -d $m

test_expect_success '(not) create HEAD with old sha1' "
	test_must_fail git update-ref HEAD $A $B
"
test_expect_success "(not) prior created .git/$m" "
	! test -f .git/$m
"
rm -f .git/$m

test_expect_success \
	"create HEAD" \
	"git update-ref HEAD $A"
test_expect_success '(not) change HEAD with wrong SHA1' "
	test_must_fail git update-ref HEAD $B $Z
"
test_expect_success "(not) changed .git/$m" "
	! test $B"' = $(cat .git/'"$m"')
'
rm -f .git/$m

: a repository with working tree always has reflog these days...
: >.git/logs/refs/heads/master
test_expect_success \
	"create $m (logged by touch)" \
	'GIT_COMMITTER_DATE="2005-05-26 23:30" \
	 git update-ref HEAD '"$A"' -m "Initial Creation" &&
	 test '"$A"' = $(cat .git/'"$m"')'
test_expect_success \
	"update $m (logged by touch)" \
	'GIT_COMMITTER_DATE="2005-05-26 23:31" \
	 git update-ref HEAD'" $B $A "'-m "Switch" &&
	 test '"$B"' = $(cat .git/'"$m"')'
test_expect_success \
	"set $m (logged by touch)" \
	'GIT_COMMITTER_DATE="2005-05-26 23:41" \
	 git update-ref HEAD'" $A &&
	 test $A"' = $(cat .git/'"$m"')'

cat >expect <<EOF
$Z $A $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150200 +0000	Initial Creation
$A $B $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150260 +0000	Switch
$B $A $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150860 +0000
EOF
test_expect_success \
	"verifying $m's log" \
	"test_cmp expect .git/logs/$m"
rm -rf .git/$m .git/logs expect

test_expect_success \
	'enable core.logAllRefUpdates' \
	'git config core.logAllRefUpdates true &&
	 test true = $(git config --bool --get core.logAllRefUpdates)'

test_expect_success \
	"create $m (logged by config)" \
	'GIT_COMMITTER_DATE="2005-05-26 23:32" \
	 git update-ref HEAD'" $A "'-m "Initial Creation" &&
	 test '"$A"' = $(cat .git/'"$m"')'
test_expect_success \
	"update $m (logged by config)" \
	'GIT_COMMITTER_DATE="2005-05-26 23:33" \
	 git update-ref HEAD'" $B $A "'-m "Switch" &&
	 test '"$B"' = $(cat .git/'"$m"')'
test_expect_success \
	"set $m (logged by config)" \
	'GIT_COMMITTER_DATE="2005-05-26 23:43" \
	 git update-ref HEAD '"$A &&
	 test $A"' = $(cat .git/'"$m"')'

cat >expect <<EOF
$Z $A $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150320 +0000	Initial Creation
$A $B $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150380 +0000	Switch
$B $A $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150980 +0000
EOF
test_expect_success \
	"verifying $m's log" \
	'test_cmp expect .git/logs/$m'
rm -f .git/$m .git/logs/$m expect

git update-ref $m $D
cat >.git/logs/$m <<EOF
0000000000000000000000000000000000000000 $C $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150320 -0500
$C $A $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150350 -0500
$A $B $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150380 -0500
$F $Z $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150680 -0500
$Z $E $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150980 -0500
EOF

ed="Thu, 26 May 2005 18:32:00 -0500"
gd="Thu, 26 May 2005 18:33:00 -0500"
ld="Thu, 26 May 2005 18:43:00 -0500"
test_expect_success \
	'Query "master@{May 25 2005}" (before history)' \
	'rm -f o e &&
	 git rev-parse --verify "master@{May 25 2005}" >o 2>e &&
	 test '"$C"' = $(cat o) &&
	 test "warning: Log for '\'master\'' only goes back to $ed." = "$(cat e)"'
test_expect_success \
	"Query master@{2005-05-25} (before history)" \
	'rm -f o e &&
	 git rev-parse --verify master@{2005-05-25} >o 2>e &&
	 test '"$C"' = $(cat o) &&
	 echo test "warning: Log for '\'master\'' only goes back to $ed." = "$(cat e)"'
test_expect_success \
	'Query "master@{May 26 2005 23:31:59}" (1 second before history)' \
	'rm -f o e &&
	 git rev-parse --verify "master@{May 26 2005 23:31:59}" >o 2>e &&
	 test '"$C"' = $(cat o) &&
	 test "warning: Log for '\''master'\'' only goes back to $ed." = "$(cat e)"'
test_expect_success \
	'Query "master@{May 26 2005 23:32:00}" (exactly history start)' \
	'rm -f o e &&
	 git rev-parse --verify "master@{May 26 2005 23:32:00}" >o 2>e &&
	 test '"$C"' = $(cat o) &&
	 test "" = "$(cat e)"'
test_expect_success \
	'Query "master@{May 26 2005 23:32:30}" (first non-creation change)' \
	'rm -f o e &&
	 git rev-parse --verify "master@{May 26 2005 23:32:30}" >o 2>e &&
	 test '"$A"' = $(cat o) &&
	 test "" = "$(cat e)"'
test_expect_success \
	'Query "master@{2005-05-26 23:33:01}" (middle of history with gap)' \
	'rm -f o e &&
	 git rev-parse --verify "master@{2005-05-26 23:33:01}" >o 2>e &&
	 test '"$B"' = $(cat o) &&
	 test "warning: Log .git/logs/'"$m has gap after $gd"'." = "$(cat e)"'
test_expect_success \
	'Query "master@{2005-05-26 23:38:00}" (middle of history)' \
	'rm -f o e &&
	 git rev-parse --verify "master@{2005-05-26 23:38:00}" >o 2>e &&
	 test '"$Z"' = $(cat o) &&
	 test "" = "$(cat e)"'
test_expect_success \
	'Query "master@{2005-05-26 23:43:00}" (exact end of history)' \
	'rm -f o e &&
	 git rev-parse --verify "master@{2005-05-26 23:43:00}" >o 2>e &&
	 test '"$E"' = $(cat o) &&
	 test "" = "$(cat e)"'
test_expect_success \
	'Query "master@{2005-05-28}" (past end of history)' \
	'rm -f o e &&
	 git rev-parse --verify "master@{2005-05-28}" >o 2>e &&
	 test '"$D"' = $(cat o) &&
	 test "warning: Log .git/logs/'"$m unexpectedly ended on $ld"'." = "$(cat e)"'


rm -f .git/$m .git/logs/$m expect

test_expect_success \
    'creating initial files' \
    'echo TEST >F &&
     git add F &&
	 GIT_AUTHOR_DATE="2005-05-26 23:30" \
	 GIT_COMMITTER_DATE="2005-05-26 23:30" git commit -m add -a &&
	 h_TEST=$(git rev-parse --verify HEAD) &&
	 echo The other day this did not work. >M &&
	 echo And then Bob told me how to fix it. >>M &&
	 echo OTHER >F &&
	 GIT_AUTHOR_DATE="2005-05-26 23:41" \
	 GIT_COMMITTER_DATE="2005-05-26 23:41" git commit -F M -a &&
	 h_OTHER=$(git rev-parse --verify HEAD) &&
	 GIT_AUTHOR_DATE="2005-05-26 23:44" \
	 GIT_COMMITTER_DATE="2005-05-26 23:44" git commit --amend &&
	 h_FIXED=$(git rev-parse --verify HEAD) &&
	 echo Merged initial commit and a later commit. >M &&
	 echo $h_TEST >.git/MERGE_HEAD &&
	 GIT_AUTHOR_DATE="2005-05-26 23:45" \
	 GIT_COMMITTER_DATE="2005-05-26 23:45" git commit -F M &&
	 h_MERGED=$(git rev-parse --verify HEAD) &&
	 rm -f M'

cat >expect <<EOF
$Z $h_TEST $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150200 +0000	commit (initial): add
$h_TEST $h_OTHER $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117150860 +0000	commit: The other day this did not work.
$h_OTHER $h_FIXED $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117151040 +0000	commit (amend): The other day this did not work.
$h_FIXED $h_MERGED $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1117151100 +0000	commit (merge): Merged initial commit and a later commit.
EOF
test_expect_success \
	'git commit logged updates' \
	"test_cmp expect .git/logs/$m"
unset h_TEST h_OTHER h_FIXED h_MERGED

test_expect_success \
	'git cat-file blob master:F (expect OTHER)' \
	'test OTHER = $(git cat-file blob master:F)'
test_expect_success \
	'git cat-file blob master@{2005-05-26 23:30}:F (expect TEST)' \
	'test TEST = $(git cat-file blob "master@{2005-05-26 23:30}:F")'
test_expect_success \
	'git cat-file blob master@{2005-05-26 23:42}:F (expect OTHER)' \
	'test OTHER = $(git cat-file blob "master@{2005-05-26 23:42}:F")'

a=refs/heads/a
b=refs/heads/b
c=refs/heads/c
E='""'
pws='path with space'

print_nul() {
	while test $# -gt 0; do
		printf -- "$1" &&
		printf -- "Q" | q_to_nul &&
		shift || return
	done
}

test_expect_success '-z fails without --stdin' '
	test_must_fail git update-ref -z $m $m $m 2>err &&
	grep "usage: git update-ref" err
'

test_expect_success 'stdin test setup' '
	echo "$pws" >"$pws" &&
	git add -- "$pws" &&
	git commit -m "$pws"
'

test_expect_success 'stdin works with no input' '
	>stdin &&
	git update-ref --stdin <stdin &&
	git rev-parse --verify -q $m
'

test_expect_success 'stdin works with whitespace-only input' '
	echo " " >stdin &&
	git update-ref --stdin <stdin 2>err &&
	git rev-parse --verify -q $m
'

test_expect_success 'stdin fails on bad input line with only --' '
	echo "--" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: input line with no ref" err
'

test_expect_success 'stdin fails on bad input line with only --bad-option' '
	echo "--bad-option" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: unknown option --bad-option" err
'

test_expect_success 'stdin fails on bad ref name' '
	echo "~a $m" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: invalid ref format: ~a" err
'

test_expect_success 'stdin fails on badly quoted input' '
	echo "$a \"master" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: badly quoted argument: \\\"master" err
'

test_expect_success 'stdin fails on bad input line with too many arguments' '
	echo "$a $m $m $m" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: too many arguments for ref $a" err
'

test_expect_success 'stdin fails on bad input line with too few arguments' '
	echo "$a" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: missing new value for ref $a" err
'

test_expect_success 'stdin fails with duplicate refs' '
	cat >stdin <<-EOF &&
	$a $m
	$b $m
	$a $m
	EOF
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: Multiple updates for ref '"'"'$a'"'"' not allowed." err
'

test_expect_success 'stdin create ref works with no old value' '
	echo "$a $m" >stdin &&
	git update-ref --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin create ref works with zero old value' '
	echo "$b $m $Z" >stdin &&
	git update-ref --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual &&
	git update-ref -d $b
'

test_expect_success 'stdin create ref works with empty old value' '
	echo "$b $m $E" >stdin &&
	git update-ref --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin create ref works with path with space to blob' '
	echo "refs/blobs/pws \"$m:$pws\"" >stdin &&
	git update-ref --stdin <stdin &&
	git rev-parse "$m:$pws" >expect &&
	git rev-parse refs/blobs/pws >actual &&
	test_cmp expect actual &&
	git update-ref -d refs/blobs/pws
'

test_expect_success 'stdin create ref fails with wrong old value' '
	echo "$c $m $m~1" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: Cannot lock the ref '"'"'$c'"'"'" err &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin create ref fails with bad old value' '
	echo "$c $m does-not-exist" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: invalid old value for ref $c: does-not-exist" err &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin create ref fails with bad new value' '
	echo "$c does-not-exist" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: invalid new value for ref $c: does-not-exist" err &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin update ref works with right old value' '
	echo "$b $m~1 $m" >stdin &&
	git update-ref --stdin <stdin &&
	git rev-parse $m~1 >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin update ref fails with wrong old value' '
	echo "$b $m~1 $m" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: Cannot lock the ref '"'"'$b'"'"'" err &&
	git rev-parse $m~1 >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin delete ref fails with wrong old value' '
	echo "$a $E $m~1" >stdin &&
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: Cannot lock the ref '"'"'$a'"'"'" err &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin update symref works with --no-deref' '
	git symbolic-ref TESTSYMREF $b &&
	echo "--no-deref TESTSYMREF $a $b" >stdin &&
	git update-ref --stdin <stdin &&
	git rev-parse TESTSYMREF >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual &&
	git rev-parse $m~1 >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin delete symref works with --no-deref' '
	git symbolic-ref TESTSYMREF $b &&
	echo "--no-deref TESTSYMREF $E $b" >stdin &&
	git update-ref --stdin <stdin &&
	test_must_fail git rev-parse --verify -q TESTSYMREF &&
	git rev-parse $m~1 >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin delete ref works with right old value' '
	echo "$b $E $m~1" >stdin &&
	git update-ref --stdin <stdin &&
	test_must_fail git rev-parse --verify -q $b
'

test_expect_success 'stdin create refs works with some old values' '
	cat >stdin <<-EOF &&
	$a $m
	$b $m $Z
	$c $Z $Z
	EOF
	git update-ref --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual &&
	git rev-parse $b >actual &&
	test_cmp expect actual &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin update refs works with identity updates' '
	cat >stdin <<-EOF &&
	$a $m $m
	$b $m $m
	$c $Z $E
	EOF
	git update-ref --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual &&
	git rev-parse $b >actual &&
	test_cmp expect actual &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin update refs works with extra whitespace' '
	cat >stdin <<-EOF &&
	''
	$a $m $m
	''
	 "$b"  $m $m ''
	''
	-- $c  $Z  $E  ''
	EOF
	git update-ref --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual &&
	git rev-parse $b >actual &&
	test_cmp expect actual &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin update refs fails with wrong old value' '
	git update-ref $c $m &&
	cat >stdin <<-EOF &&
	$a $m $m
	$b $m $m
	$c $E $E
	EOF
	test_must_fail git update-ref --stdin <stdin 2>err &&
	grep "fatal: Cannot lock the ref '"'"'$c'"'"'" err &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual &&
	git rev-parse $b >actual &&
	test_cmp expect actual &&
	git rev-parse $c >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin delete refs works with packed and loose refs' '
	git pack-refs --all &&
	git update-ref $c $m~1 &&
	cat >stdin <<-EOF &&
	$a $Z $m
	$b $Z $m
	$c $E $m~1
	EOF
	git update-ref --stdin <stdin &&
	test_must_fail git rev-parse --verify -q $a &&
	test_must_fail git rev-parse --verify -q $b &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin -z works on empty input' '
	>stdin &&
	git update-ref -z --stdin <stdin &&
	git rev-parse --verify -q $m
'

test_expect_success 'stdin -z works on empty input sequence' '
	print_nul "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	git rev-parse --verify -q $m
'

test_expect_success 'stdin -z fails on unterminated input sequence' '
	print_nul "$a" "$m" >stdin &&
	test_must_fail git update-ref -z --stdin <stdin 2>err &&
	grep "fatal: unterminated -z input sequence" err
'

test_expect_success 'stdin -z create ref works with no old value' '
	print_nul "$a" "$m" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin -z create ref works with zero old value' '
	print_nul "$b" "$m" "$Z" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual &&
	git update-ref -d $b
'

test_expect_success 'stdin -z create ref works with empty old value' '
	print_nul "$b" "$m" "" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin -z create ref works with path with space to blob' '
	print_nul "refs/blobs/pws" "$m:$pws" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	git rev-parse "$m:$pws" >expect &&
	git rev-parse refs/blobs/pws >actual &&
	test_cmp expect actual &&
	git update-ref -d refs/blobs/pws
'

test_expect_success 'stdin -z create ref fails with wrong old value' '
	print_nul "$c" "$m" "$m~1" "\n" >stdin &&
	test_must_fail git update-ref -z --stdin <stdin 2>err &&
	grep "fatal: Cannot lock the ref '"'"'$c'"'"'" err &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin -z create ref fails with bad old value' '
	print_nul "$c" "$m" "does-not-exist" "\n" >stdin &&
	test_must_fail git update-ref -z --stdin <stdin 2>err &&
	grep "fatal: invalid old value for ref $c: does-not-exist" err &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin -z create ref fails with bad new value' '
	print_nul "$c" "does-not-exist" "\n" >stdin &&
	test_must_fail git update-ref -z --stdin <stdin 2>err &&
	grep "fatal: invalid new value for ref $c: does-not-exist" err &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin -z update ref works with right old value' '
	print_nul "$b" "$m~1" "$m" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	git rev-parse $m~1 >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin -z update ref fails with wrong old value' '
	print_nul "$b" "$m~1" "$m" "\n" >stdin &&
	test_must_fail git update-ref -z --stdin <stdin 2>err &&
	grep "fatal: Cannot lock the ref '"'"'$b'"'"'" err &&
	git rev-parse $m~1 >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin -z delete ref fails with wrong old value' '
	print_nul "$a" "" "$m~1" "\n" >stdin &&
	test_must_fail git update-ref -z --stdin <stdin 2>err &&
	grep "fatal: Cannot lock the ref '"'"'$a'"'"'" err &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin -z update symref works with --no-deref' '
	git symbolic-ref TESTSYMREF $b &&
	print_nul "--no-deref" "TESTSYMREF" "$a" "$b" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	git rev-parse TESTSYMREF >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual &&
	git rev-parse $m~1 >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin -z delete symref works with --no-deref' '
	git symbolic-ref TESTSYMREF $b &&
	print_nul "--no-deref" "TESTSYMREF" "" "$b" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	test_must_fail git rev-parse --verify -q TESTSYMREF &&
	git rev-parse $m~1 >expect &&
	git rev-parse $b >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin -z delete ref works with right old value' '
	print_nul "$b" "" "$m~1" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	test_must_fail git rev-parse --verify -q $b
'

test_expect_success 'stdin -z create refs works with some old values' '
	print_nul "$a" "$m" "\n" "$b" "$m" "$Z" "\n" "$c" "$Z" "$Z" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual &&
	git rev-parse $b >actual &&
	test_cmp expect actual &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin -z update refs works with identity updates' '
	print_nul "$a" "$m" "$m" "\n" "$b" "$m" "$m" "\n" "$c" "$Z" "" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual &&
	git rev-parse $b >actual &&
	test_cmp expect actual &&
	test_must_fail git rev-parse --verify -q $c
'

test_expect_success 'stdin -z update refs fails with wrong old value' '
	git update-ref $c $m &&
	print_nul "$a" "$m" "$m" "\n" "$b" "$m" "$m" "\n" "$c" "" "" "\n" >stdin &&
	test_must_fail git update-ref -z --stdin <stdin 2>err &&
	grep "fatal: Cannot lock the ref '"'"'$c'"'"'" err &&
	git rev-parse $m >expect &&
	git rev-parse $a >actual &&
	test_cmp expect actual &&
	git rev-parse $b >actual &&
	test_cmp expect actual &&
	git rev-parse $c >actual &&
	test_cmp expect actual
'

test_expect_success 'stdin -z delete refs works with packed and loose refs' '
	git pack-refs --all &&
	git update-ref $c $m~1 &&
	print_nul "$a" "$Z" "$m" "\n" "$b" "$Z" "$m" "\n" "$c" "" "$m~1" "\n" >stdin &&
	git update-ref -z --stdin <stdin &&
	test_must_fail git rev-parse --verify -q $a &&
	test_must_fail git rev-parse --verify -q $b &&
	test_must_fail git rev-parse --verify -q $c
'

test_done
