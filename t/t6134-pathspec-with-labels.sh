#!/bin/sh

test_description='test labels in pathspecs'
. ./test-lib.sh

test_expect_success 'setup a tree' '
	for p in file sub/file sub/sub/file sub/file2 sub/sub/sub/file sub2/file; do
		if echo $p | grep /; then
			mkdir -p $(dirname $p)
		fi &&
		: >$p &&
		git add $p &&
		git commit -m $p
	done &&
	git log --oneline --format=%s >actual &&
	cat <<EOF >expect &&
sub2/file
sub/sub/sub/file
sub/file2
sub/sub/file
sub/file
file
EOF
	test_cmp expect actual
'

test_expect_success 'pathspec with labels and no .gitattributes exists' '
	git ls-files ":(label:a)" >actual &&
	test_must_be_empty actual
'

test_expect_success 'setup .gitattributes' '
	cat <<-EOF >.gitattributes &&
	/file label=b
	sub/file label=a
	sub/sub/* label=b,c
	EOF
	git add .gitattributes &&
	git commit -m "add attributes"
'

test_expect_success 'check label' '
	cat <<-EOF >expect &&
	sub/file
	EOF
	git ls-files ":(label:a)" >actual &&
	test_cmp expect actual
'

test_expect_success 'check label from label list' '
	cat <<-EOF >expect &&
	sub/sub/file
	EOF
	git ls-files ":(label:c)" >actual &&
	test_cmp expect actual
'

test_expect_success 'check label with more labels' '
	cat <<-EOF >expect &&
	file
	sub/sub/file
	EOF
	git ls-files ":(label:b)" >actual &&
	test_cmp expect actual
'

test_expect_success 'check label with more labels but excluded path' '
	cat <<-EOF >expect &&
	sub/sub/file
	EOF
	git ls-files ":(label:b)" ":(exclude)./file" >actual &&
	test_cmp expect actual
'

test_expect_success 'check label specifying more labels' '
	cat <<-EOF >expect &&
	sub/sub/file
	EOF
	git ls-files ":(label:b c)" >actual &&
	test_cmp expect actual
'

test_expect_success 'check label specifying more labels' '
	cat <<-EOF >expect &&
	sub/file
	sub/sub/file
	EOF
	git ls-files ":(label:b c)" ":(label:a)" >actual &&
	test_cmp expect actual
'
test_done
