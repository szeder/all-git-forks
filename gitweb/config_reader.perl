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

	my $sectfull;
	while (my $line = <$fd>) {
		chomp $line;

		if ($line =~ m/^\s*\[\s*([^][:space:]]*)\s*\](.*)$/) {
			# section without subsection

			my $sect = lc($1);

			$sectfull = $sect;

		} elsif ($line =~ m/\s*\[([^][:space:]]*)\s"((?:\\.|[^"])*)"\](.*)$/) {
			# section with subsection

			my $sect = lc($1);
			my $subsect = $2;
			$subsect =~ s/\\(.)/$1/g;

			$sectfull = "$sect.$subsect";

		} elsif ($line =~ m/\s*(\w+)\s*=\s*(.*?)\s*$/) {
			# variable assignment

			my $key = lc($1);
			my $rhs = $2;

			my $value = '';
			my ($next, $remainder, $prefix) = qw();
		DELIM: {
				do {
					($next, $remainder, $prefix) =
						extract_delimited($rhs, '"', qr/(?:\\.|[^"])*/);

					if ($prefix =~ s/\s*[;#].*$//) {
						# comment in unquoted part
						$value .= $prefix;
						last DELIM;
					} else {
						$value .= $prefix if $prefix;
						if ($next && $next =~ s/^"(.*)"$/$1/) {
							$value .= $next;
						}
					}

					$rhs = $remainder;
				} while ($rhs && $next);
			} # DELIM:

			if ($remainder) {
				$remainder =~ s/\s*[;#].*$//;
				$value .= $remainder;
			}

			$value =~ s/\\(.)/$1/g;

			if (exists $config{"$sectfull.$key"}) {
				push @{$config{"$sectfull.$key"}}, $value;
			} else {
				$config{"$sectfull.$key"} = [ $value ];
			}

		} elsif ($line =~ m/^\s*(\w+)\s*(:?[;#].*)?$/) {
			# boolean variable without value

			my $key = lc($1);

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
