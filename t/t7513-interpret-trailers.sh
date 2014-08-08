#!/bin/sh
#
# Copyright (c) 2013, 2014 Christian Couder
#

test_description='git interpret-trailers'

. ./test-lib.sh

# When we want one trailing space at the end of each line, let's use sed
# to make sure that these spaces are not removed by any automatic tool.

test_expect_success 'setup' '
	cat >basic_message <<-\EOF &&
		subject

		body
	EOF
	cat >complex_message_body <<-\EOF &&
		my subject

		my body which is long
		and contains some special
		chars like : = ? !

	EOF
	sed -e "s/ Z\$/ /" >complex_message_trailers <<-\EOF &&
		Fixes: Z
		Acked-by: Z
		Reviewed-by: Z
		Signed-off-by: Z
	EOF
	cat >basic_patch <<-\EOF
		---
		 foo.txt | 2 +-
		 1 file changed, 1 insertion(+), 1 deletion(-)

		diff --git a/foo.txt b/foo.txt
		index 0353767..1d91aa1 100644
		--- a/foo.txt
		+++ b/foo.txt
		@@ -1,3 +1,3 @@

		-bar
		+baz

		--
		1.9.rc0.11.ga562ddc

	EOF
'

test_expect_success 'without config' '
	sed -e "s/ Z\$/ /" >expected <<-\EOF &&

		ack: Peff
		Reviewed-by: Z
		Acked-by: Johan
	EOF
	git interpret-trailers --trailer "ack = Peff" --trailer "Reviewed-by" \
		--trailer "Acked-by: Johan" >actual &&
	test_cmp expected actual
'

test_expect_success '--trim-empty without config' '
	cat >expected <<-\EOF &&

		ack: Peff
		Acked-by: Johan
	EOF
	git interpret-trailers --trim-empty --trailer "ack = Peff" \
		--trailer "Reviewed-by" --trailer "Acked-by: Johan" \
		--trailer "sob:" >actual &&
	test_cmp expected actual
'

test_expect_success 'with config setup' '
	git config trailer.ack.key "Acked-by: " &&
	cat >expected <<-\EOF &&

		Acked-by: Peff
	EOF
	git interpret-trailers --trim-empty --trailer "ack = Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty --trailer "Acked-by = Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty --trailer "Acked-by :Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'with config setup and = sign' '
	git config trailer.ack.key "Acked-by= " &&
	cat >expected <<-\EOF &&

		Acked-by= Peff
	EOF
	git interpret-trailers --trim-empty --trailer "ack = Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty --trailer "Acked-by= Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty --trailer "Acked-by : Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'with config setup and # sign' '
	git config trailer.bug.key "Bug #" &&
	cat >expected <<-\EOF &&

		Bug #42
	EOF
	git interpret-trailers --trim-empty --trailer "bug = 42" >actual &&
	test_cmp expected actual
'

test_expect_success 'with commit basic message' '
	cat basic_message >expected &&
	echo >>expected &&
	git interpret-trailers <basic_message >actual &&
	test_cmp expected actual
'

test_expect_success 'with basic patch' '
	cat basic_message >input &&
	cat basic_patch >>input &&
	cat basic_message >expected &&
	echo >>expected &&
	cat basic_patch >>expected &&
	git interpret-trailers <input >actual &&
	test_cmp expected actual
'

test_expect_success 'with commit complex message as argument' '
	cat complex_message_body complex_message_trailers >complex_message &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Fixes: Z
		Acked-by= Z
		Reviewed-by: Z
		Signed-off-by: Z
	EOF
	git interpret-trailers complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'with 2 files arguments' '
	cat basic_message >>expected &&
	echo >>expected &&
	cat basic_patch >>expected &&
	git interpret-trailers complex_message input >actual &&
	test_cmp expected actual
'

test_expect_success 'with message that has comments' '
	cat basic_message >>message_with_comments &&
	sed -e "s/ Z\$/ /" >>message_with_comments <<-\EOF &&
		# comment

		# other comment
		Cc: Z
		# yet another comment
		Reviewed-by: Johan
		Reviewed-by: Z
		# last comment

	EOF
	cat basic_patch >>message_with_comments &&
	cat basic_message >expected &&
	cat >>expected <<-\EOF &&
		# comment

		Cc: Peff
		Reviewed-by: Johan
	EOF
	cat basic_patch >>expected &&
	git interpret-trailers --trim-empty --trailer "Cc: Peff" message_with_comments >actual &&
	test_cmp expected actual
'

test_expect_success 'with commit complex message and trailer args' '
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Fixes: Z
		Acked-by= Z
		Acked-by= Peff
		Reviewed-by: Z
		Signed-off-by: Z
		Bug #42
	EOF
	git interpret-trailers --trailer "ack: Peff" \
		--trailer "bug: 42" <complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'with complex patch, args and --trim-empty' '
	cat complex_message >complex_patch &&
	cat basic_patch >>complex_patch &&
	cat complex_message_body >expected &&
	cat >>expected <<-\EOF &&
		Acked-by= Peff
		Bug #42
	EOF
	cat basic_patch >>expected &&
	git interpret-trailers --trim-empty --trailer "ack: Peff" \
		--trailer "bug: 42" <complex_patch >actual &&
	test_cmp expected actual
'

test_expect_success 'using "where = before"' '
	git config trailer.bug.where "before" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Bug #42
		Fixes: Z
		Acked-by= Z
		Acked-by= Peff
		Reviewed-by: Z
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "ack: Peff" \
		--trailer "bug: 42" complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'using "where = before" for a token in the middle of the message' '
	git config trailer.review.key "Reviewed-by:" &&
	git config trailer.review.where "before" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Bug #42
		Fixes: Z
		Acked-by= Z
		Acked-by= Peff
		Reviewed-by:Johan
		Reviewed-by:
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "ack: Peff" --trailer "bug: 42" \
		--trailer "review: Johan" <complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'using "where = before" and --trim-empty' '
	cat complex_message_body >expected &&
	cat >>expected <<-\EOF &&
		Bug #46
		Bug #42
		Acked-by= Peff
		Reviewed-by:Johan
	EOF
	git interpret-trailers --trim-empty --trailer "ack: Peff" \
		--trailer "bug: 42" --trailer "review: Johan" \
		--trailer "Bug: 46" <complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'the default is "ifExists = addIfDifferent"' '
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Bug #42
		Fixes: Z
		Acked-by= Z
		Acked-by= Peff
		Reviewed-by:
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "ack: Peff" --trailer "review:" \
		--trailer "bug: 42" --trailer "ack: Peff" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExists = addIfDifferent"' '
	git config trailer.review.ifExists "addIfDifferent" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Bug #42
		Fixes: Z
		Acked-by= Z
		Acked-by= Peff
		Reviewed-by:
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "ack: Peff" --trailer "review:" \
		--trailer "bug: 42" --trailer "ack: Peff" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExists = addIfDifferentNeighbor"' '
	git config trailer.ack.ifExists "addIfDifferentNeighbor" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Bug #42
		Fixes: Z
		Acked-by= Z
		Acked-by= Peff
		Acked-by= Junio
		Acked-by= Peff
		Reviewed-by:
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "ack: Peff" --trailer "review:" \
		--trailer "ack: Junio" --trailer "bug: 42" \
		--trailer "ack: Peff" <complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExists = addIfDifferentNeighbor" and --trim-empty' '
	git config trailer.ack.ifExists "addIfDifferentNeighbor" &&
	cat complex_message_body >expected &&
	cat >>expected <<-\EOF &&
		Bug #42
		Acked-by= Peff
		Acked-by= Junio
		Acked-by= Peff
	EOF
	git interpret-trailers --trim-empty --trailer "ack: Peff" \
		--trailer "Acked-by= Peff" --trailer "review:" \
		--trailer "ack: Junio" --trailer "bug: 42" \
		--trailer "ack: Peff" <complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExists = add"' '
	git config trailer.ack.ifExists "add" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Bug #42
		Fixes: Z
		Acked-by= Z
		Acked-by= Peff
		Acked-by= Peff
		Acked-by= Junio
		Acked-by= Peff
		Reviewed-by:
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "ack: Peff" \
		--trailer "Acked-by= Peff" --trailer "review:" \
		--trailer "ack: Junio" --trailer "bug: 42" \
		--trailer "ack: Peff" <complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExists = overwrite"' '
	git config trailer.fix.key "Fixes: " &&
	git config trailer.fix.ifExists "overwrite" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Bug #42
		Fixes: 22
		Acked-by= Z
		Acked-by= Junio
		Acked-by= Peff
		Reviewed-by:
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "review:" \
		--trailer "fix=53" --trailer "ack: Junio" --trailer "fix=22" \
		--trailer "bug: 42" --trailer "ack: Peff" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExists = doNothing"' '
	git config trailer.fix.ifExists "doNothing" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Bug #42
		Fixes: Z
		Acked-by= Z
		Acked-by= Junio
		Acked-by= Peff
		Reviewed-by:
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "review:" --trailer "fix=53" \
		--trailer "ack: Junio" --trailer "fix=22" \
		--trailer "bug: 42" --trailer "ack: Peff" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'the default is "ifMissing = add"' '
	git config trailer.cc.key "Cc: " &&
	git config trailer.cc.where "before" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Bug #42
		Cc: Linus
		Fixes: Z
		Acked-by= Z
		Acked-by= Junio
		Acked-by= Peff
		Reviewed-by:
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "review:" --trailer "fix=53" \
		--trailer "cc=Linus" --trailer "ack: Junio" \
		--trailer "fix=22" --trailer "bug: 42" --trailer "ack: Peff" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifMissing = add"' '
	git config trailer.cc.ifMissing "add" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Cc: Linus
		Bug #42
		Fixes: Z
		Acked-by= Z
		Acked-by= Junio
		Acked-by= Peff
		Reviewed-by:
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "review:" --trailer "fix=53" \
		--trailer "ack: Junio" --trailer "fix=22" \
		--trailer "bug: 42" --trailer "cc=Linus" --trailer "ack: Peff" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifMissing = doNothing"' '
	git config trailer.cc.ifMissing "doNothing" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Bug #42
		Fixes: Z
		Acked-by= Z
		Acked-by= Junio
		Acked-by= Peff
		Reviewed-by:
		Signed-off-by: Z
	EOF
	git interpret-trailers --trailer "review:" --trailer "fix=53" \
		--trailer "cc=Linus" --trailer "ack: Junio" \
		--trailer "fix=22" --trailer "bug: 42" --trailer "ack: Peff" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'with simple command' '
	git config trailer.sign.key "Signed-off-by: " &&
	git config trailer.sign.where "after" &&
	git config trailer.sign.ifExists "addIfDifferentNeighbor" &&
	git config trailer.sign.command "echo \"A U Thor <author@example.com>\"" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Fixes: Z
		Acked-by= Z
		Reviewed-by:
		Signed-off-by: Z
		Signed-off-by: A U Thor <author@example.com>
	EOF
	git interpret-trailers --trailer "review:" --trailer "fix=22" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'with command using commiter information' '
	git config trailer.sign.ifExists "addIfDifferent" &&
	git config trailer.sign.command "echo \"\$GIT_COMMITTER_NAME <\$GIT_COMMITTER_EMAIL>\"" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Fixes: Z
		Acked-by= Z
		Reviewed-by:
		Signed-off-by: Z
		Signed-off-by: C O Mitter <committer@example.com>
	EOF
	git interpret-trailers --trailer "review:" --trailer "fix=22" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'with command using author information' '
	git config trailer.sign.key "Signed-off-by: " &&
	git config trailer.sign.where "after" &&
	git config trailer.sign.ifExists "addIfDifferentNeighbor" &&
	git config trailer.sign.command "echo \"\$GIT_AUTHOR_NAME <\$GIT_AUTHOR_EMAIL>\"" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-\EOF &&
		Fixes: Z
		Acked-by= Z
		Reviewed-by:
		Signed-off-by: Z
		Signed-off-by: A U Thor <author@example.com>
	EOF
	git interpret-trailers --trailer "review:" --trailer "fix=22" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'setup a commit' '
	echo "Content of the first commit." > a.txt &&
	git add a.txt &&
	git commit -m "Add file a.txt"
'

test_expect_success 'with command using $ARG' '
	git config trailer.fix.ifExists "overwrite" &&
	git config trailer.fix.command "git log -1 --oneline --format=\"%h (%s)\" --abbrev-commit --abbrev=14 \$ARG" &&
	FIXED=$(git log -1 --oneline --format="%h (%s)" --abbrev-commit --abbrev=14 HEAD) &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-EOF &&
		Fixes: $FIXED
		Acked-by= Z
		Reviewed-by:
		Signed-off-by: Z
		Signed-off-by: A U Thor <author@example.com>
	EOF
	git interpret-trailers --trailer "review:" --trailer "fix=HEAD" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'with failing command using $ARG' '
	git config trailer.fix.ifExists "overwrite" &&
	git config trailer.fix.command "false \$ARG" &&
	cat complex_message_body >expected &&
	sed -e "s/ Z\$/ /" >>expected <<-EOF &&
		Fixes: Z
		Acked-by= Z
		Reviewed-by:
		Signed-off-by: Z
		Signed-off-by: A U Thor <author@example.com>
	EOF
	git interpret-trailers --trailer "review:" --trailer "fix=HEAD" \
		<complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'with empty tokens' '
	cat >expected <<-EOF &&

		Signed-off-by: A U Thor <author@example.com>
	EOF
	git interpret-trailers --trailer ":" --trailer ":test" >actual <<-EOF &&
	EOF
	test_cmp expected actual
'

test_expect_success 'with command but no key' '
	git config --unset trailer.sign.key &&
	cat >expected <<-EOF &&

		sign: A U Thor <author@example.com>
	EOF
	git interpret-trailers >actual <<-EOF &&
	EOF
	test_cmp expected actual
'

test_expect_success 'with no command and no key' '
	git config --unset trailer.review.key &&
	cat >expected <<-EOF &&

		review: Junio
		sign: A U Thor <author@example.com>
	EOF
	git interpret-trailers --trailer "review:Junio" >actual <<-EOF &&
	EOF
	test_cmp expected actual
'

test_done
