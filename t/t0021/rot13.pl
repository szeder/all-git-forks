#!/usr/bin/env perl

use strict;
use warnings;
use autodie;

$| = 1;

sub rot13 {
    my ($str) = @_;
    $str =~ y/A-Za-z/N-ZA-Mn-za-m/;
    return $str;
}

open my $debug, '>>', '/mnt/migrations/projects/git/t/output';
$debug->autoflush(1);
print $debug "start\n";

print STDOUT "version 1";
print $debug "wrote version\n";

while (1) {
    my $command = <STDIN>;
    unless (defined($command)) {
        exit();
    }
    chomp $command;
    print $debug "read command: $command\n";
    my $filename = <STDIN>;
    chomp $filename;
    print $debug "read filename: $filename\n";
    my $filelen  = <STDIN>;
    chomp $filelen;
    print $debug "read filelen: $filelen\n";

    $filelen = int($filelen);
    my $output;

    if ( $filelen > 0 ) {
        my $input;
        {
            binmode(STDIN);
            my $bytes_read = 0;
            $bytes_read = read STDIN, $input, $filelen;
            if ( $bytes_read != $filelen ) {
                die "not enough read";
            }
            print $debug "read $bytes_read bytes\n";
        }

        if ( $command eq 'clean') {
            $output = rot13($input);
        }
        elsif ( $command eq 'smudge' ) {
            $output = rot13($input);
        }
        else {
            die "bad command\n";
        }
    }

    my $output_len = length($output);
    print STDOUT "$output_len\n";
    print $debug "wrote output length: $output_len\n";
    if ( $output_len > 0 ) {
        print STDOUT $output;
        print $debug "wrote output\n";
    }
}
