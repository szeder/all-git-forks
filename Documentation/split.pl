sub extract {
    my ($line) = @_;
    open CC, ">config/$filename" || die "failed to open config/$filename";
    foreach $l (@lines[($start - 1)..($line - 2)]) {
	$l =~ s/^include::/include::..\//;
	print CC $l;
    }
    close CC;
    print C "include::config/$filename" . "[]\n";
}

open C, "config.txt" || die "failed to open config.txt";
our @lines = <C>;
close C;

our $start = 0;
our $filename;

open F, "grep -n '^[a-z].*::\$' config.txt|" || die "failed to grep";
open C, ">config.txt.new" || die "unable to open config.txt.new";
while (<F>) {
    chomp;
    $_ =~ m/([^:]*):(.*)/;
    my $line = $1;
    my $name = $2;
    $name =~ s/\*/_/g;
    $name =~ s/<//g;
    $name =~ s/>//g;
    $name =~ s/::$//;
    $name = "http.speedLimit" if $name eq "http.lowSpeedLimit, http.lowSpeedTime";
    $name = "gitcvs.userpass" if $name eq "gitcvs.dbuser, gitcvs.dbpass";
    next if $line - $start == 1;
    if ($start > 0) {
	extract $line;
    } else {
	foreach $l (@lines[0..($line-2)]) {
	    print C $l;
	}
    }
    $start = $line;
    $filename = $name . ".txt";
}

extract $#lines + 2;
close C;
system "mv config.txt.new config.txt";
