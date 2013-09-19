#!/usr/bin/env ruby

def die(*args)
  fmt = args.shift
  $stderr.printf("fatal: %s\n" % fmt, *args)
  exit 128
end

def sha1_to_hex(sha1)
  sha1.unpack('H*').first
end

class CommandError < RuntimeError

  def initialize(command)
     @command = command
  end

  def to_s
    Array(@command).join(' ').inspect
  end

end

def run(cmd, *args)
  system(*cmd, *args)
  raise CommandError.new(cmd) unless $?.success?
end
