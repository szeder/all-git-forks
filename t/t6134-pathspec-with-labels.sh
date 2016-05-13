#!/bin/sh

test_description='test labels in pathspecs'
. ./test-lib.sh

test_expect_success 'setup a tree' '
	mkdir sub &&
	for p in fileA fileB fileC fileAB fileAC fileBC fileNoLabel fileUnsetLabel fileSetLabel; do
		: >$p &&
		git add $p &&
		: >sub/$p
		git add sub/$p
	done &&
	git commit -m $p &&
	git ls-files >actual &&
	cat <<EOF >expect &&
fileA
fileAB
fileAC
fileB
fileBC
fileC
fileNoLabel
fileSetLabel
fileUnsetLabel
sub/fileA
sub/fileAB
sub/fileAC
sub/fileB
sub/fileBC
sub/fileC
sub/fileNoLabel
sub/fileSetLabel
sub/fileUnsetLabel
EOF
	test_cmp expect actual
'

test_expect_success 'pathspec with labels and non existent .gitattributes' '
	git ls-files ":(label)" >actual &&
	git ls-files >expect &&
	test_cmp expect actual
'

test_expect_success 'pathspec with labels and non existent .gitattributes' '
	git ls-files ":(label:a)" >actual &&
	test_must_be_empty actual
'

test_expect_success 'setup .gitattributes' '
	cat <<EOF >.gitattributes &&
fileA label=labelA
fileB label=labelB
fileC label=labelC
fileAB label=labelA,labelB
fileAC label=labelA,labelC
fileBC label=labelB,labelC
fileUnsetLabel -label
fileSetLabel label
EOF
	git add .gitattributes &&
	git commit -m "add attributes"
'

test_expect_success 'check for any label' '
	cat <<EOF >expect &&
.gitattributes
fileA
fileAB
fileAC
fileB
fileBC
fileC
fileNoLabel
fileSetLabel
sub/fileA
sub/fileAB
sub/fileAC
sub/fileB
sub/fileBC
sub/fileC
sub/fileNoLabel
sub/fileSetLabel
EOF
	git ls-files ":(label)" >actual &&
	test_cmp expect actual
'

test_expect_success 'check specific label' '
	cat <<EOF >expect &&
fileA
fileAB
fileAC
fileSetLabel
sub/fileA
sub/fileAB
sub/fileAC
sub/fileSetLabel
EOF
	git ls-files ":(label:labelA)" >actual &&
	test_cmp expect actual
'

test_expect_success 'check label with 2 labels' '
	cat <<EOF >expect &&
fileAB
fileSetLabel
sub/fileAB
sub/fileSetLabel
EOF
	git ls-files ":(label:labelA labelB)" >actual &&
	test_cmp expect actual
'

test_expect_success 'check label with more labels' '
	test_must_fail git ls-files ":(label:labelA,label:labelB)" 2>actual &&
	test_i18ngrep "not supported" actual
'

test_expect_success 'check label with more labels but excluded path' '
	cat <<EOF >expect &&
fileAB
fileB
fileBC
fileSetLabel
EOF
	git ls-files ":(label:labelB)" ":(exclude)sub/" >actual &&
	test_cmp expect actual
'

test_expect_success 'check label excluding other labels' '
	cat <<EOF >expect &&
fileAB
fileB
fileBC
fileSetLabel
sub/fileAB
sub/fileB
EOF
	git ls-files ":(label:labelB)" ":(exclude,label:labelC)sub/" >actual &&
	test_cmp expect actual
'


test_expect_success 'check for explicit unlabeled paths' '
	cat <<EOF >expect &&
fileUnsetLabel
sub/fileUnsetLabel
EOF
	git ls-files . ":(exclude,label)" >actual &&
	test_cmp expect actual
'

test_expect_success 'check for paths that have no label' '
	cat <<EOF >expect &&
.gitattributes
fileNoLabel
sub/fileNoLabel
EOF
	git ls-files ":(label)" ":(exclude,label:labelA)" ":(exclude,label:labelB)"  ":(exclude,label:labelC)"  >actual &&
	test_cmp expect actual
'

test_done
