#!/usr/bin/perl

use strict;
use warnings;
use Git;

sub get_times {
	my $name = shift;
	open my $fh, "<", $name or return undef;
	my $line = <$fh>;
	return undef if not defined $line;
	close $fh or die "cannot close $name: $!";
	# times
	if ($line =~ /^(?:(\d+):)?(\d+):(\d+(?:\.\d+)?) (\d+(?:\.\d+)?) (\d+(?:\.\d+)?)$/) {
		my $rt = ((defined $1 ? $1 : 0.0)*60+$2)*60+$3;
		return ($rt, $4, $5);
	# size
	} elsif ($line =~ /^\d+$/) {
		return $&;
	} else {
		die "bad input line: $line";
	}
}

sub relative_change {
	my ($r, $firstr) = @_;
	if ($firstr > 0) {
		return sprintf "%+.1f%%", 100.0*($r-$firstr)/$firstr;
	} elsif ($r == 0) {
		return "=";
	} else {
		return "+inf";
	}
}

sub format_times {
	my ($r, $u, $s, $firstr) = @_;
	# no value means we did not finish the test
	if (!defined $r) {
		return "<missing>";
	}
	# a single value means we have a size, not times
	if (!defined $u) {
		return format_size($r, $firstr);
	}
	# otherwise, we have real/user/system times
	my $out = sprintf "%.2f(%.2f+%.2f)", $r, $u, $s;
	$out .= ' ' . relative_change($r, $firstr) if defined $firstr;
	return $out;
}

sub human_size {
	my $n = shift;
	my @units = ('', qw(K M G));
	while ($n > 900 && @units > 1) {
		$n /= 1000;
		shift @units;
	}
	return $n unless length $units[0];
	return sprintf '%.1f%s', $n, $units[0];
}

sub format_size {
	my ($size, $first) = @_;
	# match the width of a time: 0.00(0.00+0.00)
	my $out = sprintf '%15s', human_size($size);
	$out .= ' ' . relative_change($size, $first) if defined $first;
	return $out;
}

my (@dirs, %dirnames, %dirabbrevs, %prefixes, @tests);
while (scalar @ARGV) {
	my $arg = $ARGV[0];
	my $dir;
	last if -f $arg or $arg eq "--";
	if (! -d $arg) {
		my $rev = Git::command_oneline(qw(rev-parse --verify), $arg);
		$dir = "build/".$rev;
	} else {
		$arg =~ s{/*$}{};
		$dir = $arg;
		$dirabbrevs{$dir} = $dir;
	}
	push @dirs, $dir;
	$dirnames{$dir} = $arg;
	my $prefix = $dir;
	$prefix =~ tr/^a-zA-Z0-9/_/c;
	$prefixes{$dir} = $prefix . '.';
	shift @ARGV;
}

if (not @dirs) {
	@dirs = ('.');
}
$dirnames{'.'} = $dirabbrevs{'.'} = "this tree";
$prefixes{'.'} = '';

shift @ARGV if scalar @ARGV and $ARGV[0] eq "--";

@tests = @ARGV;
if (not @tests) {
	@tests = glob "p????-*.sh";
}

my @subtests;
my %shorttests;
for my $t (@tests) {
	$t =~ s{(?:.*/)?(p(\d+)-[^/]+)\.sh$}{$1} or die "bad test name: $t";
	my $n = $2;
	my $fname = "test-results/$t.subtests";
	open my $fp, "<", $fname or die "cannot open $fname: $!";
	for (<$fp>) {
		chomp;
		/^(\d+)$/ or die "malformed subtest line: $_";
		push @subtests, "$t.$1";
		$shorttests{"$t.$1"} = "$n.$1";
	}
	close $fp or die "cannot close $fname: $!";
}

sub read_descr {
	my $name = shift;
	open my $fh, "<", $name or return "<error reading description>";
	my $line = <$fh>;
	close $fh or die "cannot close $name";
	chomp $line;
	return $line;
}

my %descrs;
my $descrlen = 4; # "Test"
for my $t (@subtests) {
	$descrs{$t} = $shorttests{$t}.": ".read_descr("test-results/$t.descr");
	$descrlen = length $descrs{$t} if length $descrs{$t}>$descrlen;
}

sub have_duplicate {
	my %seen;
	for (@_) {
		return 1 if exists $seen{$_};
		$seen{$_} = 1;
	}
	return 0;
}
sub have_slash {
	for (@_) {
		return 1 if m{/};
	}
	return 0;
}

my %newdirabbrevs = %dirabbrevs;
while (!have_duplicate(values %newdirabbrevs)) {
	%dirabbrevs = %newdirabbrevs;
	last if !have_slash(values %dirabbrevs);
	%newdirabbrevs = %dirabbrevs;
	for (values %newdirabbrevs) {
		s{^[^/]*/}{};
	}
}

my %times;
my @colwidth = ((0)x@dirs);
for my $i (0..$#dirs) {
	my $d = $dirs[$i];
	my $w = length (exists $dirabbrevs{$d} ? $dirabbrevs{$d} : $dirnames{$d});
	$colwidth[$i] = $w if $w > $colwidth[$i];
}
for my $t (@subtests) {
	my $firstr;
	for my $i (0..$#dirs) {
		my $d = $dirs[$i];
		my $base = "test-results/$prefixes{$d}$t";
		$times{$prefixes{$d}.$t} = [];
		foreach my $type (qw(times size)) {
			if (-e "$base.$type") {
				$times{$prefixes{$d}.$t} = [get_times("$base.$type")];
				last;
			}
		}
		my ($r,$u,$s) = @{$times{$prefixes{$d}.$t}};
		my $w = length format_times($r,$u,$s,$firstr);
		$colwidth[$i] = $w if $w > $colwidth[$i];
		$firstr = $r unless defined $firstr;
	}
}
my $totalwidth = 3*@dirs+$descrlen;
$totalwidth += $_ for (@colwidth);

printf "%-${descrlen}s", "Test";
for my $i (0..$#dirs) {
	my $d = $dirs[$i];
	printf "   %-$colwidth[$i]s", (exists $dirabbrevs{$d} ? $dirabbrevs{$d} : $dirnames{$d});
}
print "\n";
print "-"x$totalwidth, "\n";
for my $t (@subtests) {
	printf "%-${descrlen}s", $descrs{$t};
	my $firstr;
	for my $i (0..$#dirs) {
		my $d = $dirs[$i];
		my ($r,$u,$s) = @{$times{$prefixes{$d}.$t}};
		printf "   %-$colwidth[$i]s", format_times($r,$u,$s,$firstr);
		$firstr = $r unless defined $firstr;
	}
	print "\n";
}
