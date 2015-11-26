# Defines all the servers to be used as the workers for running simulations and
# the number of threads available to run simulations on each of the servers. In
# addition, the master simulation scheduler is also declared here. The
# definition format of each server must come like this: (rcXX, numThreads)
workerNodes = []
threads = 5
for i in range(20, 22):
    workerNodes.append(('rc%02d' % i, threads))

# Master node that schedules simulations on the workerNodes. Need not to be
# among the workerNodes
masterThreads = 30
masterNode = ('rcmonster', masterThreads)

# This file also defines the workload types that we want to run simulations for.
# Also for each Workload, it defines a list of pairs like (workload, runnumber).
# runnumber is the specific simulation run number that we want to run for that
# workload
workloads = ['KeyValue', 'DCTCP', 'WebServer', 'CacheFollower', 'Hadoop']
runsPerWorkload= 63
runNumbers = range(runsPerWorkload)
runIdentities = []
for workload in workloads:
    for runnumber in runNumbers:
        runIdentities.append((workload, runnumber))

if __name__ == '__main__':
    print('\nThe master simulation scheduler node and # of its threads')
    print(masterNode)
    print('\nAll woker servers and num threads on each server')
    for server in workerNodes:
        print(server)

    print('\nAll workloads and run numbers for workloads')
    for identity in runIdentities:
        print(identity)
