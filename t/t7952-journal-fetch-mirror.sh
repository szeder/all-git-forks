#!/bin/sh

test_description='git journal-fetch --journal-mirror'

. ./test-lib.sh

if test -n "$NO_CURL"; then
	skip_all='skipping test, git built without http support'
	test_done
fi

if test -z "$USE_LIBLMDB"; then
	skip_all='skipping test, git built without lmdb support'
	test_done
fi

. "$TEST_DIRECTORY"/lib-httpd.sh
start_httpd

ORIGIN_REPO="$HTTPD_DOCUMENT_ROOT_PATH/origin.git"
export ORIGIN_REPO

commit_and_write_pack() {
	test_commit "commit$1" file1 "content$1" >/dev/null &&
	echo HEAD^..HEAD | git pack-objects --revs pack >"packsha$1" &&
	packsha=$(head -n1 "packsha$1") &&
	test_path_is_file pack-${packsha}.pack &&
	mkdir -p "$ORIGIN_REPO/objects/pack/" &&
	cp pack-${packsha}.pack pack-${packsha}.idx "$ORIGIN_REPO/objects/pack/" &&
	echo ${packsha}
}

test_expect_success 'setup' '
	test_commit 1st file1 content &&
	HEADSHA=$(git rev-parse HEAD) &&
	git rev-list --objects --all | git pack-objects pack >packsha &&
	git init --bare "$ORIGIN_REPO" &&
	PACKSHA=$(head -n1 packsha) &&
	cp pack-${PACKSHA}.pack pack-${PACKSHA}.idx  "$ORIGIN_REPO/objects/pack/" &&
	export PACKSHA HEADSHA ORIGIN_REPO &&
	base_repo=$(pwd) &&
	(
		cd "$ORIGIN_REPO" &&
		GIT_DIR=. &&
		export GIT_DIR &&
		mkdir -p objects/journals &&
		git journal-append pack "${PACKSHA}" &&
		test_path_is_file objects/journals/0.bin &&
		test_path_is_file objects/journals/metadata.bin &&
		test_path_is_file objects/journals/extents.bin &&
		git journal-append ref refs/heads/dir "$HEADSHA" &&
		git update-ref refs/heads/dir "$HEADSHA"
	)
'

test_expect_success 'client can fetch from empty' '
	git remote add origin "$HTTPD_URL/origin.git" &&
	mkdir -p .git/objects/journals/origin &&
	git journal-fetch --journal-mirror origin &&
	git rev-parse --verify refs/remotes/origin/dir >actual &&
	git rev-parse HEAD >expect &&
	test_cmp actual expect &&
	test_cmp "$ORIGIN_REPO"/objects/journals/0.bin .git/objects/journals/origin/0.bin &&
	test_cmp "$ORIGIN_REPO"/objects/journals/extents.bin .git/objects/journals/origin/extents.bin &&
	test_cmp "$ORIGIN_REPO"/objects/journals/metadata.bin .git/objects/journals/origin/metadata.bin
'

test_expect_success 'client can fetch from non-empty' '
	git journal-fetch --journal-mirror origin &&
	git rev-parse --verify refs/remotes/origin/dir >actual &&
	git rev-parse HEAD >expect &&
	test_cmp actual expect &&
	test_cmp "$ORIGIN_REPO"/objects/journals/0.bin .git/objects/journals/origin/0.bin &&
	test_cmp "$ORIGIN_REPO"/objects/journals/extents.bin .git/objects/journals/origin/extents.bin &&
	test_cmp "$ORIGIN_REPO"/objects/journals/metadata.bin .git/objects/journals/origin/metadata.bin
'

test_expect_success 'once you go journal-mirror you never go back ' '
	test_must_fail git journal-fetch origin 2>stderr &&
	grep "metadata.bin exists" stderr
'

test_expect_success 'set up for upgrade test' '
	git init non-mirror &&
	(
		cd non-mirror &&
		git remote add origin "$HTTPD_URL/origin.git" &&
		mkdir -p .git/objects/journals/origin &&
		git journal-fetch origin
	) &&
	PACKSHA=$(commit_and_write_pack 2) &&
	HEADSHA=$(git rev-parse HEAD) &&
	export PACKSHA HEADSHA &&
	(
		cd "$ORIGIN_REPO" &&
		git journal-append pack $PACKSHA &&
		git journal-append ref refs/heads/two $HEADSHA
	)
'

test_expect_success 'and you cannot upgrade to mirror mode' '
	(
		cd non-mirror &&
		test_must_fail git journal-fetch --journal-mirror origin 2>stderr &&
		grep "without --journal-mirror" stderr
	)
'

stop_httpd
test_done
