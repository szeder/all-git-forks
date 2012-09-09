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

test_done
