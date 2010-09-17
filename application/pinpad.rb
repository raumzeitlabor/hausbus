#!/usr/bin/ruby1.9.1

require 'bus'

bus = Hausbus.new
bus.set_interface "eth0"
bus.join_group 50, do |msg, sender|
  puts "Status: #{msg}"
  exit 0
end

puts "Requesting pinpad status..."
bus.send 1, "get_status"

bus.run
