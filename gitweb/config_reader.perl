#!/usr/bin/perl

use strict;
use warnings;

use Text::Balanced qw(extract_delimited);
#require Tie::Memoize;
use Data::Dumper;



sub read_config {
	my $configfile = shift;
	my %config;

	print "$configfile\n";

	open my $fd, $configfile
		or die "Cannot open $configfile: $!";

	my ($sect, $subsect, $sectfull, $key, $value);
	while (my $line = <$fd>) {
		chomp $line;

		print ".$line\n";


		if ($line =~ m/^\s*[;#](.*)$/) {
			# pure comment

			#print "!comment: $1\n";

		} elsif ($line =~ m/^\s*\[\s*([^][:space:]]*)\s*\](.*)$/) {
			# section without subsection

			#print "!section: $1\n";
			#print "!after:   $2\n" if defined $2;

			$sect = lc($1);
			$subsect = undef;

			$sectfull = $sect;

			#print "!SECTION: $sectfull\n";

		} elsif ($line =~ m/\s*\[([^][:space:]]*)\s"((?:\\.|[^"])*)"\](.*)$/) {
			# section with subsection

			#print "!section: $1\n";
			#print "!subsect: $2\n";
			#print "!after:   $3\n" if defined $3;

			$sect = lc($1);

			$subsect = $2;
			$subsect =~ s/\\(.)/$1/g;

			$sectfull = "$sect.$subsect";

			#print "!SECTION: $sectfull\n";

		#} elsif ($line =~ m/\s*(\w+)\s*=\s*((?:\\"|[^"])*?)\s*([;#].*)?$/) {
		} elsif ($line =~ m/\s*(\w+)\s*=\s*(.*?)\s*$/) {
			# variable assignment

			$key   = lc($1);
			$value = $2;

			my $text = $value;
			my $result = '';
		DELIM: {
				do {
					($next, $remainder, $prefix) =
						extract_delimited($text, '"', qr/(?:\\.|[^"])*/);

					print "TEXT = .$text.\n";
					print "PREF = .$prefix.\n";
					print "NEXT = .$next.\n";
					print "REST = .$remainder.\n";

					if ($prefix =~ s/\s*[;#].*$//) {
						print "Pref = .$prefix.\n";
						$result .= $prefix;
						last DELIM;
					} else {
						$result .= $prefix if $prefix;
						if ($next && $next =~ s/^"(.*)"$/$1/) {
							$result .= $next;
						}
					}

					$text = $remainder;
				} while ($remainder && $next);
			} # DELIM:

			if ($remainder) {
				$remainder =~ s/\s*[;#].*$//;
				$result .= $remainder;
			}

			$value  =~ s/\\(.)/$1/g if defined $value;
			$result =~ s/\\(.)/$1/g if $result;

			print "!result: $result!\n";
			#print "!sect:  $sectfull!\n" if defined $sectfull;
			#print "!key:   $key!\n"      if defined $key;
			print "!value:  $value!\n"    if defined $value;
			#print "!after: $rest!\n"     if defined $rest;

			#print join(', ', @value), "\n";
			#print Dumper(\@value);

			if (exists $config{"$sectfull.$key"}) {
				#push @{$config{"$sectfull.$key"}}, $value;
				push @{$config{"$sectfull.$key"}}, $result;
			} else {
				#$config{"$sectfull.$key"} = [ $value ];
				$config{"$sectfull.$key"} = [ $result ];
			}

		} elsif ($line =~ m/^\s*(\w+)\s*(:?[;#].*)?$/) {
			# boolean variable without value

			$key = lc($1);

			if (exists $config{"$sectfull.$key"}) {
				push @{$config{"$sectfull.$key"}}, undef;
			} else {
				$config{"$sectfull.$key"} = [ undef ];
			}
		} # end if
	}

	close $fd
		or die "Cannot close $configfile: $!";

	print "\n";
	print '-' x 40, "\n";

	#print Dumper(\%config);
	foreach my $ckey (sort keys %config) {
		foreach my $cvalue (@{$config{$ckey}}) {
			if (defined $cvalue) {
				print "$ckey=$cvalue!\n";
			} else {
				print "$ckey!\n";
			}
		}
	}

	print "\n";
	print '-' x 40, "\n";
	print `GIT_CONFIG=$configfile git repo-config --list`;
}

read_config("/home/jnareb/git/.git/config");
read_config("/tmp/jnareb/gitconfig");
