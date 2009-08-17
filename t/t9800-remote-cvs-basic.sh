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

test_expect_success 'setup cvsroot' '$CVS init'

test_expect_success '#1: setup a cvs module' '

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

test_expect_success 'set up CVS repo as a foreign remote' '

	git config "user.name" "Test User"
	git config "user.email" "test@example.com"
	git config "remote.$GITREMOTE.vcs" cvs
	git config "remote.$GITREMOTE.cvsRoot" "$CVSROOT"
	git config "remote.$GITREMOTE.cvsModule" "$CVSMODULE"
	git config "remote.$GITREMOTE.fetch" \
		"+refs/cvs/$GITREMOTE/*:refs/remotes/$GITREMOTE/*"

'

test_expect_success '#1: git-remote-cvs "capabilities" command' '

	echo "capabilities" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
import
marks .git/info/cvs/$GITREMOTE/marks

EOF
	test_cmp expect actual

'

test_expect_success '#1: git-remote-cvs "list" command' '

	echo "list" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
? refs/cvs/$GITREMOTE/HEAD

EOF
	test_cmp expect actual

'

test_expect_success '#1: git-remote-cvs "import" command' '

	echo "import refs/cvs/$GITREMOTE/HEAD" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
# Importing CVS revision o_fortuna:1.1
blob
mark :1
data 180
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

commit refs/cvs/$GITREMOTE/HEAD
mark :2
data 82
add "O Fortuna" lyrics

These public domain lyrics make an excellent sample text.

M 644 :1 o_fortuna

# Importing note for object 2
blob
mark :3
data 14
o_fortuna:1.1

commit refs/notes/cvs/$GITREMOTE
mark :4
data 46
Annotate commits imported by "git remote-cvs"

N :3 :2

blob
mark :5
data 32
1 o_fortuna:1.1
2 o_fortuna:1.1

blob
mark :6
data 16
blob 1
commit 2

commit refs/cvs/$GITREMOTE/_metadata
mark :7
data 42
Updated metadata used by "git remote-cvs"

M 644 :5 CVS/marks
M 644 :6 o_fortuna/1.1

EOF
	grep -v "^committer " actual > actual.filtered &&
	test_cmp expect actual.filtered

'

test_expect_success '#1: Passing git-remote-cvs output to git-fast-import' '

	git fast-import --quiet \
		--export-marks=".git/info/cvs/$GITREMOTE/marks" \
		< actual &&
	git gc

'

test_expect_success '#1: Verifying correctness of import' '

	echo "verify HEAD" | git remote-cvs "$GITREMOTE"

'

test_expect_success '#2: update cvs module' '

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
	)
'

test_expect_success '#2: git-remote-cvs "capabilities" command' '

	echo "capabilities" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
import
marks .git/info/cvs/$GITREMOTE/marks

EOF
	test_cmp expect actual

'

test_expect_success '#2: git-remote-cvs "list" command' '

	echo "list" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
? refs/cvs/$GITREMOTE/HEAD

EOF
	test_cmp expect actual

'

test_expect_success '#2: git-remote-cvs "import" command' '

	echo "import refs/cvs/$GITREMOTE/HEAD" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
# Importing CVS revision o_fortuna:1.2
blob
mark :8
data 179
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

commit refs/cvs/$GITREMOTE/HEAD
mark :9
data 44
translate to English

My Latin is terrible.

from refs/cvs/$GITREMOTE/HEAD^0
M 644 :8 o_fortuna

# Importing note for object 9
blob
mark :10
data 14
o_fortuna:1.2

commit refs/notes/cvs/$GITREMOTE
mark :11
data 46
Annotate commits imported by "git remote-cvs"

from refs/notes/cvs/$GITREMOTE^0
N :10 :9

blob
mark :12
data 32
8 o_fortuna:1.2
9 o_fortuna:1.2

blob
mark :13
data 94

blob
mark :14
data 16
blob 8
commit 9

commit refs/cvs/$GITREMOTE/_metadata
mark :15
data 42
Updated metadata used by "git remote-cvs"

from refs/cvs/$GITREMOTE/_metadata^0
M 644 :12 CVS/marks
M 644 :13 o_fortuna/1.1
M 644 :14 o_fortuna/1.2

EOF
	grep -v -e "^committer " -e "\b[0-9a-f]\{40\}\b" actual > actual.filtered &&
	test_cmp expect actual.filtered

'

test_expect_success '#2: Passing git-remote-cvs output to git-fast-import' '

	git fast-import --quiet \
		--import-marks=".git/info/cvs/$GITREMOTE/marks" \
		--export-marks=".git/info/cvs/$GITREMOTE/marks" \
		< actual &&
	git gc

'

test_expect_success '#2: Verifying correctness of import' '

	echo "verify HEAD" | git remote-cvs "$GITREMOTE"

'

test_expect_success '#3: update cvs module' '

	(
		cd module-cvs &&
		echo 1 >tick &&
		$CVS add tick &&
		$CVS commit -f -m 1 tick
	)

'

test_expect_success '#3: git-remote-cvs "capabilities" command' '

	echo "capabilities" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
import
marks .git/info/cvs/$GITREMOTE/marks

EOF
	test_cmp expect actual

'

test_expect_success '#3: git-remote-cvs "list" command' '

	echo "list" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
? refs/cvs/$GITREMOTE/HEAD

EOF
	test_cmp expect actual

'

test_expect_success '#3: git-remote-cvs "import" command' '

	echo "import refs/cvs/$GITREMOTE/HEAD" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
# Importing CVS revision tick:1.1
blob
mark :16
data 2
1

commit refs/cvs/$GITREMOTE/HEAD
mark :17
data 2
1

from refs/cvs/$GITREMOTE/HEAD^0
M 644 :16 tick

# Importing note for object 17
blob
mark :18
data 23
o_fortuna:1.2
tick:1.1

commit refs/notes/cvs/$GITREMOTE
mark :19
data 46
Annotate commits imported by "git remote-cvs"

from refs/notes/cvs/$GITREMOTE^0
N :18 :17

blob
mark :20
data 41
16 tick:1.1
17 tick:1.1
17 o_fortuna:1.2

blob
mark :21
data 104
commit 17

blob
mark :22
data 18
blob 16
commit 17

commit refs/cvs/$GITREMOTE/_metadata
mark :23
data 42
Updated metadata used by "git remote-cvs"

from refs/cvs/$GITREMOTE/_metadata^0
M 644 :20 CVS/marks
M 644 :21 o_fortuna/1.2
M 644 :22 tick/1.1

EOF
	grep -v -e "^committer " -e "\b[0-9a-f]\{40\}\b" actual > actual.filtered &&
	test_cmp expect actual.filtered

'

test_expect_success '#3: Passing git-remote-cvs output to git-fast-import' '

	git fast-import --quiet \
		--import-marks=".git/info/cvs/$GITREMOTE/marks" \
		--export-marks=".git/info/cvs/$GITREMOTE/marks" \
		< actual &&
	git gc

'

test_expect_success '#3: Verifying correctness of import' '

	echo "verify HEAD" | git remote-cvs "$GITREMOTE"

'

test_expect_success '#4: git-remote-cvs "capabilities" command' '

	echo "capabilities" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
import
marks .git/info/cvs/$GITREMOTE/marks

EOF
	test_cmp expect actual

'

commit=$(git rev-parse "refs/cvs/$GITREMOTE/HEAD")

test_expect_success '#4: git-remote-cvs "list" command' '

	echo "list" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
$commit refs/cvs/$GITREMOTE/HEAD unchanged

EOF
	test_cmp expect actual

'

test_expect_success '#4: git-remote-cvs "import" command' '

	echo "import refs/cvs/$GITREMOTE/HEAD" | git remote-cvs "$GITREMOTE" > actual &&
	cat <<EOF >expect &&
blob
mark :24
data 0

blob
mark :25
data 142

blob
mark :26
data 94

commit refs/cvs/$GITREMOTE/_metadata
mark :27
data 42
Updated metadata used by "git remote-cvs"

from refs/cvs/$GITREMOTE/_metadata^0
M 644 :24 CVS/marks
M 644 :25 o_fortuna/1.2
M 644 :26 tick/1.1

EOF
	grep -v -e "^committer " -e "\b[0-9a-f]\{40\}\b" actual > actual.filtered &&
	test_cmp expect actual.filtered

'

test_expect_success '#4: Passing git-remote-cvs output to git-fast-import' '

	git fast-import --quiet \
		--import-marks=".git/info/cvs/$GITREMOTE/marks" \
		--export-marks=".git/info/cvs/$GITREMOTE/marks" \
		< actual &&
	git gc

'

test_expect_success '#4: Verifying correctness of import' '

	echo "verify HEAD" | git remote-cvs "$GITREMOTE"

'

test_done
