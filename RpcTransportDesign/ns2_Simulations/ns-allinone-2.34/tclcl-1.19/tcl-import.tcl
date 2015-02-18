#
# Copyright (c) 1998 Regents of the University of California.
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
#       This product includes software developed by the Computer Systems
#       Engineering Group at Lawrence Berkeley Laboratory.
# 4. Neither the name of the University nor of the Laboratory may be used
#    to endorse or promote products derived from this software without
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
# ------------
#
# Filename: new-tcl-import.tcl
#   -- Created on Fri Jul 24 1998
#   -- Author: Yatin Chawathe <yatin@cs.berkeley.edu>
#
# Description:
#
#
#  @(#) $Header: /cvsroot/otcl-tclcl/tclcl/tcl-import.tcl,v 1.6 2002/07/12 00:25:46 tim1724 Exp $
#



#
# Support for a simple, java-like import construct.
#
Class Import


Import public init { } {
	$self next
	$self set use_http_cache_ 1
}


#
# For the objects provided as <i>args</i>, source the files in
# which their class & methods are defined.
# After attempting to import all supplied items, if any were unimportable,
# an error will be flagged with a detailed errormsg.
#
Import public import { args } {
	$self instvar import_dirs_ table_
	
	# initialize the import table only on demand	
	if { ![info exists import_dirs_] } {
		$self init_table
	}

	# ensure that the TCLCL_IMPORT_DIRS env var hasn't changed since we
	# initialized the table
	$self consistency_check


	foreach item $args {
		if [info exists table_($item)] {
			set file_list $table_($item)
			# although it's poor programming practice,
			# an object can be defined in multiple files
			foreach file $table_($item) {
				if { [set msg [$self source_file $file]]!=""} {
					error "could not source $file for\
							$item:\n$msg"
				}
			}
		} else {
			# if the object is not in the table_,
			# try searching for default name (<object>.mash)
			# in each of the env(TCLCL_IMPORT_DIRS)

			set list {}
			foreach dir $import_dirs_ {
				lappend list [$self file join $dir \
						[$self class_to_file \
						$item].mash]
			}

			set imported 0
			foreach filename $list {
				if { [$self source_file $filename] == "" } {
					set imported 1
					break
				}
			}

			if { ! $imported } {
				error "don't know how to import $item\n    not\
						mapped in: $import_dirs_\
						\n    and not found in default\
						locations: $list"
			}
		}
	}
}



#
# As long as the import procedure has not yet been invoked, the user is
# free to override mappings that may be read from importTables.
#
Import public override_importTable_mapping { object file_list } {
	$self instvar overrideTable_ import_dirs_
	
	if { [info exists import_dirs_] } {
		puts stderr "warning: ignoring \"override_importTable_mapping\
				$object $file_list\" \n\
				It is illegal to modify the internal table \
				after the first call to import."
		return
	}

	if { [info exists overrideTable_($object)] } {
		unset overrideTable_($object)
	}
	
	foreach file $file_list {
		set fname [$self condense_into_absolute_filename \
				[$self file join [pwd] $file]]
		lappend overrideTable_($object) $fname
	}
}


#
# Wait to redefine the unknown proc until we are running in the interpreter,
# because the original unknown proc is not defined until after this
# tcl-object.tcl code is processed. (I believe the original unknown proc
# is actually defined when Tcl_AppInit calls Tcl_Init to set up the
# script library facility.)
#
Import proc.private redefine_unknown {} {
	#
	# If a proc/instproc is called on an unknown class, you'll wind
	# up here. If auto-importing is enabled, attempt to import the class.
	#

	rename unknown unknown.orig
	# Rather than redefining this procedure in tcl/library/init.tcl,
	# we can rename & augment it here for the mash interpreter.
	proc unknown { args } {
		# first try tcl's original unknown proc and return if
		# successful
		if { ![catch "eval {unknown.orig} $args" m] } { 
			return
		}
		# otherwise, if autoimporting is enabled,
		# if able to import an item by this name, do so and return
		# btw, if the stuff in catch quotes causes "unknown" to
		# get called, error = "too many nested calls to
		# Tcl_EvalObj (infinite loop?)"
		$self instvar autoimport_
		if { [info exists autoimport_] && $autoimport_ } {
                        really_import [lindex $args 0]
		} else {
			# if not trying to import, puts original error msg
			error "$m" 
		}
	}

	# prevent this method from being called again
	Import proc.private redefine_unknown {} {}
}

#
# As new objects are needed, the unknown proc will catch them and
# import the files that define them and their methods.
# As an intended side-effect, explicit imports will be ignored
# until auto-import is disabled.
#
Import proc.public enable_autoimport {} {
	# XXX this should be done somewhere else
	import Class Object mashutils
	
	Import set autoimport_ 1
	$self redefine_unknown
	return
}


#
Import proc.public disable_autoimport {} {
	Import set autoimport_ 0
	return
}

#
# Auto-importing is disabled by default.
# (And because dynamic loading using unknown yet, due to the fact the unknown
#  isn't called when the -auperclass attribute is used in a Class defn.)
#
Import disable_autoimport 



#
# Read the environment variable, TCLCL_IMPORT_DIRS, and store the directories in a list instvar.
# Afterwards, makes a call to the instproc that generates the table.
#
Import private init_table { } {
	$self instvar import_dirs_
	global env

	# If TCLCL_IMPORT_DIRS is not set before first time import proc
	# is called, it is set to '.'
	# Note that otherwise, '.' is not appended to TCLCL_IMPORT_DIRS.
	if { ![info exists env(TCLCL_IMPORT_DIRS)] } {
		set env(TCLCL_IMPORT_DIRS) .
	}

	set import_dirs_ ""
	foreach dir [$self smart_parse_env_var $env(TCLCL_IMPORT_DIRS)] {
		# If dir is relative, it is expanded to absolute.
		# Relative pathnames in TCLCL_IMPORT_DIRS will be considered
		# relative to '.'
		# which is the directory the mash interpreter was launched from
		# (unless the cd proc has been called since then)
		# So if mashlets are bing run from a browser, '.' starts out as
		# the directory from which the browser was launched.
		lappend import_dirs_ [$self condense_to_absolute_filename $dir]
	}

	# locate the actual import directories
	set dirs [$self find_import_dirs $import_dirs_]
	
	# the first time import is called, build a table of mappings from
	# objects to the file(s) they are defined in
	$self make_table $dirs
}


#
# Build an internal table of mappings from objects to the file(s) they
# are defined in.
#
Import private make_table { dirs } {
	foreach d $dirs {
		$self read_dir $d
	}

	$self incorporate_table_overrides
}


#
# After the table_ has been generated, override existing mappings with
# any user-defined mappings.
#
Import private incorporate_table_overrides {} {
	$self instvar overrideTable_ table_

	foreach object [array names overrideTable_] {
		set table_($object) $overrideTable_($object)
	}
}


#
# Return a list of directories in which a readable importTable can be found.
# For every path in TCLCL_IMPORT_DIRS, importLocation will be read, if
# it exists, and the absolute pathnames of the importTables that it points
# to will be appended to import_table_list_ .
# For dirs in which a readable importLocation file is not found,
# the absolute pathname of the importTable in that dir, if one exists,
# will be appended instead.
# Directories may be named using complete pathnames or pathnames relative
# to CURRENTDIR.
#
Import private find_import_dirs { dirs } {
	# Generate a list of potential directories in which an importTable
	# may be found
	set list {}
	foreach dir $dirs {
		set importLocation [$self file join $dir importLocation]
		set r [$self file readable $importLocation]
		if [lindex $r 0] {
			set lines [$self read_file_into_list $importLocation]
			foreach line $lines {
				# append absolute filename on this line to
				# the list
				lappend list [$self \
						condense_to_absolute_filename \
						[$self file join $dir $line]]
			}
			if { [lindex $r 1] != {} } {
				# destroy the http token
				unset [lindex $r 1]
			}
		} else {
			lappend list $dir
		}
	}
	
	# prune the list down to the directories in which an importTable
	# is actually readable
	$self instvar last_modified_
	set dirs ""
	foreach d $list {
		set import_table [$self file join $d importTable]
		set last_modified_($import_table) -1
		set r [$self file readable $import_table]
		if [lindex $r 0] {
			lappend dirs $d
			if { [lindex $r 1] != {} } {
				# destroy the http token
				unset [lindex $r 1]
			}
		}
	}

	#if { [llength $dirs] > 0 } {
	#	puts stderr "readable importTables found in: $dirs"
	#} else {
	#	puts stderr "no readable importTables found"
	#}
	
	return $dirs
}


#
# By reading the importTable in the provided directory <i>dir</i>,
# continue to define the elements of the table_ array.  Each element of
# this array is indexed using an object name and consists of a list of
# files (using absolute pathnames) in which the the object and its
# methods are defined.  Returns the table_ in list form.
#
Import private read_dir { dir } {
	$self instvar table_ classes_mapped_ last_modified_

	set importTableFile [$self condense_to_absolute_filename \
			[$self file join $dir importTable]]
	set last_modified_($importTableFile) -1
	
	# fetch the importTable and break it into a list of lines
	set lines [$self read_file_into_list $importTableFile]

	# for every line in the importTable, parse out the object and
	# the filename, adding the file to the appropriate list element
	# of the table_ array if it is not there already
	foreach line $lines {
		set index [lindex $line 0]
		#  use the absolute file name
		# (relative filenames are considered to be relative to
		# the directory containing the importTable)
		set fname [$self condense_to_absolute_filename \
				[$self file join $dir [lindex $line 1]]]
		set last_modified [string trim [lindex $line 2]]

		# if a mapping for this object was already read from
		# another importTable, ignore this one
		if [info exists classes_mapped_($index)] {
			continue
		}

		# if this object already has a mapping to this filename,
		# skip it
		if {[info exists table_($index)]} {
			if {-1!=[lsearch -exact $table_($index) $fname]} {
				continue
			}
		}
		
		lappend table_($index) $fname
		if { $last_modified!={} } {
			set last_modified_($fname) $last_modified
		}
		
		set this_mappings($index) 1
	}

	foreach index [array names this_mappings] {
		set classes_mapped_($index) 1
	}
}


# Provide the value of an env variable and a list of the ':' separated
# items is returned.
#
# Since [split $env(TCLCL_IMPORT_DIRS) :]
# doesn't work very well if some of the ':' separated items are
# URLs (w/"http:"), you can use this proc to intelligently parse up
# an env variable
#
Import private smart_parse_env_var { env_value } {
	set env $env_value
	while {[string length [set env [string trim $env ":"]]] != 0 } {
		if [regexp {^([^:]+)://([^:/]+)(:([0-9]+))?(/[^:]*)} \
				$env url protocol server x port trailingpath] {
			lappend list $url
			regsub {([^:]+)://([^:/]+)(:([0-9]+))?(/[^:]*)} \
					$env {} env
		} else {
			regexp {^[^:]+} $env dir
			lappend list $dir
			regsub {^[^:]+} $env {} env
		}
	}
	
	return $list
}


Import private consistency_check { } {
	global env
	$self instvar orig_val_

	if { ![info exists orig_val_] } {
		set orig_val_ $env(TCLCL_IMPORT_DIRS)
	}
	
	if { $env(TCLCL_IMPORT_DIRS) != $orig_val_ } {
		puts stderr "warning: ignoring modification to\
				env(TCLCL_IMPORT_DIRS)\nit is illegal to\
				modify this after the first call to the\
				import procedure."
	}
}


#
# returns an empty string on success, or the error string on failure
#
Import private source_file { file } {
	set file_readable_result [$self file readable $file]
	set file_readable [lindex $file_readable_result 0]
	
	if { $file_readable } {
		set read_token [lindex $file_readable_result 1] 
		$self source $file $read_token
		if { $read_token!={} } { unset $read_token }
		return ""
	} else {
		return [lindex $file_readable_result 1]
	}
}


Import private source { file { read_token {} } } {
	$self instvar loaded_ uniq_num_

	if { ![info exists uniq_num_] } {
		set uniq_num_ 0
	}

	# make sure the filename has been expanded to an absolute path
	# with sym links followed and extraneous '.'s and '..'s removed
	# so that we are most likely to recognize repeated attempts to
	# source the same file
	set file [$self condense_to_absolute_filename $file]
	if [info exists loaded_($file)] {
		#puts stdout "$fileName previously sourced. \
				#Not re-sourcing it."
		return 
	}
	set loaded_($file) 1

	# redirects explicit calls to the source proc within fileName
	# to this source proc in order to ensure that files
	# are not repeatedly sourced
	incr uniq_num_	
	uplevel \#0 "rename source source.$uniq_num_"
	uplevel \#0 "proc source {args} \{ $self source \$args \}"
	
	if [$self is_http_url $file] {
		set buffer [$self read_url $file $read_token]
		# catch errors so that errors in the file being sourced
		# do not cause this script to be exited prematurely
		# (btw, need {} around $buffer so variable substitutions
		# occur in the global scope and so blank lines at beginning
		# of buffer don't cause errors

		# XXX: the buffer must be sourced in the context where
		# "import" was called from!
		if [catch "uplevel \#0 {$buffer}" errormsg] {
			global errorInfo
			error "error in $file: $errormsg\n$errorInfo\n\n"
		}
	} else {
		# catch errors so that errors in the file being sourced
		# do not cause this script to be exited prematurely

		# XXX: the buffer must be sourced in the context where
		# "import" was called from!
		if [catch "uplevel \#0 source.orig $file" errormsg] {
			global errorInfo
			error "error in $file: $errormsg\n$errorInfo\n\n"
		}
	}

	# disabling the redirection to this alternate source proc
	uplevel \#0 {rename source {}}
	uplevel \#0 "rename source.$uniq_num_ source"
	incr uniq_num_ -1
}


#
# dummy procedure for backward compatibility
# Import is always enabled by default
#
Import private enable { args } {
}


#
# Map an OTcl class name <i>c</i> to a http-able name
#
Import private class_to_file c {
	regsub -all -- "/" $c "-" filename
	return $filename
}



#
# Return 1 if the <i>name</i> is a http URL and 0 otherwise.
# An http URL is of the form http://<server and possibly port num>/
# Note that the trailing slash is important.
#
Import private is_http_url { name } {
	if [regexp {([^:]+)://([^:/]+)(:([0-9]+))?(/.*)} $name url protocol \
			server x port trailingpath] {
		if { ![info exists protocol] } {
			return 0
		} else {
			return [regexp -nocase {http} $protocol]
		}
	} else {
		return 0
	}
}


#
# Returns a buffer read from the provided http <i>url</i>.
# If the url is unreadable, nothing is returned.
# To speed up performance, can send an <i>http_token</i> as the
# optional second arg if you know this url is readable.
#
Import private read_url { url {token {}} } {
	$self instvar use_http_cache_ cache_ last_modified_
	if { $token == {} } {
		# rather than checking if { [$self file readable $url] != 1 }
		# just copying that code here to speed up performance

		if $use_http_cache_ {
			if { ![info exists cache_] } {
				set cache_ [new HTTPCache]
			}

			if [info exists last_modified_($url)] {
				set buffer [$cache_ get $url \
						$last_modified_($url)]
			} else {
				set buffer [$cache_ get $url]
			}
			if { $buffer=="" } { unset buffer }
		}

		if { ![info exists buffer] } {
			set token [Http geturl $url]
			if { [lindex [set code [::http::code $token]] 1] \
					!= 200 } {
				error "couldn't read \"$url\": no such file \
						or directory (HTTP code $code)"
			}
			set buffer [::http::data $token]
			# destroy the http token
			unset $token

			# add this to the cache
			if $use_http_cache_ {
				if { ![info exists cache_] } {
					set cache_ [new HTTPCache]
				}
				
				$cache_ put $url $buffer
			}
		}
	} else {
		set buffer [::http::data $token]
	}
	return $buffer
}


#
# Eliminates the '..' and '.' in a pathname and returns the absolute
# path of an equivalent filename after following all symbolic links.
# This is useful if the filename was created using ufile join, which
# doesn't evaluate the '..' and '.'. This can also be a useful pre-processing
# of a filename before comparing for equality.
# Note: this procedure is only valid for names of local files that are
# in executable directories.
# The filename is returned if the directory is invalid or not executable.
#
Import private condense_to_absolute_filename { name } {
	# XXX: for now, just return $name
	
	return $name
	
	set before_cd [pwd]
	# follow symlinks 
	while { ![catch "file readlink $filename"] } {
		set filename [file readlink $filename]
	}
	set dirname [$self file dirname $filename]
	# XXX: souldn't this also be "$self file tail"?
	set tailname [file tail $filename]
	set condensed_name $filename
	if { ![catch "cd $dirname"] } {
		set condensed_name [ufile join [pwd] $tailname]
	}
	cd $before_cd
	return $condensed_name
}


Import private read_file_into_list { filename } {
	if [$self is_http_url $filename] {
		set buffer [$self read_url $filename]
		set lines [split $buffer "\n"]
	} else {
		set f [open $filename "r"]
		set lines {}
		while 1 {
			set line [gets $f]
			if [eof $f] {
				close $f
				break
			}
			lappend lines "$line"
		}
	}

	return $lines
}


Import private file_readable { args } {
	if { [llength $args] == 0 } {
		error "wrong # args: should be \"$self file\
				readable name ?arg ...?\""
	}

	set name [lindex $args 0]
	if [$self is_http_url $name] {
		$self instvar use_http_cache_ cache_ last_modified_
		if $use_http_cache_ {
			if { ![info exists cache_] } {
				set cache_ [new HTTPCache]
			}

			if [info exists last_modified_($name)] {
				set buffer [$cache_ get $name \
						$last_modified_($name)]
			} else {
				set buffer [$cache_ get $name]
			}
			if { $buffer!={} } {
				# XXX: I am creating a dummy http token
				# here
				$self instvar buf_cnt_
				if ![info exists buf_cnt_] {
					set buf_cnt_ 0
				}
				set token ::http::readable_$buf_cnt_
				upvar #0 $token state
				set state(body) $buffer
				incr buf_cnt_
				return [list 1 $token]
			}
		}
		if [catch {set token [Http geturl $name]} m] {
			return [list 0 "error executing \"::http::geturl\
					$name\": $m"]
		} else {
			set code [::http::code $token]
			if {[lindex $code 1] != 200} {
				return [list 0 $code]
			} else {
				if $use_http_cache_ {
					if { ![info exists cache_] } {
						set cache_ [new HTTPCache]
					}
					$cache_ put $name [::http::data $token]
				}
				
				return [list 1 $token]
			}
		}
	} else {
		eval file readable $args
	} 
}


#
# the file instproc is basically an enhanced Tcl file proc.
# The diff is that "Import::file readable" & "Import::file dirname" &
# "Import::file join" procedures can handle URLs.
# The only difference from the file proc in behavior is that...
# for URLs, "file readable" returns a list:
#  if readable,   list = { 1 <http-token> }
#  if unreadable, list = { 0 <http-code or other error msg> }
#
# Note that "Import::file join" continues to have the feature that an
# absolute pathname overrides any previous components.
#
Import public file { option args } {
	if { $option == "readable" } {
		eval [list $self] file_readable $args
	} elseif { $option == "dirname" } {
		if { [llength $args] == 0 } {
			error "wrong # args: should be \"$self file\
					dirname name ?arg ...?\""
		} else {
			set name [lindex $args 0]
			if [$self is_http_url $name] {
				set url $name
				regexp {([^:]+)://([^:/]+)(:([0-9]+))?(/.*)} \
						$name url protocol server x \
						port trailingpath
				if {[string length $trailingpath] == 0} {
					set trailingpath /
				}
				set trailingpath [file dirname "$trailingpath"]
				return "$protocol://$server$x$trailingpath"
			} else {
				eval {file $option} $args
			}
		}
	} elseif { $option == "join" } {
		if { [llength $args] == 0 } {
			error "wrong # args: should be \"$self file\
					join name ?arg ...?\""
		} else {
			set base_url "[string trimright [lindex $args 0] /]/"
			set file_name [lindex $args 1]
			if [$self is_http_url $file_name] {
				return $file_name
			}
			if { [$self is_http_url $base_url] && \
					[llength $args] ==2 } {
				# parse URL into components
				regexp {([^:]+)://([^:/]+)(:([0-9]+))?(/.*)} \
						$base_url url protocol server \
						x port trailingpath
				# get rid of initial ./ in file name
				regsub -all {^\./} $file_name {} file_name
				# change any /./ to / in file name
				regsub -all {/\./} $file_name {/} file_name
				# get rid of initial ../ in file name
				set counter 0
				while [regsub {^\.\./} $file_name {} \
						file_name] {
					incr counter
				}
				# for each inital ../ removed, traverse
				# up directory tree one level
				while { $counter > 0 } {
					set trailingpath [$self \
							format_as_dir_string \
							[$self file dirname \
							$trailingpath]]
					incr counter -1
				}
				set trailingpath "[$self format_as_dir_string \
						$trailingpath]$file_name"
				return "$protocol://$server$x$trailingpath"
			} else {
				eval {file $option} $args
			}
		}
	} else {
		eval {file $option} $args
	}
}


#
# Add on a trailing / if not already there.
#
Import private format_as_dir_string { dir_string } {
	return "[string trimright [$self file join $dir_string .] .]"
}


#
# Augment existing "source" procedure to get http files if <i>filename</i> starts with http.
#
rename source source.orig
proc source {fileName} {
	Import instvar instance_
	
	if ![info exists instance_] {
		set instance_ [new Import]
	}
	
	if [$instance_ is_http_url $fileName] {
		set buffer [$instance_ read_url $fileName]
		uplevel eval $buffer
	} else {
		uplevel source.orig [list $fileName]
	}
}




#
# When the Mash interpreter encounters "import <object(s)>", it will
# source the code for the supplied object(s).
#
# If import is explicitly called while we are configured to autoimport_,
# sorry, but we're gonna ignore that and wait for the class to be used before
# we import it.  On the other hand, if we are not configured to autoimport_,
# sure we'll import it for you right now.
#
proc import args {
	if { ![catch "Import set autoimport_"] && ![Import set autoimport_] } {
		if [catch "really_import $args" error_msg] {
			error $error_msg
		}
	}
}


#
# The first time this import procedure is called, an Import object is created
# and the importTables are read to create a mapping from objects to the
# files in which their class defn and method defns can be found.
#
proc import args {
	Import instvar instance_

	if ![info exists instance_] {
		set instance_ [new Import]
	}

	if [catch "eval $instance_ import $args" errormsg] {
		error $errormsg
	}
}



#
# As long as the import procedure has not yet been invoked, the user is
# free to override mappings that may be read from importTables.
#
proc override_importTable_mapping { object file_list } {
	Import instvar instance_

	if ![info exists instance_] {
		set instance_ [new Import]
	}

	$instance_ override_importTable_mapping $object $file_list
}


proc import_use_http_cache { {yes 1} } {
	Import instvar instance_

	if ![info exists instance_] {
		set instance_ [new Import]
	}

	$instance_ set use_http_cache_ 1
}
