#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
#
#
use strict;
use warnings;
use AnyEvent;
use AnyEvent::Socket;
use AnyEvent::Handle;
use Data::Dumper;
use IO::All;
use JSON::XS;
use List::Util qw(first);
use v5.10;

my $tcp_conn = undef;
my $readbuffer = '';
# contains the IDs of the bus participants
my %mapping = (
    pinpad => 1,
);
my %groups = (
    pinpad => 50,
    temperature => 51,
);

my %conns;

# TODO: statistics einbauen über pakete mit falscher checksum oder in falschem framing, die dann gedroppt werden

tcp_server '127.0.0.1', 8888, sub {
    my ($fh, $host, $port) = @_;

    my $id = "$host:$port";

    say "Connection from $host:$port";
    my $handle; # avoid direct assignment so on_eof has it in scope.
    $handle = AnyEvent::Handle->new(
        fh     => $fh,
        on_error => sub {
            warn "error $_[2]\n";
            $_[0]->destroy;
            delete $conns{$id};
        },
        on_eof => sub {
            $handle->destroy; # destroy handle
            warn "done.\n";
            delete $conns{$id};

        }
    );
    my $destination = undef;
    my $content_length = undef;
    my $readcb;
    $readcb = sub {
        my ($handle, $line) = @_;

        say "received line $line";
        if ($line =~ /^POST / || $line =~ /^PUT /) {
            my ($method, $url) = ($line =~ /^(POST|PUT) ([^ ]+)/);
            say "post to URL $url";
            # check if this is a send request to the bus
            if ($url =~ m,^/bus/,) {
                # check to which participant
                my ($dest) = ($url =~ m,^/bus/([^/]+),);
                say "destination $dest";
                if (!exists $mapping{$dest}) {
                    syswrite $fh, "HTTP/1.0 404 Not Found\r\nContent-Length: " . length("No such participant") . "\r\nConnection: close\r\n\r\nNo such participant";
                    return;
                }
                $destination = $mapping{$dest};
                say "give me more";
            }
        }

        if ($line =~ /^GET /) {
            my ($url) = ($line =~ /^GET ([^ ]+)/);
            if ($url =~ m,^/bus/,) {
                # check to which participant
                my ($dest) = ($url =~ m,^/bus/([^/]+),);
                say "destination $dest";
                if (!exists $groups{$dest}) {
                    syswrite $fh, "HTTP/1.0 404 Not Found\r\nContent-Length: " . length("No such participant") . "\r\nConnection: close\r\n\r\nNo such participant";
                    return;
                }
                $conns{$id} = { group => $groups{$dest}, handle => $handle };
            }
        }

        if ($line =~ /^Content-Length: /) {
            ($content_length) = ($line =~ /^Content-Length: ([0-9]+)/);
            say "parsed content-length $content_length";
        }

        if ($line eq '' && defined($content_length)) {
            say "empty line, so the body needs to follow now";
            # TODO: check for content-length
            $handle->push_read(chunk => $content_length, sub {
                my ($handle, $chunk) = @_;

                # TODO: check for destination
                say "should forward packet with payload $chunk";
                my $pkt = sprintf('^%02d%02d%s$', $destination, $content_length, $chunk);
                say "pkt = $pkt";
                # TODO: check for tcp_conn
                $tcp_conn->push_write($pkt);
                syswrite $fh, "HTTP/1.0 200 OK\r\nContent-Length: " . length("Packet sent") . "\r\nConnection: close\r\n\r\nPacket sent";
		my $nopcb;
		$nopcb = sub {
			$handle->push_read(line => $nopcb);
		};
		$handle->push_read(line => $nopcb);
            });
        } else {
            $handle->push_read(line => $readcb);
        }
    };
    say "pushing readcb";
    $handle->push_read(line => $readcb);
};


tcp_connect '192.168.33.50', 6001, sub {
    my ($fh) = @_ or die "connection failed: $!";

    my $handle; # avoid direct assignment so on_eof has it in scope.
     $handle = AnyEvent::Handle->new(
        fh     => $fh,
        on_error => sub {
           warn "error $_[2]\n";
           $_[0]->destroy;
        },
        on_eof => sub {
           $handle->destroy; # destroy handle
           warn "done.\n";
        }
    );

    $tcp_conn = $handle;

    my $readcb;
    $readcb = sub {
        my ($handle, $byte) = @_;

        #say "length(readbuffer) = " . length($readbuffer);
        my ($u) = unpack("C", $byte);
        #say "received: " . sprintf("%02x", $u);
        if (length($readbuffer) == 0) {
            if ($byte ne '^') {
                say "Invalid byte, discarding: $byte";
            } else {
                $readbuffer .= $byte;
            }
        } else {
            if (length($readbuffer) > 125) {
                say "Invalid packet (too long), dropping";
                $readbuffer = '';
            }
            # stop-byte
            if ($byte eq '$') {
                #say "packet received, distributing";
                distribute_packet();
            } else {
                $readbuffer .= $byte;
            }
        }
        #say "buffer now: $readbuffer";

        $handle->push_read(chunk => 1, $readcb);
    };
    $handle->push_read (chunk => 1, $readcb);
};

#
# unpacks the receive buffer contents into a hash and distributes it
#
sub distribute_packet {
    # $readbuffer contains something like ^0004<pkt>$
    my ($destination,
        $source,
        $header_chk,
        $payload_chk,
        $length_hi,
        $length_lo) = unpack("CCCCCC", substr($readbuffer, 5));
    # TODO: checksum!

    my $hr_source = first { $mapping{$_} == $source } keys %mapping;
    my $hr_destination = first { $groups{$_} == $destination } keys %groups;
    my %pkt = (
        timestamp => time(),
        destination => ($hr_destination || $destination),
        source => ($hr_source || $source),
        payload => substr($readbuffer, 11)
    );
    #say "destination: $destination, source: $source, length_lo: $length_lo";
    #say Dumper(\%pkt);
    $readbuffer = '';

    for my $id (keys %conns) {
        next unless $destination == $conns{$id}->{group};
        $conns{$id}->{handle}->push_write(encode_json(\%pkt) . "\r\n");
    }

    if ($pkt{payload} =~ /^STAT/) {
        say "Tuerstatus: $pkt{payload}";

        if ($pkt{payload} =~ /^STAT open/) {
            '1' > io('/tmp/laststat');
        } elsif ($pkt{payload} =~ /^STAT lock/) {
            '0' > io('/tmp/laststat');
		} else {
            '?' > io('/tmp/laststat');
		}
    }
}

say 'yo';
AnyEvent->condvar->recv
