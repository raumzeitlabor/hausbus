#!/usr/bin/ruby1.9.1

require 'bus'

bus = Hausbus.new
bus.set_interface "eth0"
bus.join_group 38, do |msg, sender|
  puts "MSG from #{sender} len #{msg.size}" 
  for c in 0..msg.size-1
    puts "c #{c} = #{msg[c]}, #{msg[c].ord.to_s(16)}"
  end
end
bus.run
