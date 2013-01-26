# vim:ts=4:sw=4:expandtab
package Hausbus;

use Moose;
use IO::Socket::Multicast6;
use IO::Socket::INET6;
use AnyEvent::Socket;
use AnyEvent::Handle;
use Socket qw(:all);
use Socket6 qw(inet_ntop unpack_sockaddr_in6);
use v5.10;

has 'on_read'  => (
    is         => 'rw',
    isa        => 'CodeRef',
    predicate  => 'has_read_cb',
);

has '_groups'  => (
    is         => 'ro',
    isa        => 'ArrayRef[Int]',
    init_arg   => 'groups',
    required   => 1,
    traits     => [ 'Array' ],
    handles    => {
        groups => 'elements',
    }
);

has 'interface' => (
    is          => 'ro',
    isa         => 'Str',
    required    => 1,
);

my @watchers;

sub BUILD {
    my $self = shift;

    my $interface = $self->interface;
    my @groups = $self->groups;

    print "build\n";

    for my $group (@groups) {
        my $address = 'ff05::b5:' . sprintf("%x", $group);
    print "joining $address on $interface\n";
        my $socket = IO::Socket::Multicast6->new(
            Domain => AF_INET6,
            LocalAddr => $address,
            LocalPort => 41999,
        );

        $socket->mcast_add($address, $interface);

        my $io;
        $io = AnyEvent->io(
            fh => $socket,
            poll => "r",
            cb => sub {
                return unless $self->has_read_cb;

                my $buffer;
                my $sender = $socket->recv($buffer, 256);
                my ($port, $packed_addr) = unpack_sockaddr_in6($sender);
                $sender = inet_ntop(AF_INET6, $packed_addr);
                $sender =~ s/^fd1a:56e6:97e9:0:b5:ff:fe00://g;

                $self->on_read->($sender, $group, $buffer);
            }
        );

        push @watchers, $io;
    }
}

sub send {
  my ($self, $target, $data) = @_;

  # TODO: target validation

  my $sock = IO::Socket::INET6->new(
    Domain => AF_INET6,
    PeerAddr => 'fd1a:56e6:97e9:0:b5:ff:fe00:' . $target,
    PeerPort => 41999,
    Proto => 'udp'
  );

  $sock->send($data);
}

1
