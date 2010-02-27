#!/bin/sh
# Tcl ignores the next line -*- tcl -*- \
exec tclsh "$0" "$@"

set request [join $argv]
if {![string match {review://*} $request]} {
	# we do not handle this request
	exit 0
}
# trim trailing slashes (which Windows seems to add)
set request [string trimright $request "/"]

set branch [list [string range $request 9 end]]

if {[catch {set sock [socket localhost 12345]} err]} {
	puts "$err\n"
	puts "Failed to connect to git gui. Please start one instance."
	gets stdin
	exit 1
}
puts $sock $branch
close $sock
