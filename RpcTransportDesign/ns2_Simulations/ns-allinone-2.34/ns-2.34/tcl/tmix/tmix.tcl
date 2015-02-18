 # Copyright 2007, Old Dominion University
 # Copyright 2007, University of North Carolina at Chapel Hill
 #
 # Redistribution and use in source and binary forms, with or without 
 # modification, are permitted provided that the following conditions are met:
 # 
 #    1. Redistributions of source code must retain the above copyright 
 # notice, this list of conditions and the following disclaimer.
 #    2. Redistributions in binary form must reproduce the above copyright 
 # notice, this list of conditions and the following disclaimer in the 
 # documentation and/or other materials provided with the distribution.
 #    3. The name of the author may not be used to endorse or promote 
 # products derived from this software without specific prior written 
 # permission.
 # 
 # THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 # IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 # WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 # DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, 
 # INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 # (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 # SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 # CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
 # STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 # ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 # POSSIBILITY OF SUCH DAMAGE.
 #
 # Contact: Michele Weigle (mweigle@cs.odu.edu)

Tmix instproc alloc-tcp {tcptype} {
    if {$tcptype != "Reno"} {
	set tcp [new Agent/TCP/FullTcp/$tcptype]
    } else {
	set tcp [new Agent/TCP/FullTcp]
    }
    return $tcp
}

Tmix instproc setup-tcp {tcp fid wnd} {
    # set flow ID
    $tcp set fid_ $fid
    $tcp set window_ $wnd

    # register done procedure for when connection is closed
    $tcp proc done {} "$self done $tcp"
}

# These need to be here so that we can attach the
# apps to a node (done in Tcl code)
Tmix instproc alloc-app {} {
    return [new Application/Tmix]
}

Tmix instproc done {tcp} {
    # the connection is done, so recycle the agent
    $self recycle $tcp
}

Tmix instproc now {} {
    set ns [Simulator instance]
    return [format "%.6f" [$ns now]]
}
