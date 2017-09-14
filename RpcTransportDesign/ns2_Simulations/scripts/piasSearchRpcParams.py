sim_end = 1000000
link_rate = 10
mean_link_delay = 0.000000250
host_delay = 0.0000005
queueSize = 150
#load_arr = [0.9, 0.8, 0.7, 0.6, 0.5]
load_arr = [0.8, 0.5]
connections_per_pair = 10
meanFlowSize = 440.79
gptp_ratio = 440.8/530.6 # ratio of goodput over throughput. Throughput
                         # will include pkt headers and acks
paretoShape = 1
flow_cdf = 'Google_SearchRPC.tcl'

enableMultiPath = 1
perflowMP = 0

sourceAlg = 'DCTCP-Sack'
#sourceAlg='LLDCT-Sack'
initWindow = 7
ackRatio = 1
slowstartrestart = 'true'
DCTCP_g = 0.0625
min_rto = 0.000200
prob_cap_ = 5

switchAlg = 'Priority'
DCTCP_K = 15
drop_prio_ = 'true'
prio_scheme_ = 2
deque_prio_ = 'true'
keep_order_ = 'true'
prio_num_arr = [8]
ECN_scheme_ = 2 #Per-port ECN marking

pias_thresh_0 = [368, 382]
pias_thresh_1 = [410, 414]
pias_thresh_2 = [422, 422]
pias_thresh_3 = [435, 431]
pias_thresh_4 = [442, 437]
pias_thresh_5 = [451, 441]
pias_thresh_6 = [457, 444]

topology_spt = 16
topology_tors = 9
topology_spines = 4
topology_x = 1

ns_path = '/home/neverhood/Research/RpcTransportDesign/'\
    'ns2_Simulations/ns-allinone-2.34/bin/ns'
sim_script = 'search_pias.tcl'
workloadName = 'googlerpc'

