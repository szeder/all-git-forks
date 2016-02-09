#!/bin/sh

test_description='git journal-fetch'

. ./test-lib.sh

if test -z "$USE_LIBLMDB"; then
	skip_all='skipping test, git built without lmdb support'
	test_done
fi

. "$TEST_DIRECTORY"/lib-httpd.sh
start_httpd

cat >hexdump <<'EOF'
#!/bin/sh
"$PERL_PATH" -e '$/ = undef; $_ = <>; s/./sprintf "%02x", ord($&)/ge; print $_' < "$1"
EOF
chmod +x hexdump

commit_and_write_pack() {
	target=$2 &&
	test_commit "commit$1" file1 "content$1" &&
	echo HEAD^..HEAD | git pack-objects --revs pack >"packsha$1" &&
	packsha=$(head -n1 "packsha$1") &&
	test_path_is_file pack-${packsha}.pack &&
	cp pack-${packsha}.pack pack-${packsha}.idx "$target/objects/pack/" &&
	echo ${packsha}
}

test_expect_success 'setup' '
	test_commit 1st file1 content &&
	HEADSHA=$(git rev-parse HEAD) &&
	git rev-list --objects --all | git pack-objects pack >packsha &&
	packsha=$(head -n1 packsha) &&
	test_path_is_file pack-${packsha}.pack &&
	ORIGIN_REPO="$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
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
		git journal-control --packlog-dump | grep -q "$PACKSHA" &&
		git journal-append ref refs/heads/dir "$HEADSHA" &&
		git update-ref refs/heads/dir "$HEADSHA"
	)
'

test_expect_success 'client can fetch from empty' '
	git remote add origin "$HTTPD_URL/origin.git" &&
	mkdir -p .git/objects/journals/origin &&
	git fetch origin &&
	git rev-parse --verify refs/remotes/origin/dir >actual &&
	git rev-parse HEAD >expect &&
	test_cmp actual expect &&
	# Here, 0.bin will have been deleted because we already handled
	# all of its content
	test_path_is_missing .git/objects/journals/origin/0.bin &&
	test_cmp httpd/www/origin.git/objects/journals/extents.bin .git/objects/journals/origin/extents.bin &&
	./hexdump .git/objects/journals/origin/state.bin >state &&
	printf 00000030 >expect &&
	test_cmp state expect
'

Z=$_z40

test_expect_success 'set up for d/f ref change test' '
	HEADSHA=$(git rev-parse HEAD) &&
	export HEADSHA &&
	(
		cd "$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
		GIT_DIR=. &&
		export GIT_DIR &&
		git journal-append ref refs/heads/dir "$Z" &&
		git update-ref -d refs/heads/dir &&
		git journal-append ref refs/heads/dir/a "$HEADSHA" &&
		git update-ref refs/heads/dir/a "$HEADSHA"
	)
'

test_expect_success 'client can fetch across a d/f ref change' '
	git fetch origin &&
	git rev-parse --verify refs/remotes/origin/dir/a >actual &&
	git rev-parse HEAD >expect &&
	test_cmp actual expect
'

test_expect_success 'set up for f/d ref change test' '
	HEADSHA=$(git rev-parse HEAD) &&
	export HEADSHA &&
	(
		cd "$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
		GIT_DIR=. &&
		export GIT_DIR &&
		git journal-append ref refs/heads/dir/a "$Z" &&
		git update-ref -d refs/heads/dir/a &&
		git journal-append ref refs/heads/dir "$HEADSHA" &&
		git update-ref refs/heads/dir "$HEADSHA"
	)
'

test_expect_success 'client can fetch across a f/d ref change' '
	git fetch origin &&
	git rev-parse --verify refs/remotes/origin/dir >actual &&
	git rev-parse HEAD >expect &&
	test_cmp actual expect
'

save_journal() {
	test -n "$GIT_DIR" || GIT_DIR=".git"
	mkdir -p $GIT_DIR/objects/backup &&
	find $GIT_DIR/objects/journals/ -name \*.bin -o -name \*.bin.size >$GIT_DIR/objects/backup/files &&
	for file in $(cat $GIT_DIR/objects/backup/files)
	do
		backup=$(echo "$file" | sed "s,$GIT_DIR/objects/journals/,$GIT_DIR/objects/backup/,") &&
		mkdir -p $(dirname "$backup") &&
		cp "$file" "$backup"
	done
}

restore_journal() {
	test -n "$GIT_DIR" || GIT_DIR=".git" &&
	find $GIT_DIR/objects/journals/ -name \*.bin -o -name \*.bin.size |xargs rm &&
	for file in $(cat $GIT_DIR/objects/backup/files)
	do
		backup=$(echo "$file" | sed "s,$GIT_DIR/objects/journals/,$GIT_DIR/objects/backup/,") &&
		mkdir -p $(dirname "$file") &&
		cp "$backup" "$file"
	done
}

test_expect_success 'set up for fetch d/f conflict test' '
	HEADSHA=$(git rev-parse HEAD) &&
	export HEADSHA &&
	save_journal &&
	(
		cd "$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
		GIT_DIR=. &&
		export GIT_DIR &&
		cat objects/journals/0.bin | wc -c >"$base_repo/size" &&
		save_journal &&
		# because refs/heads/dir still exists, this is bogus
		git journal-append ref refs/heads/dir/a "$HEADSHA"
	)
'

# An update-ref fail only happens if the journal gets screwed up or
# someone manually updates a remote ref
test_expect_success 'fetch fails on update-ref fail' '
	test_must_fail git fetch origin -vvv 2>stderr &&
	grep "Failed to commit ref transaction" stderr &&
	egrep "cannot lock ref .refs/remotes/origin/dir/a" .git/journal-fetch-ref-update.log &&
	git for-each-ref refs/to-be-deleted/origin >undeleted-backups &&
	test_line_count = 0 undeleted-backups
'

test_expect_success 'set up for multiple journal fetch test' '
	restore_journal &&
	ORIGIN_REPO="$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
	export ORIGIN_REPO &&
	(
		cd "$ORIGIN_REPO" &&
		GIT_DIR=. &&
		export GIT_DIR &&
		restore_journal
	) &&
	commit_and_write_pack 2 "$ORIGIN_REPO" &&
	PACKSHA=$(head -n1 packsha2) &&
	COMMIT_SHA=$(git rev-parse commit2) &&
	export PACKSHA ORIGIN_REPO COMMIT_SHA &&
	(
		cd "$ORIGIN_REPO" &&
		GIT_DIR=. &&
		export GIT_DIR &&
		old_config=$(test_config journal.size-limit || echo) &&
		test_when_finished "-n \"$old_config\" && test_config journal.size-limit \"$old_config\"" &&
		test_config journal.size-limit $(wc -c objects/journals/0.bin) &&
		git journal-append pack "${PACKSHA}" &&
		git journal-append ref refs/heads/two "${COMMIT_SHA}" &&
		test_path_is_file objects/journals/1.bin
	)
'

(cd "$HTTPD_DOCUMENT_ROOT_PATH/origin.git" && test_config journal.size-limit 3g)

test_expect_success 'client can fetch across a journal roll' '
	PACKSHA=$(head -n1 packsha2) &&
	git fetch origin &&
	git rev-parse --verify refs/remotes/origin/two >actual &&
	git rev-parse commit2 >expect &&
	test_cmp actual expect
'

test_expect_success 'set up for no-extract test' '
	ORIGIN_REPO="$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
	commit_and_write_pack 3 "$ORIGIN_REPO" &&
	PACKSHA=$(head -n1 packsha3) &&
	COMMIT_SHA=$(git rev-parse commit3) &&
	export PACKSHA ORIGIN_REPO COMMIT_SHA &&
	(
		cd "$ORIGIN_REPO" &&
		GIT_DIR=. && export GIT_DIR &&
		git journal-append pack "${PACKSHA}" &&
		git journal-append ref refs/heads/three "${COMMIT_SHA}" &&
		test_path_is_file objects/journals/1.bin &&
		test_path_is_missing objects/journals/2.bin
	)
'

test_expect_success 'no-extract leaves journals in-place' '
	git journal-fetch --no-extract origin &&
	we_got=$(wc -c <.git/objects/journals/origin/1.bin) &&
	tail -c "$we_got" "$HTTPD_DOCUMENT_ROOT_PATH/origin.git/objects/journals/1.bin" >expect &&
	test_cmp .git/objects/journals/origin/1.bin expect
'

test_expect_success 'set up for existing chunk test' '
	ORIGIN_REPO="$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
	commit_and_write_pack 4 "$ORIGIN_REPO" &&
	PACKSHA=$(head -n1 packsha4) &&
	COMMIT_SHA=$(git rev-parse commit4) &&
	export PACKSHA ORIGIN_REPO COMMIT_SHA &&
	(
		cd "$ORIGIN_REPO" &&
		GIT_DIR=. && export GIT_DIR &&
		git journal-append pack "${PACKSHA}" &&
		git journal-append ref refs/heads/four "${COMMIT_SHA}" &&
		test_path_is_file objects/journals/1.bin
	)
'

test_expect_success 'journal-fetch works when there are already journal files' '
	git journal-fetch origin &&
	test_path_is_missing .git/objects/journals/origin/1.bin &&
	git rev-parse --verify refs/remotes/origin/four >actual &&
	git rev-parse commit4 >expect &&
	test_cmp actual expect
'

test_expect_success 'size backfill works' '
	rm .git/objects/journals/origin/extents.bin.size &&
	git journal-fetch origin &&
	test_path_is_file .git/objects/journals/origin/extents.bin.size
'

test_expect_success 'fetch does not advance state if no ref updates' '
	ORIGIN_REPO="$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
	commit_and_write_pack 5 "$ORIGIN_REPO" &&
	PACKSHA=$(head -n1 packsha5) &&
	COMMIT_SHA=$(git rev-parse commit5) &&
	export PACKSHA ORIGIN_REPO COMMIT_SHA &&
	(
		cd "$ORIGIN_REPO" &&
		GIT_DIR=. && export GIT_DIR &&
		git journal-append pack "${PACKSHA}"
	) &&
	cp .git/objects/journals/origin/state.bin saved-state &&
	git fetch origin &&
	test_cmp saved-state .git/objects/journals/origin/state.bin
'

test_expect_success 'fetch advances state after ref update' '
	ORIGIN_REPO="$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
	COMMIT_SHA=$(git rev-parse commit5) &&
	export ORIGIN_REPO COMMIT_SHA &&
	(
		cd "$ORIGIN_REPO" &&
		GIT_DIR=. && export GIT_DIR &&
		git journal-append ref refs/heads/five "${COMMIT_SHA}"
	) &&
	cp .git/objects/journals/origin/state.bin saved-state &&
	git fetch origin &&
	! test_cmp saved-state .git/objects/journals/origin/state.bin &&
	git rev-parse refs/remotes/origin/five
'

test_expect_success 'set up for crash-recovery test' '
	ORIGIN_REPO="$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
	commit_and_write_pack 7 "$ORIGIN_REPO" &&
	PACKSHA=$(head -n1 packsha7) &&
	COMMIT_SHA=$(git rev-parse commit7) &&
	export PACKSHA ORIGIN_REPO COMMIT_SHA &&
	(
		cd "$ORIGIN_REPO" &&
		GIT_DIR=. && export GIT_DIR &&
		git journal-append pack "${PACKSHA}" &&
		git journal-append ref refs/heads/seven "${COMMIT_SHA}"
	) &&
	# we will now pretend that we have tried to download more of
	# 1.bin, but crashed, leaving the file full of gibberish.
	# But since the size file has not been updated, the gibberish
	# should be ignored
	test_path_is_missing .git/objects/journals/origin/1.bin &&
	test_path_is_missing .git/objects/journals/origin/1.bin.size &&
	echo "stark raving gibberish" >>.git/objects/journals/origin/1.bin
'

test_expect_success 'journal-fetch works when there are existing corrupt journal files' '
	git journal-fetch origin &&
	test_path_is_missing .git/objects/journals/origin/1.bin &&
	git rev-parse --verify refs/remotes/origin/seven >actual &&
	git rev-parse commit7 >expect &&
	test_cmp actual expect
'

test_expect_success 'journal-fetch works when extents points past the end of the journal' '
	cp "$ORIGIN_REPO/objects/journals/1.bin" old_bin &&
	ORIGIN_REPO="$HTTPD_DOCUMENT_ROOT_PATH/origin.git" &&
	commit_and_write_pack 6 "$ORIGIN_REPO" &&
	PACKSHA=$(head -n1 packsha6) &&
	COMMIT_SHA=$(git rev-parse commit6) &&
	export PACKSHA ORIGIN_REPO COMMIT_SHA &&
	(
		cd "$ORIGIN_REPO" &&
		GIT_DIR=. && export GIT_DIR &&
		git journal-append pack "${PACKSHA}" &&
		git journal-append ref refs/heads/six "${COMMIT_SHA}"
	) &&
	cp "$ORIGIN_REPO/objects/journals/1.bin" new_bin &&
	# after this test, make the journal on origin catch up
	test_when_finished "cp new_bin \"$ORIGIN_REPO/objects/journals/1.bin\"" &&
	cp old_bin "$ORIGIN_REPO/objects/journals/1.bin" &&

	git journal-fetch origin &&
	test_must_fail git rev-parse --verify refs/remotes/origin/six >actual
'

test_expect_success 'journal-fetch fails when size file present but base
file missing' '
	mv .git/objects/journals/origin/extents.bin saved-extents &&
	test_must_fail git journal-fetch origin 2>stderr &&
	grep "size file exists" stderr &&
	mv saved-extents .git/objects/journals/origin/extents.bin
'

test_expect_success 'journal-fetch recovers when journal catches up' '
	git journal-fetch origin &&
	git rev-parse --verify refs/remotes/origin/six >actual &&
	git rev-parse commit6 >expect &&
	test_cmp actual expect
'

stop_httpd

# This test is intentionally after stop_httpd
test_expect_success 'journal-fetch fails on http failure' '
	test_must_fail git journal-fetch origin
'

test_done
