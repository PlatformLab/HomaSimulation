import os
import sys

sim_end = 5000000
link_rate = 10
mean_link_delay = 0.000000250
host_delay = 0.0000005
queueSize = 250 # large queue size to get stable simulation
#load_arr = [0.9, 0.8, 0.7, 0.6, 0.5]
load_arr = [0.8, 0.5]
connections_per_pair = 10
meanFlowSize =127796.6 
gptp_ratio = 127796.6/134847 # ratio of goodput over throughput. Throughput
                             # will include pkt headers and acks
paretoShape = 1
flow_cdf = 'Facebook_HadoopDist_All.tcl'

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

pias_thresh_0 = [38210, 43898]
pias_thresh_1 = [48958, 49621]
pias_thresh_2 = [50905, 50878]
pias_thresh_3 = [68384, 64461]
pias_thresh_4 = [73799, 70549]
pias_thresh_5 = [76588, 73131]
pias_thresh_6 = [78129, 74788]

topology_spt = 16
topology_tors = 9
topology_spines = 4
topology_x = 1

ns_path = os.environ['HOME'] +\
    '/Research/RpcTransportDesign/ns2_Simulations/ns-allinone-2.34/bin/ns'
sim_script = 'search_pias.tcl'
workloadName = 'facebookhadoop'

