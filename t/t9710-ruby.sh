#!/bin/sh
#
# Copyright (c) 2013 Felipe Contreras
#

test_description='test ruby support'

. ./test-lib.sh

if ! test_have_prereq RUBY
then
	skip_all='skipping ruby tests'
	test_done
fi

test_expect_success 'basic support' '
	git ruby > actual <<-EOF &&
	puts "hello world"
	EOF
	echo "hello world" > expected &&
	test_cmp expected actual
'

test_expect_success 'argument passing' '
	cat > script <<-"EOF" &&
	p($0)
	p(ARGV)
	EOF
	git ruby script foo bar > actual &&
	cat > expected <<-EOF &&
	"script"
	["foo", "bar"]
	EOF
	test_cmp expected actual
'

test_expect_success 'test for_each_ref()' '
	test_commit foo &&
	git ruby > actual <<-EOF &&
	for_each_ref() do |name, sha1, flags|
		puts "%s: %s" % [name, sha1_to_hex(sha1)]
	end
	EOF
	git for-each-ref --format="%(refname): %(objectname)" > expected &&
	test_cmp expected actual
'

test_done
