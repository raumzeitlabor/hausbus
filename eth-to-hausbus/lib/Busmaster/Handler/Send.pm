# vim:ts=4:sw=4:expandtab
package Busmaster::Handler::Send;

use strict;
use parent qw(Tatsumaki::Handler);
use JSON::XS;
use Busmaster;
__PACKAGE__->asynchronous(1);

$Tatsumaki::MessageQueue::BacklogLength = 0;

sub post {
    my ($self, $group) = @_;
    my $busmaster = Busmaster->new;

    if (!$busmaster->is_valid_target($group)) {
        Tatsumaki::Error::HTTP->throw(404);
    }

    my $json = $self->request->content;
    if ($json) {
        my $msg = decode_json($json);
        $busmaster->send($group, $msg->{payload});
        $self->write({ status => 'ok' });
        $self->finish;
    } else {
        Tatsumaki::Error::HTTP->throw(400);
    }
};

sub put {
	my ($self, $group) = @_;
	return post($self, $group);
}

1
