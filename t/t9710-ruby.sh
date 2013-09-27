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

test_expect_success 'test setup_git_directory()' '
	mkdir t &&
	(
	cd t &&
	git ruby > ../actual <<-EOF
	prefix, nongit_ok = setup_git_directory()
	puts prefix
	EOF
	) &&
	echo "t/" > expected &&
	test_cmp expected actual
'

test_expect_success 'test dwim_ref()' '
	git ruby > actual <<-EOF &&
	sha1, num, ref = dwim_ref("HEAD")
	puts sha1_to_hex(sha1), num, ref
	EOF
	git rev-parse -q --verify HEAD > expected &&
	echo 1 >> expected &&
	git rev-parse -q --verify --symbolic-full-name HEAD >> expected &&
	test_cmp expected actual
'

test_expect_success 'test git_config()' '
	git ruby > actual <<-EOF &&
	git_config() do |key, value|
	  puts "%s=%s" % [key, value]
	end
	EOF
	git config --list > expected &&
	test_cmp expected actual
'

test_done
