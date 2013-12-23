#!/bin/sh
#
# Copyright (c) 2013 Christian Couder
#

test_description='git interpret-trailers'

. ./test-lib.sh

cat >basic_message <<'EOF'
subject

body
EOF

cat >complex_message_body <<'EOF'
my subject

my body which is long
and contains some special
chars like : = ? !

EOF

# Do not remove trailing spaces below!
cat >complex_message_trailers <<'EOF'
Fixes: 
Acked-by: 
Reviewed-by: 
Signed-off-by: 
EOF

test_expect_success 'without config' '
	printf "ack: Peff\nReviewed-by: \nAcked-by: Johan\n" >expected &&
	git interpret-trailers "ack = Peff" "Reviewed-by" "Acked-by: Johan" >actual &&
	test_cmp expected actual
'

test_expect_success '--trim-empty without config' '
	printf "ack: Peff\nAcked-by: Johan\n" >expected &&
	git interpret-trailers --trim-empty "ack = Peff" "Reviewed-by" "Acked-by: Johan" "sob:" >actual &&
	test_cmp expected actual
'

test_expect_success 'with config setup' '
	git config trailer.ack.key "Acked-by: " &&
	printf "Acked-by: Peff\n" >expected &&
	git interpret-trailers --trim-empty "ack = Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty "Acked-by = Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty "Acked-by :Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'with config setup and = sign' '
	git config trailer.ack.key "Acked-by= " &&
	printf "Acked-by= Peff\n" >expected &&
	git interpret-trailers --trim-empty "ack = Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty "Acked-by= Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty "Acked-by : Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'with config setup and # sign' '
	git config trailer.bug.key "Bug #" &&
	printf "Bug #42\n" >expected &&
	git interpret-trailers --trim-empty "bug = 42" >actual &&
	test_cmp expected actual
'

test_expect_success 'with commit basic message' '
	git interpret-trailers --infile basic_message >actual &&
	test_cmp basic_message actual
'

test_expect_success 'with commit complex message' '
	cat complex_message_body complex_message_trailers >complex_message &&
	cat complex_message_body >expected &&
	printf "Fixes: \nAcked-by= \nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message >actual &&
	test_cmp expected actual
'

test_expect_success 'with commit complex message and args' '
	cat complex_message_body >expected &&
	printf "Fixes: \nAcked-by= \nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \nBug #42\n" >>expected &&
	git interpret-trailers --infile complex_message "ack: Peff" "bug: 42" >actual &&
	test_cmp expected actual
'

test_expect_success 'with commit complex message, args and --trim-empty' '
	cat complex_message_body >expected &&
	printf "Acked-by= Peff\nBug #42\n" >>expected &&
	git interpret-trailers --trim-empty --infile complex_message "ack: Peff" "bug: 42" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "where = before"' '
	git config trailer.bug.where "before" &&
	cat complex_message_body >expected &&
	printf "Bug #42\nFixes: \nAcked-by= \nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "ack: Peff" "bug: 42" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "where = before" for a token in the middle of infile' '
	git config trailer.review.key "Reviewed-by:" &&
	git config trailer.review.where "before" &&
	cat complex_message_body >expected &&
	printf "Bug #42\nFixes: \nAcked-by= \nAcked-by= Peff\nReviewed-by: Johan\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "ack: Peff" "bug: 42" "review: Johan" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "where = before" and --trim-empty' '
	cat complex_message_body >expected &&
	printf "Bug #46\nBug #42\nAcked-by= Peff\nReviewed-by: Johan\n" >>expected &&
	git interpret-trailers --infile complex_message --trim-empty "ack: Peff" "bug: 42" "review: Johan" "Bug: 46"  >actual &&
	test_cmp expected actual
'

test_expect_success 'the default is "ifExist = addIfDifferent"' '
	cat complex_message_body >expected &&
	printf "Bug #42\nFixes: \nAcked-by= \nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "ack: Peff" "review:" "bug: 42" "ack: Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExist = addIfDifferent"' '
	git config trailer.review.ifExist "addIfDifferent" &&
	cat complex_message_body >expected &&
	printf "Bug #42\nFixes: \nAcked-by= \nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "ack: Peff" "review:" "bug: 42" "ack: Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExist = addIfDifferentNeighbor"' '
	git config trailer.ack.ifExist "addIfDifferentNeighbor" &&
	cat complex_message_body >expected &&
	printf "Bug #42\nFixes: \nAcked-by= \nAcked-by= Peff\nAcked-by= Junio\nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "ack: Peff" "review:" "ack: Junio" "bug: 42" "ack: Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExist = addIfDifferentNeighbor" and --trim-empty' '
	git config trailer.ack.ifExist "addIfDifferentNeighbor" &&
	cat complex_message_body >expected &&
	printf "Bug #42\nAcked-by= Peff\nAcked-by= Junio\nAcked-by= Peff\n" >>expected &&
	git interpret-trailers --infile complex_message --trim-empty "ack: Peff" "Acked-by= Peff" "review:" "ack: Junio" "bug: 42" "ack: Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExist = add"' '
	git config trailer.ack.ifExist "add" &&
	cat complex_message_body >expected &&
	printf "Bug #42\nFixes: \nAcked-by= \nAcked-by= Peff\nAcked-by= Peff\nAcked-by= Junio\nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "ack: Peff" "Acked-by= Peff" "review:" "ack: Junio" "bug: 42" "ack: Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExist = overwrite"' '
	git config trailer.fix.key "Fixes:" &&
	git config trailer.fix.ifExist "overwrite" &&
	cat complex_message_body >expected &&
	printf "Bug #42\nFixes: 22\nAcked-by= \nAcked-by= Junio\nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "review:" "fix=53" "ack: Junio" "fix=22" "bug: 42" "ack: Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifExist = doNothing"' '
	git config trailer.fix.ifExist "doNothing" &&
	cat complex_message_body >expected &&
	printf "Bug #42\nFixes: \nAcked-by= \nAcked-by= Junio\nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "review:" "fix=53" "ack: Junio" "fix=22" "bug: 42" "ack: Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'the default is "ifMissing = add"' '
	git config trailer.cc.key "Cc: " &&
	git config trailer.Cc.where "before" &&
	cat complex_message_body >expected &&
	printf "Bug #42\nCc: Linus\nFixes: \nAcked-by= \nAcked-by= Junio\nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "review:" "fix=53" "cc=Linus" "ack: Junio" "fix=22" "bug: 42" "ack: Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifMissing = add"' '
	git config trailer.Cc.ifMissing "add" &&
	cat complex_message_body >expected &&
	printf "Cc: Linus\nBug #42\nFixes: \nAcked-by= \nAcked-by= Junio\nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "review:" "fix=53" "ack: Junio" "fix=22" "bug: 42" "cc=Linus" "ack: Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'using "ifMissing = doNothing"' '
	git config trailer.Cc.ifMissing "doNothing" &&
	cat complex_message_body >expected &&
	printf "Bug #42\nFixes: \nAcked-by= \nAcked-by= Junio\nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "review:" "fix=53" "cc=Linus" "ack: Junio" "fix=22" "bug: 42" "ack: Peff" >actual &&
	test_cmp expected actual
'

test_done
