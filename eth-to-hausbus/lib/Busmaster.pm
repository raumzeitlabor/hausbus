# vim:ts=4:sw=4:expandtab
package Busmaster;

use Moose;
use MooseX::Singleton;
use AnyEvent;
use AnyEvent::Socket;
use AnyEvent::Handle;
use Tatsumaki::MessageQueue;
use IO::All;
use Data::Dumper;
use Hausbus;
use DateTime;
use v5.10;

# XXX: 6 r a fd1a:56e6:97e9:0:b5:ff:fe00:00/64 dev eth5.hausbus


# TODO: proper reconnect/retry for the tcp conn

has '_handle' => (isa => 'AnyEvent::Handle', is => 'rw');
# XXX: global variables, ugly
my $readbuffer = '';

sub BUILD {
    my ($self) = @_;

    say "init";
    #my $t;
    #$t = AE::timer 1, 10, sub {
    #    scalar $t;
    #    say "busmaster still alive and kicking";

    #    my $mq = Tatsumaki::MessageQueue->instance('pinpad');
    #    $mq->publish({ source => 'pinpad', payload => 'STAT lock' });
    #};

my $bus = Hausbus->new(groups => [0, 1, 50], interface => 'eth5.hausbus');
$bus->on_read(sub {
    my ($sender, $group, $data) = @_;
    my $dt = DateTime->now(time_zone => 'Europe/Berlin')->strftime('%F %H:%M:%S');
    say "group = $group, dt = $dt, sender = $sender, data = $data";
    my $destination = "$group";

    my %pkt = (
        destination => $destination,
        source => $sender,
        payload => $data,
    );
    say "destination: $destination, source: $sender";
    say Dumper(\%pkt);

    if ($destination == '50') {
	    my $mq = Tatsumaki::MessageQueue->instance('pinpad');
	    $mq->publish(\%pkt);
    }

});

}


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
    my %pkt = (
        destination => $destination,
        source => $source,
        payload => substr($readbuffer, 11)
    );
    say "destination: $destination, source: $source, length_lo: $length_lo";
    say Dumper(\%pkt);
    $readbuffer = '';

    if ($destination eq '50') {
	    my $mq = Tatsumaki::MessageQueue->instance('pinpad');
	    $mq->publish(\%pkt);
    }


    # XXX: this does not belong here, but well…
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


=head2 send($target, $payload)

Sends a message to the specified C<$target> on the bus with payload C<$payload>.

=cut
sub send {
    my ($self, $target, $payload) = @_;

    say "should send $payload to $target";
    my $destination = 1;

    my $bus = Hausbus->new(groups => [], interface => 'eth5.hausbus');
    $bus->send($destination, $payload);

    #my $content_length = length($payload);
    #my $pkt = sprintf('^%02d%02d%s$', $destination, $content_length, $payload);
    #say "pkt = $pkt";
    ## TODO: check for tcp_conn
    #my $hdl = $self->_handle;
    #$hdl->push_write($pkt);

}

sub is_valid_target {
    my ($self, $target) = @_;
    return ($target eq 'pinpad');
}

#__PACKAGE__->meta->make_immutable;

1
