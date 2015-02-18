#
# $Id: test.tcl,v 1.2 2003/07/29 18:13:37 xuanc Exp $
# 
# Copyright 1993 Massachusetts Institute of Technology
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation, and that the name of M.I.T. not be used in advertising or
# publicity pertaining to distribution of the software without specific,
# written prior permission.  M.I.T. makes no representations about the
# suitability of this software for any purpose.  It is provided "as is"
# without express or implied warranty.
#
#


#
# a meta-class for test objects, and a class for test suites
#

Class TestClass -superclass Class

Class TestSuite

#
# check basic argument dispatch and unknown
#

TestSuite objectdispatch

objectdispatch proc run {{n 50}} {
  Object adispatch

  adispatch proc unknown {m args} {eval list [list $m] $args}
  adispatch proc cycle {l n args} {
    if {$l>=$n} then {return ok}
    set i [llength $args]
    foreach a $args {
      if {$a != $i} then {
	error "wrong order in arguments: $l $n $args"
      }
      incr i -1
    }
    incr l
    
    set ukn [eval [list $self] $args]
    if {$ukn != $args} then {
      error "wrong order in unknown: $ukns"
    }
    eval [list $self] [list $proc] [list $l] [list $n] [list $l] $args
  }

  if {[catch {adispatch cycle 1 $n 1} msg]} then {
    error "FAILED $self: cycle: $msg"
  }

  return "PASSED $self"
}

#
# examples from the workshop paper
#

TestSuite paperexamples

paperexamples proc example1 {} {
  Object astack
  
  astack set things {}
  
  astack proc put {thing} {
    $self instvar things
    set things [concat [list $thing] $things]
    return $thing
  }
  
  astack proc get {} {
    $self instvar things
    set top [lindex $things 0]
    set things [lrange $things 1 end]
    return $top
  }
  
  astack put bagel
  astack get
  astack destroy
}

paperexamples proc example2 {} {
  Class Safety
  
  Safety instproc init {} {
    $self next
    $self set count 0
  }
  
  Safety instproc put {thing} {
    $self instvar count
    incr count
    $self next $thing
  }
  
  Safety instproc get {} {
    $self instvar count
    if {$count == 0} then { return {empty!} }
    incr count -1
    $self next
  }
  
  Class Stack
  
  Stack instproc init {} {
    $self next
    $self set things {}
  }
  
  Stack instproc put {thing} {
    $self instvar things
    set things [concat [list $thing] $things]
    return $thing
  }
  
  Stack instproc get {} {
    $self instvar things
    set top [lindex $things 0]
    set things [lrange $things 1 end]
    return $top
  }
  
  Class SafeStack -superclass {Safety Stack}
  
  SafeStack s
  s put bagel
  s get
  s get
  s destroy

  SafeStack destroy
  Stack destroy
  Safety destroy
}

paperexamples proc run {} {
  set msg {}

  if {[catch {$self example1; $self example2} msg] == "0"} then {
    return "PASSED $self"
  } else {
    error "FAILED $self: $msg"
  }
}


#
# create a graph of classes
#

TestSuite classcreate

classcreate proc factorgraph {{n 3600}} {
  TestClass $n

  for {set i [expr {$n/2}]} {$i>1} {incr i -1} {
    if {($n % $i) == 0} then {
      
      #
      # factors become subclasses, direct or indirect
      #

      if {[TestClass info instances $i] == {}} then {
	$self factorgraph $i
	$i superclass $n
      } elseif {[$i info superclass $n] == 0} then {
	$i superclass [concat [$i info superclass] $n]
      }
    }
  }
}

classcreate proc run {} {
  set msg {}
  if {[catch {$self factorgraph} msg] == "0"} then {
    return "PASSED $self"
  } else {
    error "FAILED $self: $msg"
  }
}


#
# lookup superclasses and combine inherited methods
#

TestSuite inheritance

inheritance proc meshes {s l} {
  set p -1
  foreach j $s {
    set n [lsearch -exact $l $j]
    if {$n == -1} then {
      error "FAILED $self - missing superclass"
    }
    if {$n <= $p} then {
      error "FAILED $self - misordered heritage: $s : $l"
    }
    set p $n
  }
}

inheritance proc superclass {} {
  foreach i [TestClass info instances] {
    set s [$i info superclass]
    set h [$i info heritage]
    
    #
    # superclasses should mesh with heritage
    #
    
    $self meshes $s $h
  }
}

inheritance proc combination {} {
  foreach i [TestClass info instances] {
    
    #
    # combination should mesh with heritage
    #
    
    $i anumber
    set obj [lrange [anumber combineforobj] 1 end]
    set h [$i info heritage]
    $self meshes $obj $h
    anumber destroy
    
    if {[$i info procs combineforclass] != {}} then {
      set cls [lrange [$i combineforclass] 1 end]
      $self meshes $cls $h
    }
  }
}

inheritance proc run {} {

  #
  # add combine methods to "random" half of the graph
  #

  set t [TestClass info instances]
  for {set i 0} {$i < [llength $t]} {incr i 2} {
    set o [lindex $t $i]
    $o instproc combineforobj {} {
      return [concat [list $class] [$self next]]
    }
    $o proc combineforclass {} {
      return [concat [list $class] [$self next]]
    }
  }
  
  #
  # and to Object as a fallback
  #

  Object instproc combineforobj {} {
    return [concat [list $class] [$self next]]
  }
  Object proc combineforclass {} {
    return [concat [list $class] [$self next]]
  }

  $self superclass
  $self combination

  return "PASSED $self"
}


#
# destroy graph of classes
#

TestSuite classdestroy

classdestroy proc run {} {
  
  #
  # remove half of the graph at a time
  #

  TestClass instproc destroy {} {
    global TCdestroy
    set TCdestroy $self
    $self next
  }

  while {[TestClass info instances] != {}} {
    set t [TestClass info instances]

    for {set i 0} {$i < [llength $t]} {incr i} {
      set o [lindex $t $i]

      #
      # quarter dies directly, quarter indirectly, quarter renamed
      #
    
      if {($i % 2) == 0} then {
	global TCdestroy
	set sb [$o info subclass]

	if {[info tclversion] >= 7.4 && ($i % 4) == 0} then {
	  rename $o {}
	} else {
	  $o destroy
	}
	if {[catch {set TCdestroy}] || $TCdestroy != $o} then {
	  error "FAILED $self - destroy instproc not run for $o"
	}
	if {[info commands $o] != {}} then {
	  error "FAILED $self - $o not removed from interpreter"
	}
	unset TCdestroy

	#
	# but everyone must still have a superclass
	#
	
	foreach j $sb {
	  if {[$j info superclass] == {}} then {
	    $j superclass Object
	  }
	}
      } elseif {[info tclversion] >= 7.4 && ($i % 3) == 0} then {
        rename $o $o.$i
      }

    }
    
    inheritance superclass
    inheritance combination
  }

  return "PASSED $self"
}


TestSuite objectinits

objectinits proc prepare {n} {

  #
  # head of a chain of classes that do add inits
  #

  TestClass 0
  0 instproc init {args} {
    eval $self next $args
    $self set order {}
  }

  #
  # and the rest
  #

  for {set i 1} {$i < $n} {incr i} {
    TestClass $i -superclass [expr {$i-1}]
    
    #
    # record the reverse order of inits
    #

    $i instproc init {args} {
      eval $self next $args
      $self instvar order
      lappend order $class
    }

    #
    # add instproc for init options
    #

    $i instproc $i.set {val} {
      $self instvar $class
      set $class $proc.$val
    }
  }
}
  
objectinits proc run {{n 15}} {
  $self prepare $n

  set il {}
  for {set i 1} {$i < $n} {incr i} {
    lappend il $i
    set al {}
    set args {}
    for {set j $i} {$j > 0} {incr j -1} {
      lappend al $j
      lappend args -$j.set $j

      #
      # create obj of increasing class with increasing options
      #

      if {[catch {eval $i $i.$j $args} msg] != 0} then {
	error "FAILED $self - $msg"
      }

      if {[$i.$j set order] != $il} then {
	error "FAILED $self - inited order was wrong"
      }

      set vl [lsort -decreasing -integer [$i.$j info vars {[0-9]*}]]
      if {$vl != $al} then {
	error "FAILED $self - wrong instvar names: $vl : $al"
      }
      foreach k $vl {
	set val $k.set.$k
	if {[$i.$j set $k] != $val} then {
	  error "FAILED $self - wrong instvar values"
	}
      }
    }
  }

  return "PASSED $self"
}


TestSuite objectvariables

objectvariables proc run {{n 100}} {
  TestClass Variables
  Variables avar

  foreach obj {avar Variables TestClass Class Object} {
    
    #
    # set up some variables
    #
    
    $obj set scalar 0
    $obj set array() {}
    $obj unset array()
    $obj set unset.$n {}
    
    #
    # mess with them recursively
    #
    
    $obj proc recurse {n} {
      $self instvar scalar array
      incr scalar
      set array($n) $n
      $self instvar unset.$n
      unset unset.$n
      incr n -1
      $self instvar unset.$n
      set unset.$n [array names array]
      if {$n > 0} then { 
	$self recurse $n
      }
    }
    
    
    $obj recurse $n

    #
    # check the result and clean up
    #
    
    if {[$obj set scalar] != $n} then {
      error "FAILED $self - scalar"
    }
    $obj unset scalar

    for {set i $n} {$i > 0} {incr i -1} {
      if {[$obj set array($i)] != $i} then {
	error "FAILED $self - array"
      }
    }    
    $obj unset array

    if {[$obj info vars] != "unset.0"} then {
      error "FAILED $self - unset: [$obj info vars]"
    }
  }

  #
  # trace variables
  #

  Variables avar2

  avar2 proc trace {var ops} {
    $self instvar $var
    trace variable $var $ops "avar2 traceproc"
  }

  avar2 proc traceproc {maj min op} {
    global trail; lappend trail [list $maj $min $op]
  }

  global guide trail
  avar2 trace array wu
  for {set i 0} {$i < $n} {incr i} {
    avar2 trace scalar$i wu
    avar2 set scalar$i $i
    lappend guide [list scalar$i {} w]
    avar2 set array($i) [avar2 set scalar$i]
    lappend guide [list array $i w]
  }
  if {$guide != $trail} then {
    error "FAILED $self - trace: expected $guide, got $trail"
  }

  #
  # destroy must trigger unset traces
  #

  set trail {}
  set guide {}
  lappend guide [list array {} u]
  for {set i 0} {$i < $n} {incr i} {
    lappend guide [list scalar$i {} u]
  }

  avar2 destroy
  if {[lsort $guide] != [lsort $trail]} then {
    error "FAILED $self - trace: expected $guide, got $trail"
  }

  Variables destroy
  
  return "PASSED $self"
}


#
# c api, if compiled with -DTESTCAPI
#

TestSuite capi

capi proc run {{n 50}} {
  set start [dawnoftime read]
  for {set i 0} {$i < $n} {incr i} {
    Timer atime$i
    if {$i % 3} {atime$i stop}
    if {$i % 7} {atime$i read}
    if {$i % 2} {atime$i start}
    if {$i % 5} {atime$i stop}
  }
  set end [dawnoftime read]
  if {$end < $start} {
    error "FAILED $self: timer doesn't work"
  }

  foreach i [Timer info instances] {$i destroy}
  Timer destroy

  return "PASSED $self"
}


#
# high and low level autoload
#

TestSuite autoload

autoload proc atest {} {
}

autoload proc run {{n 10}} {
  global auto_path

  foreach i [glob -nocomplain tmpld*.tcl] {exec rm -f $i}
  set prev Object
  for {set i 0} {$i <= $n} {incr i} {
    set fid [open "tmpld$i.tcl" w]
    puts $fid "Class AutoTest$i -superclass $prev"
    puts $fid "AutoTest0 instproc $i {args} {return 1}"
    set prev AutoTest$i
    close $fid
  }

  catch {exec mv -f tclIndex tclIndex.saved}
  otcl_mkindex Class . tmpld*.tcl
  lappend auto_path .
  auto_reset

  # why use AutoTest5?
  # fine for 0, but not others
  # if enable print out in otcl.c, seg fault
  # xuanc, 7/29/03
  set m [expr {$n/2}]
  if {[catch {AutoTest$m atest} msg]} then {
    error "FAILED $self - $msg"
  }

  for {set i $n} {$i > $m} {incr i -1} {
    if {[AutoTest0 info instprocs $i] == {}} then {
      error "FAILED $self - missing loader stub"
    }
    if {![catch {AutoTest0 info instbody $i}]} then {
      error "FAILED $self - premature load"
    }
  }

  for {set i 0} {$i <= $m} {incr i} {
    if {[AutoTest0 info instprocs $i] == {}} then {
      error "FAILED $self - missing instproc"
    }
    if {[catch {AutoTest0 info instbody 0}]} then {
      error "FAILED $self - failed load"
    }
  }

  # why 0-10? AutoTest5 can only load procs 0-5
  # hangs when i = 6
  # need to fix, xuanc, 7/29/2003
  for {set i 0} {$i <= $n} {incr i} {
    if {![atest $i]} then {
      error "FAILED $self - wrong proc result"
    }
  }   
  puts "after atest"
  
  exec rm -f tclIndex
  foreach i [glob -nocomplain tmpld*.tcl] {exec rm -f $i}
  catch {exec mv -f tclIndex.saved tclIndex}

  return "PASSED $self"
}


TestSuite proc run {} {
  
  #
  # run individual tests in needed order
  #

  puts [objectdispatch run]
  puts [paperexamples run]
  puts [classcreate run]
  puts [inheritance run]
  puts [classdestroy run]
  puts [objectinits run]
  puts [objectvariables run]
  if {[info commands Timer] != {}} then {
    puts [capi run]
  }
  # autoload hangs---xuanc, 7/29/03
  puts [autoload run]
}

TestSuite run

exit

# Local Variables:
# mode: tcl
# tcl-indent-level: 2
# End:
