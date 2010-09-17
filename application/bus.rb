##
## RaumZeitLabor-Hausbus
## © 2010 Michael Stapelberg
##
require 'socket'
require 'ipaddr'
require 'eventmachine'

class HausbusClient < EventMachine::Connection
  def notify_readable
    msg, info = @io.recvfrom(1500)
    # remove static part of the link-local address
    sender = info[3].gsub(/fe80::b5:ff:fe00:/, "")
    # remove the scope identifier from link-local address
    sender = sender.gsub(/%([^%]+)$/, "")

    # call the registered callback
    @block.call msg, sender
  end

  def set_block(block)
    @block = block
  end
end

class Hausbus
  def initialize()
    @sockets = []
    @index = nil
    @interface = nil
  end

  def set_interface(name)
    @interface = name

    ## relatively ugly workaround for the fact that you cannot currently find
    ## out the interface index for a given interface name (see ruby-dev:37765)
    File.open("/proc/net/igmp6").each do |line|
      index, interface = line.match('^([0-9]+)\s+([^\s]+)')[1,2]
      if interface == name
        @index = index.to_i
        return
      end
    end
  end

  ##
  ## num is a number between 50 and 100 (inclusively) which identifies a
  ## specific group to receive packets.
  ##
  def join_group(num, &block)
    if !@index
      throw "You need to call set_interface first"
    end
    sock = UDPSocket.new(Socket::AF_INET6)
    optval = IPAddr.new("[ff02::b5:#{num.to_s(16)}]").hton + [@index].pack('i')
    sock.setsockopt(Socket::IPPROTO_IPV6, Socket::IPV6_JOIN_GROUP, optval)
    sock.bind("::", 41999)
    @sockets += [ { "fd" => sock, "block" => block } ]
  end

  ##
  ## sends the given msg to the controller with the given num (between 1
  ## and 29 inclusively)
  ##
  def send(num, msg)
    sock = UDPSocket.new(Socket::AF_INET6)
    sock.connect("fe80::b5:ff:fe00:#{num.to_s(16)}%#{@interface}", 41999)
    sock.send msg, 0
    sock.close
  end

  def run
    EM.run {
      for sock in @sockets
        conn = EM.watch sock["fd"], HausbusClient
        conn.notify_readable = true
        conn.set_block sock["block"]
      end
    }
  end
end
