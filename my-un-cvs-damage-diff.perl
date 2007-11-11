#!/usr/bin/perl

# This is a simple state machine.
#
# There is the state of the current file; its header is stored
# in $current_file to avoid outputting it when all hunks were
# culled.  It is only printed before the first hunk, and then
# set to "" to avoid outputting it twice.
#
# There are the states of the current hunk, stored in
# * $current_hunk (possibly modified hunk)
# * $start_minus, $start_plus (from the original)
# * $plus, $minus, $space (the current count of the respective lines)
# a hunk is only printed (in flush_hunk) if any '+' or '-' lines
# are left after filtering.
#
# For $Log..$, there is the state $skip_logs, which is set to 1
# after seeing such a line, and set to 0 when the first line
# was seen which does not begin with '+'.
#
# A particularly nasty special case is when a single "*" was
# misattributed by the diff to be _inserted before_ a $Log, instead
# of _appended after_ a $Log.
# This is the purpose of the $before_log and $after_log variables:
# if not empty, the state machine expects the next line to begin
# with '+' or '-', respectively, and followed by a $Log.  If this
# expectation is not met, the variable is output.
#
# The variable $plus_minus_adjust contains the number of lines which
# were skipped from the "+" side, so that the correct offset is shown.


# This function gets a hunk header.
#
# It initializes the state variables described above

sub init_hunk {
	my $line = $_[0];
	$current_hunk = "";
	($start_minus, $dummy, $start_plus, $dummy) =
		($line =~ /^\@\@ -(\d+)(,\d+|) \+(\d+)(,\d+|) \@\@/);
	$plus = $minus = $space = 0;
	$skip_logs = 0;
	$before_log = '';
	$after_log = '';

	# we prefer /dev/null as original file name when a file is new
	if ($start_minus eq 0) {
		$current_file =~ s/\n--- .*\n/\n--- \/dev\/null\n/;
	} elsif ($start_plus eq 0) {
		$current_file =~ s/\n\+\+\+ .*\n/\n+++ \/dev\/null\n/;
	}
}

# This function is called whenever there is possibly a hunk to print.
# Nothing is printed if no '+' or '-' lines are left.
# Otherwise, if the file header was not yet shown, it does so now.

sub flush_hunk {
	if (($plus > 0 || $minus > 0) && $current_hunk ne '') {
		if ($current_file ne "") {
			print $current_file;
			$current_file = "";
		}
		$minus += $space;
		$plus += $space;
		print "\@\@ -$start_minus,$minus "
			. "+" . ($start_plus - $start_plus_adjust)
			. ",$plus \@\@\n";
		print $current_hunk;
		$current_hunk = '';
	}
}

# This adds a line to the current hunk and updates $space, $plus or $minus

sub add_line {
	my $line = $_[0];
	$current_hunk .= $line;
	if ($line =~ /^ /) {
		$space++;
	} elsif ($line =~ /^\+/) {
		$plus++;
	} elsif ($line =~ /^-/) {
		$minus++;
	} elsif ($line =~ /^\\/) {
		# do nothing
	} else {
		die "Unexpected line: $line";
	}
}

# This function splits the current hunk into the part before the current
# line, and the part after the current line.

sub skip_line {
	my $line = $_[0];

	if ($start_minus == 0) {
		# This patch adds a new file, just ignore that line
		return;
	} elsif ($start_plus == 0) {
		# This patch removes a file, so include the line nevertheless
		add_line $_;
		return;
	}

	flush_hunk;
	if ($line =~ /^-/) {
		$minus++;
	} elsif ($line =~ /^\+/) {
		$plus++;
		$start_plus_adjust++;
	}
	init_hunk "@@ -" . ($start_minus + $minus + $space)
		. " +" . ($start_plus + $plus + $space)
		. " @@\n";
}

$simple_keyword = "Id|Revision|Author|Date|Source|Header";

# This is the main loop

sub check_file {
	$_ = $_[0];
	$current_file = $_;
	$start_plus_adjust = 0;
	while (<>) {
		if (/^\@\@/) {
			last;
		}
		$current_file .= $_;
	}

	init_hunk $_;

	# check hunks
	while (<>) {
		if ($before_log) {
			if (!/\+.*\$Log.*\$/) {
				add_line $before_log;
			} else {
				skip_line $before_log;
			}
			$before_log = '';
		}

		if ($after_log) {
			if (!/-.*\$Log.*\$/) {
				add_line $after_log;
			} else {
				skip_line $after_log;
			}
			$after_log = '';
		}

		if ($skip_logs) {
			if (/^\+/) {
				skip_line $_;
				$skip_logs = 1;
			} else {
				$skip_logs = 0;
				if (/^ *\*$/) {
					$after_log = $_;
				}
			}
		} elsif (/^\+.*\$($simple_keyword).*\$/) {
			skip_line $_;
		} elsif (/^\@\@.*/) {
			flush_hunk;
			init_hunk $_;
		} elsif (/^diff/) {
			flush_hunk;
			return;
		} elsif (/^-.*\$($simple_keyword).*\$/) {
			# fake new hunk
			skip_line $_;
		} elsif (/^\+ *\*$/) {
			$before_log = $_;
		} elsif (/^([- \+]).*\$Log.*\$/) {
			skip_line $_;
			$skip_logs = 1;
		} else {
			add_line $_;
		}
	}
}

# This loop just shows everything before the first diff, and then hands
# over to check_file whenever it sees a line beginning with "diff".

while (<>) {
	if (/^diff/) {
		do {
			check_file $_;
		} while(/^diff/);
	} else {
		printf $_;
	}
}
flush_hunk;
