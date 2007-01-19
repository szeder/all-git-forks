#!/usr/bin/perl

use strict;
use warnings;

use Text::Balanced qw(extract_delimited);


sub read_config {
	my $configfile = shift;
	my $section = shift;
	my %config;

	open my $fd, $configfile
		or die "Cannot open $configfile: $!";

	my $sectfull;
	while (my $line = <$fd>) {
		chomp $line;

		if ($line =~ m/^\s*\[\s*([^][:space:]]*)\s*\](.*)$/) {
			# section without subsection

			my $sect = lc($1);
			$line = $2;

			$sectfull = $sect;

		} elsif ($line =~ m/\s*\[([^][:space:]]*)\s"((?:\\.|[^"])*)"\](.*)$/) {
			# section with subsection

			my $sect = lc($1);
			my $subsect = $2;
			$line = $3;
			$subsect =~ s/\\(.)/$1/g;

			$sectfull = "$sect.$subsect";

		}

		# if instead of elsif to cover the following situation:
		#  [section] var = value
		if ($line =~ m/\s*(\w+)\s*=\s*(.*?)\s*$/) {
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

	return wantarray ? %config : \%config;
}

my %config;

%config = read_config("~/git/.git/config");
%config = read_config("/tmp/jnareb/gitconfig");

foreach my $ckey (sort keys %config) {
	foreach my $cvalue (@{$config{$ckey}}) {
		if (defined $cvalue) {
			print "$ckey=$cvalue\n";
		} else {
			print "$ckey\n";
		}
	}
}
