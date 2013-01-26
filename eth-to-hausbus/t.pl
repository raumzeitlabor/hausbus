#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use Plack::Runner;
use Tatsumaki::Application;
use lib qw(lib);
use Busmaster;
use Busmaster::Handler::Group;
use Busmaster::Handler::Send;
use v5.10;

# hotfix, just in case the route is not properly set up in /etc/network/interfaces
system('ip -6 r a fd1a:56e6:97e9:0:b5:ff:fe00:00/64 dev eth5');

print "before busmaster\n";
my $master = Busmaster->new;
print "after busmaster\n";

my $r = Plack::Runner->new;
$r->parse_options('--port', '8888');
my $app = Tatsumaki::Application->new([
    '/group/(\w+)' => 'Busmaster::Handler::Group',
    '/send/(\w+)' => 'Busmaster::Handler::Send',
]);

$r->run($app->psgi_app);
