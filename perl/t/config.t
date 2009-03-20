#!/usr/bin/env perl -- -w

use t::TestUtils;
use Test::More no_plan;
use strict;

use Error qw(:try);

use_ok("Git::Config");

in_empty_repo sub {
	my $git = Git->repository;
	$git->command_oneline("config", "foo.bar", "baz");
	$git->command_oneline("config", "list.value", "one");
	$git->command_oneline("config", "--add", "list.value", "two");
	$git->command_oneline("config", "foo.intval", "12g");
	$git->command_oneline("config", "foo.falseval", "false");
	$git->command_oneline("config", "foo.trueval", "on");

	my $conf = Git::Config->new();
	ok($conf, "constructed a new Git::Config");
	isa_ok($conf, "Git::Config", "Git::Config->new()");

	is($conf->config("foo.bar"), "baz", "read single line");
	$conf->config("foo.bar", "frop");
	like($git->command_oneline("config", "foo.bar"), qr/frop/,
		"->config() has immediate effect");
	$conf->autoflush(0);
	is_deeply(
		[$conf->config("list.value")],
		[qw(one two)],
		"read multi-value item",
	);

	my $error;
	try {
		my $val = $conf->config("list.value");
	}
	catch {
		$@ =~ m{multiple}i && ($error = $@);
	};
	ok($error, "produced an error reading a list into a scalar");

	undef($error);
	try {
		$conf->config("list.value" => "single");
	}
	catch {
		$@ =~ m{multiple}i && ($error = $@);
	}
	ok($error, "produced an error replacing a list with a scalar");

	ok(eval { $conf->config("foo.bar", [ "baz", "frop"]); 1 },
		"no error replacing a scalar with a list");

	like($git->command_oneline("config", "foo.bar"), qr/frop/,
		"->config() no immediate effect with autoflush = 0");

	$conf->flush;

	like($git->command_oneline("config", "--get-all", "foo.bar"),
		qr/baz\s*frop/,
		"->flush()");

	SKIP:{
		if ($git->command_oneline( "config", "--get-all",
		    "--global", "foo.bar" )) {
			skip "someone set foo.bar in global config", 1;
		}
		my @foo_bar = $conf->global("foo.bar");
		pass(!@foo_bar, "->global() reading only");
	}
};


