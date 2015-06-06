#!/bin/sh

test_description='closing packs'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-terminal.sh

test_expect_success setup '
	test_commit commit1 &&
	git repack
'

current_dir=$(pwd)
pack_path=$(find "$current_dir/.git/objects/pack" -name "pack-*.pack")

test_expect_success 'pack exists' 'test_path_is_file "$pack_path"'

test_expect_success TTY 'pack removed by pager' '
	test_terminal env pack_path="$pack_path" \
		PAGER="/bin/sh -c '\''cat >/dev/null && rm -- \"\$pack_path\"'\''" \
		git log &&
	test_path_is_missing "$pack_path"
'

test_done
