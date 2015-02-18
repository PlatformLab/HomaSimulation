#!/usr/local/bin/tclsh7.6

set filename [lindex $argv 0]

if {$filename==""} {
    puts stderr "Usage: fix-script filename"
}

set file [open $filename "r"]

while {[eof $file]==0} {
    set line [gets $file]
    set op [lindex $line 0]
    if {$op!="v"} {
	set time [lindex $line 1]
	set src [lindex $line 2]
	set dst [lindex $line 3]
	set size [lindex $line 4]
	set attr [lindex $line 5]
	set ptype [lindex $line 6]
	set conv [lindex $line 7]
	set id [lindex $line 8]
	puts "$op -t$time -e$size -s$src -d$dst -a$attr -c$conv -p$ptype -i$id"
    }
}
