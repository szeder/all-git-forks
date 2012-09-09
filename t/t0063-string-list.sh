#!/bin/sh
#
# Copyright (c) 2012 Michael Haggerty
#

test_description='Test string list functionality'

. ./test-lib.sh

string_list_split_in_place() {
	cat >split-expected &&
	test_expect_success "split $1 $2 $3" "
		test-string-list split_in_place '$1' '$2' '$3' >split-actual &&
		test_cmp split-expected split-actual
	"
}

longest_prefix() {
	test "$(test-string-list longest_prefix "$1" "$2")" = "$3"
}

no_longest_prefix() {
	test_must_fail test-string-list longest_prefix "$1" "$2"
}

string_list_split_in_place "foo:bar:baz" ":" "-1" <<EOF
3
[0]: "foo"
[1]: "bar"
[2]: "baz"
EOF

string_list_split_in_place "foo:bar:baz" ":" "0" <<EOF
3
[0]: "foo"
[1]: "bar"
[2]: "baz"
EOF

string_list_split_in_place "foo:bar:baz" ":" "1" <<EOF
2
[0]: "foo"
[1]: "bar:baz"
EOF

string_list_split_in_place "foo:bar:baz" ":" "2" <<EOF
3
[0]: "foo"
[1]: "bar"
[2]: "baz"
EOF

string_list_split_in_place "foo:bar:" ":" "-1" <<EOF
3
[0]: "foo"
[1]: "bar"
[2]: ""
EOF

string_list_split_in_place "" ":" "-1" <<EOF
1
[0]: ""
EOF

string_list_split_in_place ":" ":" "-1" <<EOF
2
[0]: ""
[1]: ""
EOF

test_expect_success "test longest_prefix" '
	no_longest_prefix - '' &&
	no_longest_prefix - x &&
	longest_prefix "" x "" &&
	longest_prefix x x x &&
	longest_prefix "" foo "" &&
	longest_prefix : foo "" &&
	longest_prefix f foo f &&
	longest_prefix foo foobar foo &&
	longest_prefix foo foo foo &&
	no_longest_prefix bar foo &&
	no_longest_prefix bar:bar foo &&
	no_longest_prefix foobar foo &&
	longest_prefix foo:bar foo foo &&
	longest_prefix foo:bar bar bar &&
	longest_prefix foo::bar foo foo &&
	longest_prefix foo:foobar foo foo &&
	longest_prefix foobar:foo foo foo &&
	longest_prefix foo: bar "" &&
	longest_prefix :foo bar ""
'

test_done
