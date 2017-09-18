import os
import sys

sim_end = 1000000
link_rate = 10
mean_link_delay = 0.000000250
host_delay = 0.0000005
queueSize = 250
#load_arr = [0.9, 0.8, 0.7, 0.6, 0.5]
load_arr = [0.8, 0.5]
connections_per_pair = 10
meanFlowSize = 1745.0 * 1460 
gptp_ratio = 2547262.0/2686844.0 # ratio of goodput over throughput. Throughput
                                 # will include pkt headers and acks
paretoShape = 1
flow_cdf = 'CDF_search.tcl'

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
#pias_thresh_0 = [759*1460 , 909*1460 , 999*1460 , 956*1460 , 1059*1460]
#pias_thresh_1 = [1132*1460, 1329*1460, 1305*1460, 1381*1460, 1412*1460]
#pias_thresh_2 = [1456*1460, 1648*1460, 1564*1460, 1718*1460, 1643*1460]
#pias_thresh_3 = [1737*1460, 1960*1460, 1763*1460, 2028*1460, 1869*1460]
#pias_thresh_4 = [2010*1460, 2143*1460, 1956*1460, 2297*1460, 2008*1460]
#pias_thresh_5 = [2199*1460, 2337*1460, 2149*1460, 2551*1460, 2115*1460]
#pias_thresh_6 = [2325*1460, 2484*1460, 2309*1460, 2660*1460, 2184*1460]

pias_thresh_0 = [909*1460 , 1059*1460]
pias_thresh_1 = [1329*1460, 1412*1460]
pias_thresh_2 = [1648*1460, 1643*1460]
pias_thresh_3 = [1960*1460, 1869*1460]
pias_thresh_4 = [2143*1460, 2008*1460]
pias_thresh_5 = [2337*1460, 2115*1460]
pias_thresh_6 = [2484*1460, 2184*1460]

topology_spt = 16
topology_tors = 9
topology_spines = 4
topology_x = 1

ns_path = os.environ['HOME'] +\
    '/Research/RpcTransportDesign/ns2_Simulations/ns-allinone-2.34/bin/ns'
sim_script = 'search_pias.tcl'
workloadName = 'search'

