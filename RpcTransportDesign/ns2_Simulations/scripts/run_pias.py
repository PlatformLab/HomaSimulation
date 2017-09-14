import threading
import os
import Queue
import time 

#from piasSearchParams import *
from piasHadoopParams import *
#from piasAllRpcParams import *
#from piasSearchRpcParams import *
#from piasKeyValueParams import *

def worker():
	while True:
		try:
			j = q.get(block = 0)
		except Queue.Empty:
			return
		#Make directory to save results
		os.system('mkdir -p '+j[1])
		os.system(j[0])

q = Queue.Queue()

for prio_num_ in prio_num_arr:
	for i in range(len(load_arr)):

		scheme = 'unknown'
		if switchAlg == 'Priority' and prio_num_ > 1 and\
                            sourceAlg == 'DCTCP-Sack':
			scheme = 'pias'
		elif switchAlg == 'Priority' and prio_num_ == 1:
			if sourceAlg == 'DCTCP-Sack':
				scheme = 'dctcp'
			elif sourceAlg == 'LLDCT-Sack':
				scheme = 'lldct'

		if scheme == 'unknown':
			print 'Unknown scheme'
			sys.exit(0)

		#Directory name: workload_scheme_load_[load]
		directory_name = '%s_%s_%d' % (workloadName,
                    scheme,int(load_arr[i]*100))
		directory_name = directory_name.lower()
                localtime = time.localtime()
                directory_name = 'traces'+ '/' + '%.4d%.2d%.2d_%.2d%.2d' % (
                    localtime.tm_year, localtime.tm_mon, localtime.tm_mday, 
                    localtime.tm_hour,localtime.tm_min) + '/' + directory_name

		#Simulation command
		cmd = ns_path+' '+sim_script+' '\
			+str(sim_end)+' '\
			+str(link_rate)+' '\
			+str(mean_link_delay)+' '\
			+str(host_delay)+' '\
			+str(queueSize)+' '\
			+str(load_arr[i])+' '\
			+str(connections_per_pair)+' '\
			+str(meanFlowSize)+' '\
			+str(gptp_ratio)+' '\
			+str(paretoShape)+' '\
			+str(flow_cdf)+' '\
			+str(enableMultiPath)+' '\
			+str(perflowMP)+' '\
			+str(sourceAlg)+' '\
			+str(initWindow)+' '\
			+str(ackRatio)+' '\
			+str(slowstartrestart)+' '\
			+str(DCTCP_g)+' '\
			+str(min_rto)+' '\
			+str(prob_cap_)+' '\
			+str(switchAlg)+' '\
			+str(DCTCP_K)+' '\
			+str(drop_prio_)+' '\
			+str(prio_scheme_)+' '\
			+str(deque_prio_)+' '\
			+str(keep_order_)+' '\
			+str(prio_num_)+' '\
			+str(ECN_scheme_)+' '\
			+str(pias_thresh_0[i])+' '\
			+str(pias_thresh_1[i])+' '\
			+str(pias_thresh_2[i])+' '\
			+str(pias_thresh_3[i])+' '\
			+str(pias_thresh_4[i])+' '\
			+str(pias_thresh_5[i])+' '\
			+str(pias_thresh_6[i])+' '\
			+str(topology_spt)+' '\
			+str(topology_tors)+' '\
			+str(topology_spines)+' '\
			+str(topology_x)+' '\
			+str('./'+directory_name+'/flow.tr')+'  >'\
			+str('./'+directory_name+'/logFile.tr')
                print cmd
		q.put([cmd, directory_name])

#Create all worker threads
threads = []
number_worker_threads = 4

#Start threads to process jobs
for i in range(number_worker_threads):
	t = threading.Thread(target = worker)
	threads.append(t)
	t.start()

#Join all completed threads
for t in threads:
	t.join()
