# vim:ts=4:sw=4:expandtab
package Busmaster::Handler::Group;

use strict;
use parent qw(Tatsumaki::Handler);
use Tatsumaki::MessageQueue;
use Busmaster;
use JSON::XS;
use v5.10;
__PACKAGE__->asynchronous(1);

sub get {
    my ($self, $group) = @_;

    my $busmaster = Busmaster->new;
    if (!$busmaster->is_valid_target($group)) {
        Tatsumaki::Error::HTTP->throw(404);
    }

    $self->response->content_type('application/json');

    my $client_id = rand(1);
    my $mq = Tatsumaki::MessageQueue->instance($group);
    $mq->poll($client_id, sub {
        my @events = @_;
        for my $event (@events) {
            $self->stream_write(JSON::XS->new->encode($event) . "\r\n");
        }
    });

    # start keepalive timer
    my $t;
    $t = AE::timer 60, 60, sub {
        scalar $t;
        $self->stream_write("\r\n");
    };
};

1
