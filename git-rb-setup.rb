#!/usr/bin/env ruby

def die(*args)
  fmt = args.shift
  $stderr.printf("fatal: %s\n" % fmt, *args)
  exit 128
end

def sha1_to_hex(sha1)
  sha1.unpack('H*').first
end
