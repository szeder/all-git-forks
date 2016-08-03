#!/usr/bin/perl
#
# Example implementation for the Git filter protocol version 2
# See Documentation/gitattributes.txt, section "Filter Protocol"
#
# The script takes the list of supported protocol capabilities as
# arguments ("clean", "smudge", etc).
#
# This implementation supports three special test cases:
# (1) If data with the pathname "clean-write-fail.r" is processed with
#     a "clean" operation then the write operation will die.
# (2) If data with the pathname "smudge-write-fail.r" is processed with
#     a "smudge" operation then the write operation will die.
# (3) If data with the pathname "reject.r" is processed with any
#     operation then the filter signals that it does not want to process
#     the file.
#

use strict;
use warnings;

my $MAX_PACKET_CONTENT_SIZE = 65516;
my @capabilities            = @ARGV;

sub rot13 {
    my ($str) = @_;
    $str =~ y/A-Za-z/N-ZA-Mn-za-m/;
    return $str;
}

sub packet_read {
    my $buffer;
    my $bytes_read = read STDIN, $buffer, 4;
    if ( $bytes_read == 0 ) {
        return;
    }
    elsif ( $bytes_read != 4 ) {
        die "invalid packet size '$bytes_read' field";
    }
    my $pkt_size = hex($buffer);
    if ( $pkt_size == 0 ) {
        return ( 1, "" );
    }
    elsif ( $pkt_size > 4 ) {
        my $content_size = $pkt_size - 4;
        $bytes_read = read STDIN, $buffer, $content_size;
        if ( $bytes_read != $content_size ) {
            die "invalid packet ($content_size expected; $bytes_read read)";
        }
        return ( 0, $buffer );
    }
    else {
        die "invalid packet size";
    }
}

sub packet_write {
    my ($packet) = @_;
    print STDOUT sprintf( "%04x", length($packet) + 4 );
    print STDOUT $packet;
    STDOUT->flush();
}

sub packet_flush {
    print STDOUT sprintf( "%04x", 0 );
    STDOUT->flush();
}

open my $debug, ">>", "rot13-filter.log";
print $debug "start\n";
$debug->flush();

packet_write("git-filter-protocol\n");
packet_write("version=2\n");
packet_write( "capabilities=" . join( ' ', @capabilities ) . "\n" );
print $debug "wrote filter header\n";
$debug->flush();

while (1) {
    my ($command) = packet_read() =~ /^command=([^=]+)\n$/;
    unless ( defined($command) ) {
        exit();
    }
    print $debug "IN: $command";
    $debug->flush();

    if ( $command eq "shutdown" ) {
        print $debug " -- [OK]";
        $debug->flush();
        packet_write("result=success\n");
        exit();
    }

    my ($pathname) = packet_read() =~ /^pathname=([^=]+)\n$/;
    print $debug " $pathname";
    $debug->flush();

    my $input = "";
    {
        binmode(STDIN);
        my $buffer;
        my $done = 0;
        while ( !$done ) {
            ( $done, $buffer ) = packet_read();
            $input .= $buffer;
        }
        print $debug " " . length($input) . " [OK] -- ";
        $debug->flush();
    }

    my $output;
    if ( $pathname eq "reject.r" ) {
        $output = "";
    }
    elsif ( $command eq "clean" and grep( /^clean$/, @capabilities ) ) {
        $output = rot13($input);
    }
    elsif ( $command eq "smudge" and grep( /^smudge$/, @capabilities ) ) {
        $output = rot13($input);
    }
    else {
        die "bad command $command";
    }

    print $debug "OUT: " . length($output) . " ";
    $debug->flush();

    if ( $pathname eq "${command}-write-fail.r" ) {
        print $debug "[WRITE FAIL]\n";
        $debug->flush();
        die "write error";
    }
    elsif ( $pathname eq "reject.r" ) {
        packet_flush();
        print $debug "[REJECT]\n";
        $debug->flush();
        packet_write("result=reject\n");
    }
    else {
        while ( length($output) > 0 ) {
            my $packet = substr( $output, 0, $MAX_PACKET_CONTENT_SIZE );
            packet_write($packet);
            if ( length($output) > $MAX_PACKET_CONTENT_SIZE ) {
                $output = substr( $output, $MAX_PACKET_CONTENT_SIZE );
            }
            else {
                $output = "";
            }
        }
        packet_flush();
        print $debug "[OK]\n";
        $debug->flush();
        packet_write("result=success\n");
    }
}
