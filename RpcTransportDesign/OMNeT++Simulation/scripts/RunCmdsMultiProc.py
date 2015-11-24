#!/usr/bin/python

"""
This scripts runs a set of external commands in prallel on multiple processors.
"""

from __future__ import print_function
import os
import subprocess
import random
import re
import sys
from optparse import OptionParser
from Queue import Queue
from threading import Thread

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

def main(filename, numThreads):
    queue = Queue()
    threads = [Thread(target=simWorker, args=(queue,)) for _ in range(numThreads)]
    for t in threads:
        t.daemon = True # threads die if the program dies
        t.start()

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
    for t in threads: t.join() # wait for completion

if __name__=="__main__":
    parser = OptionParser(description='Runs multiple homatransport simulations'
        ' in multiprocess fashion.')
    parser.add_option('--cmdFile', metavar='FILENAME',
        default = '',
        dest='fileName',
        help='The file cotaining homatransport run commands. This file is'
        ' expected to be in "homatransport/simulations/runCommands/"'
        ' directory')
    parser.add_option('--numThreads', metavar='#NUM_THREADS',
        default = '8',
        dest='numThreads',
        help='The number of available cores on the machine')
 
    options, args = parser.parse_args()
    filename = options.fileName
    numThreads = int(options.numThreads)
    main(filename, numThreads)
