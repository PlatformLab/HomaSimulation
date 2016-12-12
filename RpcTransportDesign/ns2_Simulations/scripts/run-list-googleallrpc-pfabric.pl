#!/usr/bin/perl -w
$sim_end = 20000000;

$cap = 10;
$link_delay = 0.000000250;
$host_delay = 0.0000005;
@queueSize = (13);

@load = (0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8);

$connections_per_pair = 1;
$meanFlowSize =  3150.3;

$enableMultiPath = 1;
@perflowMP = (0);

@sourceAlg = ("DCTCP-Sack");
@ackratio = (1);
@slowstartrestart = ("true");
$DCTCP_g = 1.0/16.0;
@min_rto = (0.000024);

$switchAlg = "DropTail";
@DCTCP_K = (10000);
@drop_prio_ = ("true");
@deque_prio_ = ("true");
@prio_scheme_ = (2);
@prob_mode_ = (5);
@keep_order_ = ("true");

@topology_spt = (16); #server per tor
@topology_tors = (9);
@topology_spines = (4);
@topology_x = (1);

$enableNAM = 0;

$numcores = 8;

$trial = 1;

## Change this according to your environment
$user = "behnamm";
$top_dir = "/home/behnamm/Research/RpcTransportDesign/ns2_Simulations";
###########################################
$tcl_script = "empirical_googleallrpc_pfabric";
$work_dir = "/scratch/behnamm/pfabric/rc33/work_dir";
###########################################
# ns source code server
$ns_source_server = "rc33";
# ns source code path
$ns_source_path = "$top_dir"; # we expect tree /ns-allinone-2.34 to be at this directory
                              # for now, this also needs to be the same as $top_dir
###########################################
# worker server List
@server_list = ("rc33");
$server_loop_first = 1;  # First loop across servers, than cores
###########################################
# log destination server
$log_server = "rcmaster";
$log_dir = "/scratch/behnamm/pfabric/rc33/logs";

###########################################
# Prepare servers
###########################################
print "compressing ns...\n";
`ssh $user\@$ns_source_server 'cd $ns_source_path && tar cvzf ns-latest.tar.gz ns-allinone-2.34 >> /dev/null'`;

print "backing up ns at parent folder...\n";
`scp $user\@$ns_source_server:$ns_source_path/ns-latest.tar.gz ..`;

$ns_path ="$top_dir/ns-allinone-2.34/bin:$top_dir/ns-allinone-2.34/tcl8.4.18/unix:$top_dir/ns-allinone-2.34/tk8.4.18/unix:\$PATH";
$ld_lib_path = "$top_dir/ns-allinone-2.34/otcl-1.13:$top_dir/ns-allinone-2.34/lib";
$tcl_lib = "$top_dir/ns-allinone-2.34/tcl8.4.18/library";
$set_path_cmd = "export PATH=$ns_path && export LD_LIBRARY_PATH=$ld_lib_path && export TCL_LIBRARY=$tcl_lib";

foreach (@server_list) {
    $server = $_;
    if ($server ne $ns_source_server) {
	print "copying and decompressing ns at $server...\n";
	`scp ../ns-latest.tar.gz $user\@$server:$top_dir/.`;
	`ssh $user\@$server 'cd $top_dir && tar xvzf ns-latest.tar.gz >> /dev/null'`;
    } else {
	print "skipping copy/decompress ns for $server...\n";
    }
    `ssh $user\@$server 'killall ns'`;
    sleep(1);

    `ssh $user\@$server 'mkdir -p $work_dir'`;
    `ssh $user\@$server 'rm -rf $work_dir/proc*'`;
}

`ssh $user\@$log_server 'mkdir -p $log_dir'`;
`ssh $user\@$log_server 'mkdir $log_dir/logs'`;


$sleep_time = 10;
##########################################
# Run tests
##########################################
foreach (@load) {
    $cur_load = $_;
    for ($i = 0; $i < @topology_spt; $i++) {
	$cur_topology_spt = @topology_spt[$i];
	$cur_topology_tors = @topology_tors[$i];
	$cur_topology_spines = @topology_spines[$i];
	$cur_topology_x = $topology_x[$i];
	foreach (@perflowMP) {
	    $cur_perflowMP = $_;
	    foreach (@queueSize) {
		$cur_queueSize = $_;
		foreach (@sourceAlg) {
		    $cur_sourceAlg = $_;
		    foreach (@ackratio) {
			$cur_ackratio = $_;
			foreach(@slowstartrestart) {
			    $cur_slowstartrestart = $_;
			    foreach(@min_rto) {
				$cur_rto = $_;
				for ($i = 0; $i < @DCTCP_K; $i++) {
				    $cur_DCTCP_K = $DCTCP_K[$i];
				    foreach($dp = 0; $dp < @drop_prio_; $dp++)  {
					$cur_drop_prio = $drop_prio_[$dp];
					$cur_prio_scheme = $prio_scheme_[$dp];
					$cur_deque_scheme = $deque_prio_[$dp];
					foreach($prob = 0; $prob < @prob_mode_; $prob++) {
					    $cur_prob_cap = $prob_mode_[$prob];
					    foreach($ko = 0; $ko < @keep_order_; $ko++) {
						$cur_ko = $keep_order_[$ko];

						$arguments = "$sim_end $cap $link_delay $host_delay $cur_queueSize $cur_load $connections_per_pair $meanFlowSize $enableMultiPath $cur_perflowMP $cur_sourceAlg $cur_ackratio $cur_slowstartrestart $DCTCP_g $cur_rto $switchAlg $cur_DCTCP_K $cur_drop_prio $cur_prio_scheme $cur_deque_scheme $cur_prob_cap $cur_ko $cur_topology_spt $cur_topology_tors $cur_topology_spines $cur_topology_x $enableNAM";

						if ($trial < 10) {
						    $strial = "00$trial";
						} elsif ($trial < 100) {
						    $strial = "0$trial";
						} else {
						    $strial = $trial;
						}

						$dir_name = "$strial-$tcl_script-s$cur_topology_spt-x$cur_topology_x-q$cur_queueSize-load$cur_load-avesize$meanFlowSize-mp$cur_perflowMP-$cur_sourceAlg-ar$cur_ackratio-SSR$cur_slowstartrestart-$switchAlg$cur_DCTCP_K-minrto$cur_rto-drop$cur_drop_prio-prio$cur_prio_scheme-dq$cur_deque_scheme-prob$cur_prob_cap-ko$cur_ko";

						#print "args $arguments\n";
						#print "dir_name $dir_name \n";

						++$trial;

						$found = 0;
						$proc_index = 0;
						$server_index = 0;
						while (!$found) {
						    $server = $server_list[$server_index];
						    $target_dir = "$work_dir/proc$proc_index";

						    #print "checking availability of $target_dir at $server\n";
						    `ssh $user\@$server 'test -e $target_dir/logFile.tr'`;
						    $rc = $? >> 8;
						    if ($rc) {
							# target_dir/logFile.tr doesn't exist
							$found = 1;
							`ssh $user\@$server 'test -d $target_dir/'`;
							$rc = $? >> 8;
							if ($rc) {
							    # target_dir doesn't exist
							    `ssh $user\@$server 'mkdir $target_dir'`;
							} else {
							    wait();
							}
							`ssh $user\@$server 'rm -f $target_dir/*'`;
							last;
						    }

						    if ($server_loop_first == 1) {

							$server_index++;
							if ($server_index == @server_list) {
							    $server_index = 0;
							    $proc_index++;
							    if ($proc_index == $numcores) {
								$proc_index = 0;
								print "sleeping $sleep_time sec..\n";
								sleep($sleep_time);
							    }
							}
						    } else {
							$proc_index++;
							if ($proc_index == $numcores) {
							    $proc_index = 0;
							    $server_index++;
							    if ($server_index == @server_list) {
								$server_index = 0;
								print "sleeping $sleep_time sec..\n";
								sleep($sleep_time);
							    }
							}
						    }
						}

						print "starting $dir_name process on $server at $target_dir\n";
						`scp *.tcl $user\@$server:$target_dir/.`;

						$pid = fork();
						die "unable to fork: $!" unless defined($pid);
						if (!$pid) {  # child
						    `ssh $user\@$server '$set_path_cmd && cd $target_dir && nice ns $tcl_script.tcl $arguments > logFile.tr'`;
						    print "$dir_name (proc$proc_index) on $server finished. cleaning up...\n";
						    `ssh $user\@$log_server 'rm -rf $log_dir/logs/$dir_name/'`;
						    `ssh $user\@$log_server 'mkdir $log_dir/logs/$dir_name/'`;
						    `ssh $user\@$server 'rm -f $target_dir/queue*.tr'`;
						    `mkdir temp$server$proc_index`;
						    `scp $user\@$server:$target_dir/*.tr  temp$server$proc_index/.`;
						    `scp $user\@$server:$target_dir/out.nam temp$server$proc_index/.`;
						    `scp temp$server$proc_index/*  $user\@$log_server:$log_dir/logs/$dir_name/.`;
						    `rm -rf temp$server$proc_index`;
						    `ssh $user\@$server 'rm -f $target_dir/*'`;
						    exit(0);
						} else {
						    #print "started ns with index $proc_index and pid $pid\n";
						    sleep(1);
						}
					    }
					}
				    }
				}
			    }
			}
		    }
		}
	    }
	}
    }
}

print "waiting for child procs to terminate.\n";

while (wait() != -1) {
    print "waiting for child procs to terminate.\n";
}


#######################################################
# Create log folder
#######################################################
($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst)  = localtime(time);
$mon++;


`ssh $user\@$log_server 'mv $log_dir/logs $log_dir/$mon$mday-searchallrpc-pfabric'`;

######################################################
# Cleanup servers
######################################################
foreach (@server_list) {
    $server = $_;
    print "cleaning up $server...\n";
    `ssh $user\@$server 'rm -rf $work_dir/proc*'`;

}

print "Done\n";
