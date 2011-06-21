#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long;


my %ignore;
my $follow = 1;
my $rc = GetOptions(
	"mainlist=s" => \my $mainlistfile,
	"ignore=s" => sub { $ignore{$_[1]} = 1 },
	"deps=s" => \my $deps,
	"follow|follow-includes!" => \$follow,
	"ignore-all" => sub { $follow = 0 },
);

if (!$rc || !defined $mainlistfile) {
	print "$0 --mainlist=<mainlist> [--ignore=<ignore>...|--ignore-all] <asciidoc_manpage>...\n";
	exit 1;
}

if ($deps) {
	$ignore{$deps} = 1;
	my %included = find_included($mainlistfile, $follow ? @ARGV : ());
	print "$deps : ".join(" ", keys %included)."\n";
	exit 0;
}

my ($mainlist, $mainvars) = read_varlist($mainlistfile);

my %var_manpages;
my %manpage_section;

foreach my $name (@ARGV) {
	read_man_txt($name);
}

my %missing_vars =
	map  { $_ => $var_manpages{$_} }
	grep { !exists $mainlist->{lc($_)} }
	keys %var_manpages;

my %insert = find_insertion_points($mainlist, \%missing_vars);

open my $fh, '<', $mainlistfile
	or die "Couldn't open '$mainlistfile' for reading: $!";
while (<$fh>) {
	if (exists $insert{$.}) {
		print vars_documentation($insert{$.}, \%missing_vars);
		print "\n";
	}
	print;
}
# special case: insertion after last line in $mainlistfile
print vars_documentation($insert{-1}, \%missing_vars)
	if exists $insert{-1};
close $fh
	or die "Couldn't close '$mainlistfile': $!";

exit 0;

# ----------------------------------------------------------------------
# ----------------------------------------------------------------------
# ----------------------------------------------------------------------

sub find_included {
	my @files = @_;
	my %included = ();

	foreach my $file (@files) {
		open my $fh, '<', $file
			or die "Couldn't open '$file' for reading: $!";
		while (<$fh>) {
			if (/^include::\s*(\S+)\s*\[\]/) {
				$included{$1} = 1;
				%included = (%included, find_included($1));
			}
		}
		close $fh
			or die "Couldn't close '$file': $!";
	}
	return %included;
}

sub read_varlist {
	my ($filename) = @_;

	open my $fh, '<', $filename
		or die "Couldn't open '$filename' for reading: $!";

	my (%mainlist, @mainvars);
	while (<$fh>) {
		if (/^(\S+)::/) {
			my $v = $1;
			push @mainvars, $v;
			$mainlist{lc($v)} = $.;
		} elsif (/^include::\s*(\S+)\s*\[\]/) {
			my $includefile = $1;
			$ignore{$includefile} = 1;
			my (undef, $includevars) = read_varlist($includefile);
			push @mainvars, @$includevars;
			$mainlist{lc($_)} = $. foreach @$includevars;
		}
	}

	close $fh
		or die "Couldn't close '$filename': $!";

	return \%mainlist, \@mainvars;
}

sub read_man_txt {
	my ($filename, $manpage, $in_config_section) = @_;
	if (!defined $manpage) {
		$manpage = $filename;
		$manpage =~ s/\.txt//;
	}

	open my $fh, '<', $filename
		or die "Couldn't open '$filename' for reading: $!";
	my $lastline;
	my $last_section = "";
	while (my $line = <$fh>) {
		chomp $line;
		if ($. < 5 && $line =~ /^$manpage\((\d+)\)/) {
			$manpage_section{$manpage} = $1;
		}
		if ($line =~ /^([a-z0-9]+\.[a-zA-Z<>0-9.]+)::\s*$/ &&
		    $in_config_section) {
			push @{$var_manpages{$1}}, $manpage;
		}
		if ($line =~ /^include::\s*(\S+)\s*\[\]/ &&
		    $follow && !exists $ignore{$1}) {
			read_man_txt($1, $manpage, $in_config_section);
		}
		my ($section, $level) = check_section_header($lastline, $line);
		if ($section) {
			$last_section = $section;
			if (!$in_config_section && $section =~ /^Config/i) {
				$in_config_section = $level;
			} elsif ($in_config_section && $level <= $in_config_section) {
				$in_config_section = 0;
			}
		}
		$lastline = $line;
	}
	close $fh
		or die "Couldn't close '$filename': $!";
}

# supports only two line titles (for now)
# http://www.methods.co.nz/asciidoc/userguide.html#X17
sub check_section_header {
	my ($lastline, $line) = @_;

	# levels moved 1 level up, i.e. top level is 1 not 0
	my %level_underline = ('=' => 1, '-' => 2, '~' => 3, '^' => 4, '+' => 5);

	return unless ($lastline && $line);
	if (length($lastline) == length($line) &&
	    $line =~ /^([-=~^+])\1*$/) {
		return ($lastline, $level_underline{$1});
	}
	return;
}

sub find_insertion_points {
	my ($mainlist, $missing_vars) = @_;
	my %insert;

	my %all_vars = (%$mainlist, %$missing_vars);
	my $lineno = -1; # means after last line

	# reverse order because we want to find a place before which to insert
	# generated documentation; it is easy to find where description
	# of variable begins, but in general harder to find where it ends.
	my @sorted_vars = reverse sort { lc($a) cmp lc($b) } keys %all_vars;
	foreach my $key (@sorted_vars) {
		my $val = $all_vars{$key};
		if (ref $val) {
			# this came from %$missing_vars
			push @{$insert{$lineno}}, $key;
		} else {
			# this came from %$mainlist
			if ($lineno < 0) {
				# $lineno < 0 means after end of file (special case)
				$lineno = $val;
			} else {
				# this is in case of unsorted entries in $mainlistfile
				$lineno = $val < $lineno ? $val : $lineno; # min($val, $lineno)
			}
		}
	}
	return %insert;
}

sub vars_documentation {
	my ($keylist, $vars) = @_;
	my @keys = sort @$keylist;
	my %out;

	# generate output for each key now, because it is easier to compare
	# strings than arrays; comparing which is needed for compacting output
	foreach my $k (@keys) {
		$out{$k} = "\tSee: ".gen_links($vars->{$k}).".\n";
	}

	my $output = '';
	while (my $k = pop @keys) {
		$output .= $k."::\n";
		unless (@keys && $out{$k} eq $out{$keys[0]}) {
			$output .= $out{$k};
		}
	}
	return $output;
}

sub gen_links {
	my $manpages = shift;
	return join(", ", map { linkgit($_) } @$manpages);
}

sub linkgit {
	my $manpage = shift;

	if (!exists $manpage_section{$manpage}) {
		warn "section for $manpage unknown, assuming '1'\n";
		$manpage_section{$manpage} = 1;
	}
	return "linkgit:${manpage}[$manpage_section{$manpage}]";
}

__END__
