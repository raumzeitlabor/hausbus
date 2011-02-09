#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use AnyEvent;
use v5.10;
use lib qw(lib);
use Hausbus;
use DateTime;

my $bus = Hausbus->new(group => 50);

$bus->on_read(sub {
    my ($sender, $data) = @_;
    my $dt = DateTime->now(time_zone => 'Europe/Berlin')->strftime('%F %H:%M:%S');
    if ($data =~ /^SRAW/) {
        my ($s1, $s2, $s3) = unpack("%C%C%C", substr($data, 5, 3));
        say "$dt - Tür, Sensorendaten: $s1, $s2, $s3";
    }
    return if ($data =~ /^SRAW/);
    if ($data =~ /^STAT/) {
        say "$dt - Neuer Status: " . $data;
    }
});

$bus->send(1, 'status');

AnyEvent->condvar->recv
