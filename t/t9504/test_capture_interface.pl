#!/usr/bin/perl
use lib (split(/:/, $ENV{GITPERLLIB}));

use warnings;
use strict;
use utf8;

use Test::More;
use File::Compare;

# test source version
use lib $ENV{GITWEBLIBDIR} || "$ENV{GIT_BUILD_DIR}/gitweb/lib";

# ....................................................................

use_ok('GitwebCache::Capture::Simple');
diag("Using lib '$INC[0]'");
diag("Testing '$INC{'GitwebCache/Capture/Simple.pm'}'");

# Test setting up capture
#
my $capture = new_ok('GitwebCache::Capture::Simple' => [], 'The $capture');

# Test capturing
#
sub capture_block (&) {
	return $capture->capture(shift);
}

diag('Should not print anything except test results and diagnostic');
my $test_data = 'Capture this';
my $captured = capture_block {
	print $test_data;
};
is($captured, $test_data, 'capture simple data');

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

# Test nested capturing
#
TODO: {
	local $TODO = "not required for capturing gitweb output";
	no warnings;

	my $outer_capture = GitwebCache::Capture::Simple->new();
	$captured = $outer_capture->capture(sub {
		print "pre|";
		my $captured = $capture->capture(sub {
			print "INNER";
		});
		print lc($captured);
		print "|post";
	});
	is($captured, "pre|inner|post", 'nested capture');
}

SKIP: {
	skip "Capture::Tiny not available", 1
		unless eval { require Capture::Tiny; };

	$captured = Capture::Tiny::capture(sub {
		my $inner = $capture->capture(sub {
			print "INNER";
		});
	});
	is($captured, '', "doesn't print while capturing");
}

# Test capturing to file
#
my $test_data = 'Capture this';
open my $fh, '>', 'expected' or die "Couldn't open file for writing: $!";
print {$fh} $test_data;
close $fh;

$capture->capture(sub { print $test_data; }, 'actual');
cmp_ok(compare('expected', 'actual'), '==', 0, 'capturing to file via filename');

open my $fh, '>', 'actual' or die "Couldn't open file for writing: $!";
$capture->capture(sub { print $test_data; }, $fh);
close $fh;
cmp_ok(compare('expected', 'actual'), '==', 0, 'capturing to file via filehandle');


done_testing();

# Local Variables:
# coding: utf-8
# End:
