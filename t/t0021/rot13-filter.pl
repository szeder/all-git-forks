#!/usr/bin/perl
#
# Example implementation for the Git filter protocol version 2
# See Documentation/gitattributes.txt, section "Filter Protocol"
#
# The script takes the list of supported protocol capabilities as
# arguments ("clean", "smudge", etc).
#
# This implementation supports special test cases:
# (1) If data with the pathname "clean-write-fail.r" is processed with
#     a "clean" operation then the write operation will die.
# (2) If data with the pathname "smudge-write-fail.r" is processed with
#     a "smudge" operation then the write operation will die.
# (3) If data with the pathname "error.r" is processed with any
#     operation then the filter signals that it cannot or does not want
#     to process the file.
# (4) If data with the pathname "error-all.r" is processed with any
#     operation then the filter signals that it cannot or does not want
#     to process the file and any file after that is processed with the
#     same command.
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

        # EOF - Git stopped talking to us!
        exit();
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

( packet_read() eq ( 0, "git-filter-client" ) ) || die "bad initialization";
( packet_read() eq ( 0, "version=2" ) )         || die "bad version";
( packet_read() eq ( 1, "" ) )                  || die "bad version end";

packet_write("git-filter-server\n");
packet_write("version=2\n");

( packet_read() eq ( 0, "clean=true" ) )  || die "bad capability";
( packet_read() eq ( 0, "smudge=true" ) ) || die "bad capability";
( packet_read() eq ( 1, "" ) )            || die "bad capability end";

foreach (@capabilities) {
    packet_write( $_ . "=true\n" );
}
packet_flush();
print $debug "wrote filter header\n";
$debug->flush();

while (1) {
    my ($command) = packet_read() =~ /^command=([^=]+)\n$/;
    print $debug "IN: $command";
    $debug->flush();

    my ($pathname) = packet_read() =~ /^pathname=([^=]+)\n$/;
    print $debug " $pathname";
    $debug->flush();

    # Flush
    packet_read();

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
    if ( $pathname eq "error.r" or $pathname eq "error-all.r" ) {
        $output = "";
    }
    elsif ( $command eq "clean" and grep( /^clean$/, @capabilities ) ) {
        $output = rot13($input);
    }
    elsif ( $command eq "smudge" and grep( /^smudge$/, @capabilities ) ) {
        $output = rot13($input);
    }
    else {
        die "bad command '$command'";
    }

    print $debug "OUT: " . length($output) . " ";
    $debug->flush();

    if ( $pathname eq "error.r" ) {
        print $debug "[ERROR]\n";
        $debug->flush();
        packet_write("status=error\n");
        packet_flush();
    }
    elsif ( $pathname eq "error-all.r" ) {
        print $debug "[ERROR-ALL]\n";
        $debug->flush();
        packet_write("status=error-all\n");
        packet_flush();
    }
    else {
        packet_write("status=success\n");
        packet_flush();

        if ( $pathname eq "${command}-write-fail.r" ) {
            print $debug "[WRITE FAIL]\n";
            $debug->flush();
            die "${command} write error";
        }

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
        packet_flush();
    }
}
