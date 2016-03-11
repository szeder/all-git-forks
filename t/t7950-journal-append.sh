#!/bin/sh

test_description='git journal-append exercise'

. ./test-lib.sh

if test -z "$USE_LIBLMDB"; then
	skip_all='skipping test, git built without lmdb support'
	test_done
fi

commit_and_write_pack() {
	test_commit "commit$1" file1 "content$1" &&
	echo HEAD^..HEAD | git pack-objects --revs pack >"packsha$1" &&
	packsha=$(head -n1 "packsha$1") &&
	test_path_is_file pack-${packsha}.pack &&
	cp pack-${packsha}.pack pack-${packsha}.idx bear.git/objects/pack/ &&
	echo ${packsha}
}

test_expect_success 'setup' '
	test_commit 1st file1 content &&
	git rev-list --objects --all | git pack-objects pack >packsha &&
	packsha=$(head -n1 packsha) &&
	test_path_is_file pack-${packsha}.pack &&
	git init --bare bear.git &&
	cp pack-${packsha}.pack pack-${packsha}.idx bear.git/objects/pack/ &&
	git tag commit1 &&

	commit_and_write_pack 2 &&
	commit_and_write_pack 3 &&
	commit_and_write_pack 4 &&
	commit_and_write_pack 6 &&

	commit_and_write_pack 7 &&
	commit_and_write_pack 8
'

test_expect_success 'journal-append will work when jcdb has not been set up' '
	packsha=$(head -n1 packsha) &&
	packsha2=$(head -n1 packsha2) &&
	headsha=$(git rev-parse commit1) &&
	export packsha headsha &&
	(
		cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		mkdir -p objects/journals &&
		git journal-append pack "${packsha}" &&
		git journal-append ref refs/heads/morx "${headsha}" &&
		test_path_is_file objects/journals/0.bin &&
		test_path_is_file objects/journals/metadata.bin &&
		test "$(wc -c < objects/journals/extents.bin)" -eq 48 &&
		test_path_is_dir connectivity-lmdb &&
		git journal-control --packlog-dump | grep -q "$packsha" &&
		git journal-control --packlog-dump | grep -vq "$packsha2"
	)
'

test_expect_success 'journal-append will work when jcdb has been set up' '
	packsha=$(head -n1 packsha) &&
	packsha2=$(head -n1 packsha2) &&
	packsha3=$(head -n1 packsha3) &&
	headsha=$(git rev-parse commit2) &&
	export packsha packsha2 headsha &&
	(
	  cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		git journal-append pack "${packsha2}" &&
		git journal-append ref refs/heads/morx "${headsha}" &&
		test_path_is_file objects/journals/0.bin &&
		test_path_is_file objects/journals/metadata.bin &&
		test "$(wc -c < objects/journals/extents.bin)" -eq 96 &&
		test_path_is_dir connectivity-lmdb &&
		git journal-control --packlog-dump | grep -q "$packsha" &&
		git journal-control --packlog-dump | grep -q "$packsha2" &&
		git journal-control --packlog-dump | grep -vq "$packsha3"
	)
'

test_expect_success 'journal-append will roll over to new journal' '
	packsha=$(head -n1 packsha) &&
	packsha2=$(head -n1 packsha2) &&
	packsha3=$(head -n1 packsha3) &&
	headsha=$(git rev-parse commit3) &&
	export packsha3 headsha &&
	(cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		# save the old config
		old_config=$(test_config journal.size-limit || echo) &&
		test_when_finished "-n \"$old_config\" && test_config journal.size-limit \"$old_config\"" &&
		test_config journal.size-limit $(wc -c objects/journals/0.bin) &&
		git journal-append pack "${packsha3}" &&
		git journal-append ref refs/heads/morx "${headsha}" &&
		test_path_is_file objects/journals/0.bin &&
		! test -w objects/journals/0.bin &&
		test -r objects/journals/0.bin &&
		test_path_is_file objects/journals/1.bin &&
		test_path_is_file objects/journals/metadata.bin &&
		test "$(wc -c < objects/journals/extents.bin)" -eq 144 &&
		git journal-control --packlog-dump | grep -q "$packsha" &&
		git journal-control --packlog-dump | grep -q "$packsha2" &&
		git journal-control --packlog-dump | grep -q "$packsha3"

	)
'

test_expect_success 'journal-append will refuse to add packs larger than journal.maxpacksize' '
	PACKSHA4=$(head -n1 packsha4) &&
	export PACKSHA4 &&
	(
		cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		test_config journal.maxpacksize 1 &&
		test_must_fail git journal-append pack "${PACKSHA4}"
	)
'

test_expect_success 'journal-append will treat journal.maxpacksize 0 as unset' '
	test_path_is_file packsha4 &&
	PACKSHA4=$(head -n1 packsha4) &&
	export PACKSHA4 &&
	headsha=$(git rev-parse commit4) &&
	(
		cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		test_config journal.maxpacksize 0 &&
		git journal-append pack "${PACKSHA4}" &&
		git journal-append ref refs/heads/four "${headsha}"
	)
'

test_expect_success 'journal-append will treat pack.maxpacksize as the default value for journal.maxpacksize' '
	test_commit 3rd file3 contentxyz &&
	git rev-list --objects --all | grep file3 | git pack-objects pack >packsha3 &&
	PACKSHA3=$(head -n1 packsha3) &&
	test_path_is_file pack-${PACKSHA3}.pack &&
	test_path_is_file pack-${PACKSHA3}.idx &&
	mv pack-${PACKSHA3}.pack pack-${PACKSHA3}.idx bear.git/objects/pack &&
	export PACKSHA3 &&
	(
		cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		test_config journal.maxpacksize 0 &&
		test_config pack.maxpacksize 1 &&
		test_must_fail git journal-append pack "${PACKSHA3}"
	)
'

test_expect_success 'journal-append will treat journal.maxpacksize as the definitive value' '
	test_path_is_file packsha3 &&
	PACKSHA3=$(head -n1 packsha3) &&
	export PACKSHA3 &&
	(
		cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		test_config journal.maxpacksize 500m &&
		test_config pack.maxpacksize 1 &&
		git journal-append pack "${PACKSHA3}"
	)
'

(
	cd bear.git &&
	test_config journal.maxpacksize 0 &&
	test_config pack.maxpacksize 0
)

test_expect_success 'journal-append will refuse to add commits with missing packs' '
	test_commit "commit-missing" file1 "content-missing" &&
	for obj in $(git rev-list --objects commit-missing^..commit-missing | sed "s/ .*//")
	do
		objdir=$(echo "$obj" | sed "s,\(..\).*,objects/\1,") &&
		mkdir -p "bear.git/$objdir" &&
		objpath=$(echo "$obj" | sed "s,\(..\),objects/\1/,") &&
		cp ".git/$objpath" "bear.git/$objpath"
	done &&
	shamissing=$(git rev-parse commit-missing) &&
	export shamissing &&
	(
		cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		test_must_fail git journal-append ref refs/heads/five "$shamissing"
	)
'

test_expect_success 'The journal will not be corrupted if garbage data is written during a crash' '
	PACKSHA6=$(head -n1 packsha6) &&
	export PACKSHA6 &&
	headsha=$(git rev-parse commit6) &&
	export headsha &&
	(
		cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		echo "stark raving gibberish" >>objects/journals/2.bin &&
		echo "stark raving gibberish" >>objects/journals/extents.bin &&
		echo "stark raving gibberish" >>objects/journals/integrity.bin &&
		git journal-append pack "${PACKSHA6}" &&
		git journal-append ref refs/heads/six "${headsha}" &&
		! grep "stark raving gibberish" objects/journals/2.bin &&
		! grep "stark raving gibberish" objects/journals/extents.bin &&
		! grep "stark raving gibberish" objects/journals/integrity.bin
	)
'


test_expect_success 'journal-append will not journal deletion of non-existent ref' '
	(
		cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		test_must_fail git journal-append ref refs/heads/nosuchref $_z40
	)
'

test_expect_success 'journal-append will not journal addition of pack which references missing object' '
	PACKSHA8=$(head -n1 packsha8) &&
	export PACKSHA8 &&
	headsha=$(git rev-parse commit8) &&
	export headsha &&
	(
		cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&
		test_must_fail git journal-append pack "$PACKSHA8"
	)
'

test_expect_success 'journal-append will not journal addition of pack which references dead object' '
	PACKSHA7=$(head -n1 packsha7) &&
	PACKSHA8=$(head -n1 packsha8) &&
	export PACKSHA7 PACKSHA8 &&
	headsha=$(git rev-parse commit7) &&
	export headsha &&
	(
		cd bear.git &&
		GIT_DIR=. &&
		export GIT_DIR &&

		git journal-append pack "$PACKSHA7" &&
		git journal-append ref refs/heads/seven "$headsha" &&
		git update-ref refs/heads/seven "$headsha" &&
		git journal-append ref refs/heads/seven "$_z40" &&
		git update-ref -d refs/heads/seven &&
		test_must_fail git journal-append pack "$PACKSHA8"
	)
'

test_done
