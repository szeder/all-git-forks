#!/bin/sh
#
# Copyright (c) 2012 Robin Rosenberg

test_description='git checkout and reset with symlinks as copy'

. ./test-lib.sh

fullfilelist="a
linkd
linkd/reald2
linkd/reald2/zlink2
linkd/reald2/zlink3
linkd/realfile
linkd/zlink
linkfile
reald
reald/reald2
reald/reald2/zlink2
reald/reald2/zlink3
reald/realfile
reald/zlink
text
z"

# setup work with or without real symlink support,
# by default we use real symlink support
test_expect_success setup '
	git commit --allow-empty -m "empty" &&
	git branch empty &&
	echo >a data &&
	git add a &&
	git ln -s a z &&
	mkdir reald &&
	mkdir reald/reald2 &&
	echo >../textf external_file &&
	git ln -s ../textf text &&
	echo >reald/realfile content &&
	git add reald/realfile &&
	git ln -s reald/realfile linkfile &&
	(cd reald/reald2 && git ln -s ../realfile zlink2) &&
	(cd reald/reald2 && git ln -s ../../z zlink3) &&
	(cd reald && git ln -s ../z zlink) &&
	git ln -s reald linkd &&
	git commit -m "repo with symlinks" &&
	test "$(find -L * -print)" = "$fullfilelist" &&
	test "$(git ls-tree -r HEAD)" = "100644 blob 1269488f7fb1f4b56a8c0e5eb48cecbfadfa9219	a
120000 blob afb748c1b973ba508f014b969b193f5370060583	linkd
120000 blob 9a5d4d6a54a108d78fd356d95e15612ace0fc7ed	linkfile
120000 blob bf219293900231f5f9fdb67fa004f2c83bec1635	reald/reald2/zlink2
120000 blob a240f4229e24dde99ca0293d47d62906907e749c	reald/reald2/zlink3
100644 blob d95f3ad14dee633a758d2e331151e950dd13e4ed	reald/realfile
120000 blob 1856dae911a6b4e1a06f19218f97d94d2b1e5a96	reald/zlink
120000 blob ae1910dff1c8f7f779f2154de8f3902702b90e8b	text
120000 blob 2e65efe2a145dda7ee51d1741299f848e5bf752e	z" &&
	git config core.symlinks copy
'

test_expect_success 'reset --hard' '
	rm -rf * &&
	test "$(find * -print)" = "" &&

	git reset --hard &&
	test "$(find * -print )" = "$fullfilelist"
'

test_expect_success 'checkout -f empty' '
	rm -rf * &&
	test "$(find * -print)" = "" &&
	git checkout -f empty &&
	test "$(find * -print)" = ""
'

test_expect_success 'checkout -f master' '
	rm -rf * &&
	test "$(find * -print)" = "" &&

	git checkout -f master &&
	test "$(find * -print )" = "$fullfilelist"
'

test_done
