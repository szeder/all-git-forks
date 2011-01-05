#!/usr/bin/perl
use lib (split(/:/, $ENV{GITPERLLIB}));

use warnings;
use strict;
use utf8;

use Test::More;

# test source version
use lib $ENV{GITWEBLIBDIR} || "$ENV{GIT_BUILD_DIR}/gitweb/lib";

# ....................................................................

use_ok('GitwebCache::Capture::ToFile');
note("Using lib '$INC[0]'");
note("Testing '$INC{'GitwebCache/Capture/ToFile.pm'}'");

# Test setting up capture
#
my $capture = new_ok('GitwebCache::Capture::ToFile' => [], 'The $capture');


# Test capturing to file (given by filename) and to filehandle
#
sub capture_block (&;$) {
	$capture->capture(shift, shift || 'actual');

	open my $fh, '<', 'actual' or return;
	local $/ = undef;
	my $result = <$fh>;
	close $fh;
	return $result;
}

diag('Should not print anything except test results and diagnostic');
my $test_data = 'Capture this';
my $captured = capture_block {
	print $test_data;
};
is($captured, $test_data, 'capture simple data: filename');

open my $fh, '>', 'actual';
$captured = capture_block(sub {
	print $test_data;
}, $fh);
close $fh;
is($captured, $test_data, 'capture simple data: filehandle');


# Test capturing :utf8 and :raw data
#
binmode STDOUT, ':utf8';
$test_data = <<'EOF';
Zażółć gęsią jaźń
EOF
utf8::decode($test_data);
$captured = capture_block {
	binmode STDOUT, ':utf8';

	print $test_data;
};
utf8::decode($captured);
is($captured, $test_data, 'capture utf8 data');

$test_data = '|\x{fe}\x{ff}|\x{9F}|\000|'; # invalid utf-8
$captured = capture_block {
	binmode STDOUT, ':raw';

	print $test_data;
};
is($captured, $test_data, 'capture raw data');


# Test nested capturing, useful for future GitwebCache::CacheOutput tests
#
sub read_file {
	my $filename = shift;

	open my $fh, '<', $filename or return;
	local $/ = undef;
	my $result = <$fh>;
	close $fh;

	return $result;
}

my $outer_capture = GitwebCache::Capture::ToFile->new();
$captured = $outer_capture->capture(sub {
	print "pre|";
	my $captured = $capture->capture(sub {
		print "INNER";
	}, 'inner_actual');
	print "|post";
}, 'outer_actual');

my $inner = read_file('inner_actual');
my $outer = read_file('outer_actual');

is($inner, "INNER",     'nested capture: inner');
is($outer, "pre||post", 'nested capture: outer');


# Testing capture when code dies
#
$captured = $outer_capture->capture(sub {
	print "pre|";
	eval {
		my $captured = $capture->capture(sub {
			print "INNER:pre|";
			die "die from inner\n";
			print "INNER:post|"
		}, 'inner_actual');
	};
	print "@=$@" if $@;
	print "|post";
}, 'outer_actual');

my $inner = read_file('inner_actual');
my $outer = read_file('outer_actual');

is($inner, "INNER:pre|",
   'nested capture with die: inner output captured up to die');
is($outer, "pre|@=die from inner\n|post",
   'nested capture with die: outer caught rethrown exception from inner');


done_testing();

# Local Variables:
# coding: utf-8
# End:
