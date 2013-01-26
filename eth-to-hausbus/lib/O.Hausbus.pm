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

has '_socket' => (is => 'rw', isa => 'IO::Socket::Multicast6');
has '_io' => (is => 'rw', isa => 'Ref');
has 'on_read' => (is => 'rw', isa => 'CodeRef');
has 'group' => (is => 'ro', isa => 'Int', required => 1);

sub BUILD {
  my $self = shift;

  my $address = 'ff05::b5:' . sprintf("%x", $self->group);
  my $s = IO::Socket::Multicast6->new(
      Domain => AF_INET6,
      LocalAddr => $address,
      LocalPort => 41999,
  );

  $s->mcast_add($address, 'eth5');

  $self->_socket($s);

  my $io;
  $io = AnyEvent->io(
        fh => $s,
        poll => "r",
        cb => sub {
        if (!defined($self->on_read)) {
          return;
        }

        my $buffer;
        my $sender = $s->recv($buffer, 256);
        my ($port, $packed_addr) = unpack_sockaddr_in6($sender);
        $sender = inet_ntop(AF_INET6, $packed_addr);
        $sender =~ s/^fd1a:56e6:97e9:0:b5:ff:fe00://g;

        $self->on_read->($sender, $buffer);
      });
  $self->_io($io);
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
