#!/usr/bin/python

"""
This scripts runs a set of homa simulations in prallel on multiple processors of
a subset of worker nodes. 
Information about the simulation runs and worker nodes comes from the
schedulingConfig.py. The scheduling of simulation runs on the worker nodes
follows a fire and forget behavior based on the information provided in
schedulingConfig.py scripts. User need to make sure the info in that script is
valid. 
"""

from __future__ import print_function
import os
import subprocess
import random
import re
import sys
import numpy
import time
from optparse import OptionParser
from Queue import Queue
from threading import Thread
from schedulingConfig import *

"""
This function reads gets a command with arguments and a execute it. Each
commands must be written on a separate line.
"""
def run(homatransportCmd):
    filePath = os.path.join(os.environ['HOME'], ('Research/RpcTransportDesign/'
        'OMNeT++Simulation/homatransport' '/src/dcntopo'))
  
    devNull = open(os.devnull,"w")
    cmdToRun = 'cd {0};'.format(filePath) + '{0};'.format(homatransportCmd)
    print('%s\n' % (cmdToRun), file=sys.stdout)
    subprocess.check_call(cmdToRun, shell=True, stdout=devNull)
    print('Finished Simulation:\n\t%s\n' % (cmdToRun), file=sys.stdout)
    devNull.close()

def simWorker(queue):
    """
    Takes a command from the queue and calls run for it. If errors happen, it
    also handles possible the errors.
    """
    for args in iter(queue.get, None):
        try:
            run(args)
        except Exception as e: # catch exceptions to avoid exiting the
                               # thread prematurely
            print('%r failed: %s' % (args, e,), file=sys.stderr)

def workerMain(filename, numThreads):
    queue = Queue()
    threads = (
        [Thread(target=simWorker, args=(queue,)) for _ in range(numThreads)])
    filepath = os.path.join(os.environ['HOME'], 
        ('Research/RpcTransportDesign/'
        'OMNeT++Simulation/homatransport/simulations/runCommands/'),
        filename)

    f = open(filepath, 'r') 
    # populate commands read from file in the queue
    for line in f:
        cmd = re.split('(.+)', line)[1] 
        queue.put_nowait(cmd)
    f.close()
    for _ in threads: queue.put_nowait(None) # signal no more files

    for t in threads:
        t.daemon = True # threads die if the program dies
        t.start()
    for t in threads: t.join() # wait for completion

def masterMain():
    """
    This function reads all cluster and scheduling config info from the
    schedulingConfig script, divides appropriate number of simulations
    between the worker nodes proportional to the number of threads available on
    the worker nodes and runs the simulations on those nodes.
    """

    # We use fair queuing approach for assigning runs to workers. Number of runs
    # assigned to each worker is proportional to the number of threads running
    # on that worker.
    scrambledRunIdent = runIdentities[:]
    random.shuffle(scrambledRunIdent)

    allWorkers = workerNodes[:]
    allWorkers.insert(0, masterNode)
    allThreads = sum([worker[1] for worker in allWorkers])
    numAllRuns = len(scrambledRunIdent) 
    for worker in allWorkers:
        # For each worker, create the command file
        if len(scrambledRunIdent) == 0:
            break
        workerName = worker[0]
        workerThreads= worker[1]
        runsForWorker = int(numpy.ceil(1.0*workerThreads*numAllRuns/allThreads))
        cmdfilename = 'runCmds_%s' % workerName 
        cmdFullFilename = os.path.join(os.environ['HOME'], 
            ('Research/RpcTransportDesign/'
            'OMNeT++Simulation/homatransport/simulations/runCommands/'),
            cmdfilename)
        f = open(cmdFullFilename, 'w')
        for _ in range(runsForWorker): 
            if len(scrambledRunIdent) == 0:
                break
            runIdentity = scrambledRunIdent.pop()  
            workLoad = runIdentity[0] 
            runNum = runIdentity[1]
            homaCmd = ('../homatransport -u Cmdenv -c Workload%s -r %d -n '
                '..:../../simulations:../../../inet/examples:../../../inet/src '
                '-l ../../../inet/src/INET %s\n' % (workLoad, runNum, omnetConfigFile,))
            f.write(homaCmd)
        f.close()
        time.sleep(2)

        # ssh into worker and run this script in worker mode
        try:
            scriptPath = os.path.join(os.environ['HOME'], 
                'Research/RpcTransportDesign/OMNeT++Simulation/scripts')
            scriptToRun = ('./runCmdsMultiProc.py --serverType worker '
                '--workerCmdFile %s --numThreads %d' %
                (cmdfilename, workerThreads,))
            sshCmd = ("""ssh -n -f %s "sh -c 'cd %s; nohup %s > /dev/null """
                """2>&1 &'" """ % (workerName, scriptPath, scriptToRun,))
            devNull = open(os.devnull,"w")
            subprocess.check_call(sshCmd, shell=True, stdout=devNull) 
            devNull.close()
        except Exception as e:
            devNull.close()
            print('Can not ssh to worker node: %s' % (workerName),
                file=sys.stderr)

def killAll():
    """
    ssh into all worker nodes and kill simulation runs
    """
    allWorkers = workerNodes[:]
    allWorkers.append(masterNode)
    for worker in allWorkers:
        workerName = worker[0]
        killCmd = ("kill -SIGKILL \$(ps aux | grep runCmdsMultiProc | grep worker |"
            " grep -v grep | awk '{print \$2}') > /dev/null 2>&1;"
            " kill -SIGINT \$(pidof homatransport) > /dev/null 2>&1 &")
        sshKillCmd = ('ssh -n -f %s "%s"'
            % (workerName, killCmd,))
        try:
            devNull = open(os.devnull,"w")
            subprocess.check_call(sshKillCmd, shell=True, stdout=devNull)
            devNull.close()
        except Exception as e:
            devNull.close()
            print('Can not kill simulaton on worker node: %s' % (workerName),
                file=sys.stderr)

if __name__=="__main__":
    parser = OptionParser(description='Runs multiple homatransport simulations'
        ' in multiprocess fashion on a subset of rcXX machines defined in'
        ' schedulingConfig.py. Please make sure you have properly updated the'
        ' info in schedulingConfig.py script before running any simulation.'
        ' Note: Users of this script must run this script with no argument.')
    parser.add_option('--serverType', metavar='master/worker',
        default = '', dest='serverType', 
        help = 'Defines if this server is a master or worker node for running'
        ' simulations.')
    parser.add_option('--workerCmdFile', metavar='FILENAME',
        default = '', dest='fileName',
        help='The file cotaining homatransport run commands for a worker node.'
        ' This file is expected to be in'
        ' "homatransport/simulations/runCommands/" directory')
    parser.add_option('--numThreads', metavar='NUM_THREADS',
        default = '6',
        dest='numThreads',
        help='The number of available cores on the worker machine')
    parser.add_option("--kill",
        action="store_true", dest="kill", default=False,
        help="Kills all simulation runs on all worker nodes.")

    options, args = parser.parse_args()
    filename = options.fileName
    numThreads = int(options.numThreads)
    serverType = options.serverType

    if options.kill:
        print('Killing all simulation instances on worker nodes.')
        killAll()
        exit()

    if serverType == '':
        # make sure the master node is defined in the schedulingConfig.py
        try:
            masterNode
        except Exception as e:
            print('No master node defined in schedulingConfig.py',
                file=sys.stderr)
        masterName = masterNode[0]
        masterThreads = masterNode[1]

        # ssh into master node and run this script with server type master
        try:
            scriptPath = os.path.join(os.environ['HOME'], 
                'Research/RpcTransportDesign/OMNeT++Simulation/scripts')
            scriptToRun = './runCmdsMultiProc.py --serverType master '
            sshCmd = ("""ssh -n -f %s "sh -c 'cd %s; nohup %s > /dev/null"""
                """ 2>&1 &'" """ % (masterName, scriptPath, scriptToRun,))
            devNull = open(os.devnull,"w")
            subprocess.check_call(sshCmd, shell=True, stdout=devNull) 
            devNull.close()
        except Exception as e:
            devNull.close()
            print('Can not ssh to master node: %s' % (masterName),
                file=sys.stderr)
            exit()

    elif serverType == 'master':
        masterMain()

    elif serverType == 'worker':
        workerMain(filename, numThreads)
    
    else:
        exit('Unknow servertype: %s' % (serverType))
