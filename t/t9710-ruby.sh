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

test_expect_success 'test get_sha1()' '
	git ruby > actual <<-EOF &&
	puts sha1_to_hex(get_sha1("HEAD"))
	EOF
	git rev-parse -q --verify HEAD > expected &&
	test_cmp expected actual
'

test_expect_success 'test Object' '
	git ruby > actual <<-EOF &&
	object = Git::Object.get(get_sha1("HEAD"))
	puts object, object.type == OBJ_COMMIT, sha1_to_hex(object.sha1)
	EOF
	git rev-parse -q --verify HEAD > expected &&
	echo "true" >> expected &&
	git rev-parse -q --verify HEAD >> expected &&
	test_cmp expected actual
'

test_expect_success 'test Commit' '
	git ruby > actual <<-EOF &&
	commit = Git::Commit.get(get_sha1("HEAD"))
	puts commit, commit.buffer
	EOF
	git rev-parse -q --verify HEAD > expected &&
	git cat-file commit HEAD >> expected &&
	test_cmp expected actual
'

test_expect_success 'test ParseOpt' '
	cat > parse-script <<"EOF"
	$str = "default"
	$num = 0
	$bool = false

	opts = ParseOpt.new
	opts.usage = "git foo"

	opts.on("b", "bool", help: "Boolean") do |v|
	  $bool = v
	end

	opts.on("s", "string", help: "String") do |v|
	  $str = v
	end

	opts.on("n", "number", help: "Number") do |v|
	  $num = v.to_i
	end

	opts.parse

	p(ARGV)
	p({ :bool => $bool, :str => $str, :num => $num })
	EOF

	git ruby parse-script > actual &&
	cat > expected <<-EOF &&
	[]
	{:bool=>false, :str=>"default", :num=>0}
	EOF
	test_cmp expected actual &&

	git ruby parse-script --bool > actual &&
	cat > expected <<-EOF &&
	[]
	{:bool=>true, :str=>"default", :num=>0}
	EOF
	test_cmp expected actual &&

	git ruby parse-script -b > actual &&
	cat > expected <<-EOF &&
	[]
	{:bool=>true, :str=>"default", :num=>0}
	EOF
	test_cmp expected actual &&

	git ruby parse-script --string=foo > actual &&
	cat > expected <<-EOF &&
	[]
	{:bool=>false, :str=>"foo", :num=>0}
	EOF
	test_cmp expected actual &&

	git ruby parse-script -sfoo > actual &&
	cat > expected <<-EOF &&
	[]
	{:bool=>false, :str=>"foo", :num=>0}
	EOF
	test_cmp expected actual &&

	git ruby parse-script --number=10 > actual &&
	cat > expected <<-EOF &&
	[]
	{:bool=>false, :str=>"default", :num=>10}
	EOF
	test_cmp expected actual &&

	git ruby parse-script --bool --string=bar --number=-20 > actual &&
	cat > expected <<-EOF &&
	[]
	{:bool=>true, :str=>"bar", :num=>-20}
	EOF
	test_cmp expected actual &&

	git ruby parse-script --help > actual &&
	cat > expected <<-EOF &&
	usage: git foo
	    -b, --bool            Boolean
	    -s, --string          String
	    -n, --number          Number
	EOF
	test_cmp expected actual &&

	git ruby parse-script --help > actual &&
	cat > expected <<-EOF &&
	usage: git foo
	    -b, --bool            Boolean
	    -s, --string          String
	    -n, --number          Number
	EOF
	test_cmp expected actual &&

	test_must_fail git ruby parse-script --bad > actual &&
	cat > expected <<-EOF &&
	usage: git foo
	    -b, --bool            Boolean
	    -s, --string          String
	    -n, --number          Number
	EOF
	test_cmp expected actual &&

	git ruby parse-script one --bool two --string=bar three --number=-20 mambo > actual &&
	cat > expected <<-EOF &&
	["one", "two", "three", "mambo"]
	{:bool=>true, :str=>"bar", :num=>-20}
	EOF
	test_cmp expected actual &&

	git ruby parse-script one --bool two -- --three four > actual &&
	cat > expected <<-EOF &&
	["one", "two", "--three", "four"]
	{:bool=>true, :str=>"default", :num=>0}
	EOF
	test_cmp expected actual &&

	git ruby parse-script one --bool --no-bool > actual &&
	cat > expected <<-EOF &&
	["one"]
	{:bool=>false, :str=>"default", :num=>0}
	EOF
	cat actual
	test_cmp expected actual
'

test_done
