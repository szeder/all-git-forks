#!/usr/bin/perl
#
# Example implementation for the Git filter protocol version 2
# See Documentation/gitattributes.txt, section "Filter Protocol"
#
# This implementation supports two special test cases:
# (1) If data with the filename "clean-write-fail.r" is processed with
#     a "clean" operation then the write operation will die.
# (2) If data with the filename "smudge-write-fail.r" is processed with
#     a "smudge" operation then the write operation will die.
#

use strict;
use warnings;

my $MAX_PACKET_CONTENT_SIZE = 65516;

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
            die "invalid packet";
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

open my $debug, ">>", "output.log";
print $debug "start\n";
$debug->flush();

packet_write("git-filter-protocol\n");
packet_write("version 2\n");
packet_write("clean smudge\n");
print $debug "wrote filter header\n";
$debug->flush();

while (1) {
    my $command = packet_read();
    unless ( defined($command) ) {
        exit();
    }
    chomp $command;
    print $debug "IN: $command";
    $debug->flush();
    my $filename = packet_read();
    chomp $filename;
    print $debug " $filename";
    $debug->flush();
    my $filelen = packet_read();
    chomp $filelen;
    print $debug " $filelen";
    $debug->flush();

    $filelen =~ /\A\d+\z/ or die "bad filelen: $filelen";
    my $output;

    if ( $filelen > 0 ) {
        my $input = "";
        {
            binmode(STDIN);
            my $buffer;
            my $done = 0;
            while ( !$done ) {
                ( $done, $buffer ) = packet_read();
                $input .= $buffer;
            }
            print $debug " [OK] -- ";
            $debug->flush();
        }

        if ( $command eq "clean" ) {
            $output = rot13($input);
        }
        elsif ( $command eq "smudge" ) {
            $output = rot13($input);
        }
        else {
            die "bad command";
        }
    }

    my $output_len = length($output);
    packet_write("$output_len\n");
    print $debug "OUT: $output_len ";
    $debug->flush();
    if ( $output_len > 0 ) {
        if (   ( $command eq "clean" and $filename eq "clean-write-fail.r" )
            or
            ( $command eq "smudge" and $filename eq "smudge-write-fail.r" ) )
        {
            print $debug " [FAIL]\n";
            $debug->flush();
            die "write error";
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
        }
    }
}
