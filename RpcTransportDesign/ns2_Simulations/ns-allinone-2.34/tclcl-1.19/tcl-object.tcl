#
# Copyright (c) 1996 Regents of the University of California.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
# 	This product includes software developed by the MASH Research
# 	Group at the University of California Berkeley.
# 4. Neither the name of the University nor of the Research Group may be
#    used to endorse or promote products derived from this software without
#    specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $Header: /cvsroot/otcl-tclcl/tclcl/tcl-object.tcl,v 1.41 2000/07/28 23:15:08 johnh Exp $
#

#
# InitObject is an OTcl object that exports "init-vars" to initialize
# default variables, and mimic Object's args processing.
#
Class InitObject

#
# init-vars: calls init-default-vars to initilize all default variables
#   then process args the way Object::init does, and return unused args
#
#  This is how it should be used from "init" instproc of split objects
#	set args [eval $self init-vars $args]
#	eval $self next $args
#
Object instproc init-vars {args} {
	$self init-default-vars [$self info class]

	# Emulate Object's args processing
	#  1.  Look for pairs of {-cmd val} in args
	#  2.  If "$self $cmd $val" is not valid then put it in $shadow_args
	set shadow_args ""
	for {} {$args != ""} {set args [lrange $args 2 end]} {
		set key [lindex $args 0]
		set val [lindex $args 1]
		if {$val != "" && [string match {-[A-z]*} $key]} {
			set cmd [string range $key 1 end]
			if ![catch "$self $cmd $val"] {
				continue
			}
		}
		lappend shadow_args $key $val
	}
	return $shadow_args
}

#
# init-all-vars:  initializes all default variables for an object
#
Object instproc init-default-vars {classes} {
	foreach cl $classes {
		if {$cl == "Object"} continue
		# depth first: set vars of ancestors first
		$self init-default-vars "[$cl info superclass]"
		foreach var [$cl info vars] {
			if [catch "$self set $var"] {
				$self set $var [$cl set $var]
			}
		}
	}
}


#
# A SplitObject is an OTcl object with a C++ shadow object, i.e., an object
# whose implementation is split across OTcl and C++.  This is the base
# class of all such objects.
#

Class SplitObject
SplitObject set id 0

SplitObject instproc init args {
	$self next
	if [catch "$self create-shadow $args"] {
		error "__FAILED_SHADOW_OBJECT_" ""
	}
}

#
# reimplement set and get in terms of instvar
# to handle classinstvars
#
SplitObject instproc set args {
	set var [lindex $args 0]
	$self instvar -parse-part1 $var
	if {[llength $args] == 1} {
		return [subst $[subst $var]]
	} else {
		return [set $var [lindex $args 1]]
	}
}

SplitObject instproc destroy {} {
	$self delete-shadow
	$self next
}

SplitObject proc getid {} {
	$self instvar id
	incr id
	return _o$id
}

SplitObject proc is-class cl {
	if [catch "SplitObject info subclass $cl" v] {
		return 0
	}
	return $v
}

SplitObject instproc unknown args {
	if [catch "$self cmd $args" ret] {
		set cls [$self info class]
		global errorInfo
		set savedInfo $errorInfo
		error "error when calling class $cls: $args" $savedInfo
	}
	return $ret
}

proc new { className args } {
	set o [SplitObject getid]
	if [catch "$className create $o $args" msg] {
		if [string match "__FAILED_SHADOW_OBJECT_" $msg] {
			#
			# The shadow object failed to be allocated.
			# 
			delete $o
			return ""
		}
		global errorInfo
		error "class $className: constructor failed: $msg" $errorInfo
	}
	return $o
}

proc delete o {
	$o delete_tkvar
	$o destroy
}

#
# Register a C++ compiled-in class with OTcl
# Arrange things so we catch create and destroy
# and manage the underlying C++ object accordingly.
# Classes have structured names with the hierarchy
# delineated by slashes, e.g., a side effect of
# creating class "A/B/C" is to create classes "A"
# and "A/B".	
# This routine invoked by TclClass::bind.
#
SplitObject proc register className {
	set classes [split $className /]
	set parent SplitObject
	set path ""
	set sep ""
	foreach cl $classes {
		set path $path$sep$cl
		if ![$self is-class $path] {
			Class $path -superclass $parent
		}
		set sep /
		set parent $path
	}
}

#
# warn if a class variable not defined
# this is in a separate method so user can nop it
#
# In ns, this error happens for several possible reasons:
#
#	1. you bound a variable in C but didn't initialize it in tcl
#		To fix:  put initialization code in tcl/lib/ns-default.tcl
#		(and make sure that this code ends up compiled into your
#		version of ns!)
#
#	2. You bound it in C and think you initialized it in Tcl,
#		but there's an error in your class hierarchy
#		(for example, the Tcl hierarchy name given in
#		the XxxClass declaration in C++
#		doesn't match the name used in the Tcl initialization).
#
#	3. you invoked something which assumed that something else had
#		been built (for example, doing "new Node" without having
#		first done "new Simulator")
#		To fix:  do new Simulator (or whatever).
#
#	4. You created a split object from C++ (with new)
#		rather than from Tcl.
#		Nitin Vaidya <Nitin.Vaidya@Eng.Sun.COM> found this problem
#		and suggested working around it either by avoiding
#		binding the variable or invoking tcl to create the object.
#		See the discussion at
#		http://www-mash.cs.berkeley.edu/dist/archive/ns-users/9808/
#		for more details.
#
SplitObject instproc warn-instvar item {
	$self instvar issuedWarning
	if ![info exists issuedWarning($item)] {
		set issuedWarning($item) 1
		puts stderr "warning: no class variable $item\n"
		$self instvar SplitObject_issued_undeclared_warning
		if ![info exists SplitObject_issued_undeclared_warning] {
			puts stderr "\tsee tcl-object.tcl in tclcl for info about this warning.\n"
			# (i.e., see the comment 20 lines back)
			set SplitObject_issued_undeclared_warning 1
		}
	}
}

#
# Initialize a class instance variable from its
# parent class' member value.  If no such variable,
# call the warning method (which can be overridden).
# We search the class hierarchy for the variable
# until we give up at the SplitObject class.
#
SplitObject instproc init-instvar var {
	set cl [$self info class]
	while { "$cl" != "" && "$cl" != "SplitObject" } {
		foreach c $cl {
			if ![catch "$c set $var" val] {
				$self set $var $val
				return
			}
		}
		set parents ""
		foreach c $cl {
			if { $cl != "SplitObject" && $cl != "Object" } {
				set parents "$parents [$c info superclass]"
			}
		}
		set cl $parents
	}
	$self warn-instvar [$self info class]::$var
}

proc tkerror msg {
	global errorInfo
	puts -nonewline "$msg: "
	puts $errorInfo
	exit 1
}

# somehow tkerror is *evaluated* as 'bgerror', and put 'tkerror' in
# bgerror's body will cause an infinite execution loop... :(
proc bgerror msg {
	global errorInfo
	puts -nonewline "$msg: "
	puts $errorInfo
	exit 1
}

#
# Method hooks to allow using public/private keywords
# instead of instproc to define methods.  For now, we
# don't actually enforce these semantics.  Rather, they
# serve only to document the API (and as a hook for
# the automatic doc generator).
#
Object instproc public args {
	eval $self instproc $args
}

Object instproc private args {
	eval $self instproc $args
}

Object instproc proc.public args {
	eval $self proc $args
}

Object instproc proc.private args {
	eval $self proc $args
}

Object instproc proc.invoke { arglist body args } {
	$self proc invokeproc_ $arglist $body
	eval [list $self] invokeproc_ $args
}

#tkvar stuff


Object instproc tkvar args {
	foreach var $args {
		if { [llength $var] > 1 } {
			set varname [lindex $var 1]
			set var [lindex $var 0]
		} else {
			set varname $var
		}
		uplevel upvar #0 $self/$var $varname
	}
}

Object instproc tkvarname var {
	return $self/$var
}

Object instproc delete_tkvar { } {
	set fullname [$self tkvarname foo]
	regexp "(.*)foo$" $fullname dummy prefix
	foreach global [info globals "$prefix*"] {
		global $global
		unset $global
	}
}

Object instproc info_tkvar { pattern } {
	set pattern [$self tkvarname $pattern]
	return [info globals $pattern]
}

#
# Backward compat: SplitObjects used to be called TclObjects
#
proc TclObject args {
	return [eval SplitObject $args]
}

# Misc. helpers
#
# Procedure to order a list of SplitObject's.
#
# use as: "lsort -command SplitObjectCompare {list}"
#
proc SplitObjectCompare {a b} {
	set o1 [string range $a 2 end]
	set o2 [string range $b 2 end]
	if {$o1 < $o2} {
		return -1
	} elseif {$o1 == $o2} {
		return 0
	} else {
		return 1
	}
}

Object instproc extract-var varname {
        set aidx [string first "(" $varname]
        if { $aidx >= 0 } {
                string range $varname 0 [incr aidx -1]
        } else {
                set varname
        }
}

Object instproc add-to-list {list elem} {
	$self instvar [$self extract-var $list]
	set ret 0
	if ![info exists $list] {
		set $list $elem
		set ret 1
	} elseif { [lsearch [set $list] $elem] < 0 } {
               	lappend $list $elem
		set ret 1
        }
	set ret
}

Object instproc remove-from-list {list elem} {
	$self instvar [$self extract-var $list]
	set wtag "$self: remove $elem from $list failed"
	set ret  0
	if ![info exists $list] {
		warn "$wtag: list does not exist"
	} else {
		set k [lsearch [set $list] $elem]
		if { $k < 0 } {
			warn "$wtag: element does not exist"
		} else {
			set $list [lreplace [set $list] $k $k]
			set ret 1
		}
	}
	set ret
}
