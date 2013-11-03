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
	git config trailer.ack.value "Acked-by: " &&
	printf "Acked-by: Peff\n" >expected &&
	git interpret-trailers --trim-empty "ack = Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty "Acked-by = Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty "Acked-by :Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'with config setup and = sign' '
	git config trailer.ack.value "Acked-by= " &&
	printf "Acked-by= Peff\n" >expected &&
	git interpret-trailers --trim-empty "ack = Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty "Acked-by= Peff" >actual &&
	test_cmp expected actual &&
	git interpret-trailers --trim-empty "Acked-by : Peff" >actual &&
	test_cmp expected actual
'

test_expect_success 'with config setup and # sign' '
	git config trailer.bug.value "Bug #" &&
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
	git interpret-trailers --infile complex_message >actual &&
	test_cmp complex_message actual
'

test_expect_success 'with commit complex message and args' '
	cat complex_message_body >expected &&
	printf "Bug #42\nFixes: \nAcked-by= Peff\nReviewed-by: \nSigned-off-by: \n" >>expected &&
	git interpret-trailers --infile complex_message "ack: Peff" "bug: 42" >actual &&
	test_cmp expected actual
'

test_expect_success 'with commit complex message, args and --trim-empty' '
	cat complex_message_body >expected &&
	printf "Bug #42\nAcked-by= Peff\n" >>expected &&
	git interpret-trailers --trim-empty --infile complex_message "ack: Peff" "bug: 42" >actual &&
	test_cmp expected actual
'

test_done
