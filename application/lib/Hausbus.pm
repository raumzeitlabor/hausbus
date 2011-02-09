# vim:ts=4:sw=4:expandtab
package Hausbus;

use Moose;
use IO::Socket::Multicast6;
use IO::Socket::INET6;
use AnyEvent::Socket;
use AnyEvent::Handle;
use v5.10;

has '_socket' => (is => 'rw', isa => 'IO::Socket::Multicast6', default => undef);
has '_handle' => (is => 'rw', isa => 'AnyEvent::Handle', default => undef);
has 'on_read' => (is => 'rw', isa => 'CodeRef', default => undef);
has 'group' => (is => 'ro', isa => 'Int', required => 1);

sub BUILD {
  my $self = shift;

  my $s = IO::Socket::Multicast6->new(
      Domain => AF_INET6,
      LocalPort => 41999
  );

  $s->mcast_add('ff05::b5:' . sprintf("%x", $self->group));

  $self->_socket($s);

  my $handle;
  $handle = AnyEvent::Handle->new(
      fh => $s,
      on_error => sub {
          say "error";
      },
      on_read => sub {
        return [] unless length($_[0]->rbuf) > 0;
        if (!defined($self->on_read)) {
          $_[0]->rbuf = "";
          return;
        }

        # TODO: find out sender
        $self->on_read->(undef, $_[0]->rbuf);
        $_[0]->rbuf = '';
      });
  $self->_handle($handle);
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
