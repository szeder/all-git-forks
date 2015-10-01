#!/bin/sh

test_description='closing packs'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-terminal.sh

test_expect_success setup '
	for i in $(test_seq 100)
	do
	    test_commit commit$i || return
	done &&
	git repack
'

current_dir=$(pwd)
pack_path=$(find "$current_dir/.git/objects/pack" -name "pack-*.pack")

test_expect_success 'pack exists' 'test_path_is_file "$pack_path"'

test_expect_success TTY 'pack is closed after all output passed to pager' '
	test_terminal env pack_path="$pack_path" \
		PAGER="/bin/sh -c '\''cat >/dev/null && lsof -- \"\$pack_path\" >pack_opened'\''" \
		git -c core.packedGitWindowSize=100 log &&
	test_must_be_empty pack_opened
'

test_expect_success false false

test_done
