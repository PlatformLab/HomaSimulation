#!/usr/bin/python

from numpy import *
from glob import glob
from optparse import OptionParser
from pprint import pprint
from functools import partial
from xml.dom import minidom
import math
import os
import subprocess
import random
import re
import sys
import warnings
#sys.path.insert(0, os.environ['HOME'] +\
#    '/Research/RpcTransportDesign/OMNeT++Simulation/analysis')
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)) +\
    '/..')
from parseResultFiles import *
from MetricsDashBoard import *

if __name__ == '__main__':
    parser = OptionParser(description = 'This script is meant to post-process '
        'the vector result files to extract a specific stats metric and create '
        'an output file for that metric that is tabulated similar to R '
        'dataframe format. This script takes in a fileList filename argument '
        'that contains homa\'s vector-result-file filenames, one per each line.'
        'The vector-result filenames are used to compute cumulative percent of '
        'times for number of active scheduled messages.')

    parser.add_option('--fileList', metavar='FILENAME', default='',
        dest='fileList', help='name of file, containing vector result '
        'files names, one filename per line')

    parser.add_option('--outputType', metavar='a', default='',
        dest='outputType', help='Type of output stats metric to be processed '
        'from the vector result files.')

    options, args = parser.parse_args()
    fileList = options.fileList
    if fileList == '':
        sys.exit('\'fileList\' input args is required!')

    outputType = AttrDict({'activeScheds' : False})
    hasValidType = False
    for char in options.outputType:
        if char == 'a':
            outputType.activeScheds = True
            hasValidType = True
            activeTimePctFile = 'activeTimePct.txt'

    if not(hasValidType):
        sys.exit('Enter a valid combination for --outputType command line'
        ' option!')

    # open file list and read the result files
    absPath = os.path.abspath(fileList)
    absDir = os.path.dirname(absPath)
    f = open(absPath)
    resultFiles = [line.rstrip('\n') for line in f]
    f.close()

    # read output from resultFiles
    activeScheds = []
    for elem in resultFiles:
        vecFile = os.path.join(absDir, elem)
        print vecFile
        match = re.match('(.*)\.vec', vecFile)
        vciFile = match.group(1) + '.vci'
        print vciFile
        vp = VectorParser(vciFile, vecFile)

        # Simulation config parameters
        redundancy = vp.paramDic.numSendersToKeepGranted
        workloadType =  vp.paramDic.workloadType
        nominalLoad = float(vp.paramDic.rlf)*100
        prioLevels = int(eval(vp.paramDic.prioLevels))
        linkCheckBytes = int(vp.paramDic.linkCheckBytes)
        schedPrioLevels = int(eval(vp.paramDic.adaptiveSchedPrioLevels))

        if outputType.activeScheds:
            vecName = '"Time division of num active sched senders"'
            ids = vp.getIdsByNames([vecName] , ['globalListener'])
            data = []
            for i in ids[vecName]:
                data += vp.getVecDataByIds(i)

            # write the output data to dataframe format
            for result in data:
                cumTimePct = result[1]/data[-1][1]
                activeMesgs = int(result[2])
                activeScheds.append((prioLevels, schedPrioLevels, workloadType,
                    redundancy, nominalLoad, linkCheckBytes, activeMesgs,
                    cumTimePct))

    # dump the results in outputfiles
    scriptPath = os.path.dirname(os.path.realpath(__file__))
    tw_h = 40
    tw_l = 15 
    if outputType.activeScheds:
        # create output file
        activeMesgFd = open(os.path.join(scriptPath, "activeMesgs.txt"), 'w')
        activeMesgFd.write('prioLevels'.ljust(tw_l) +
            'schedPrioLevels'.center(tw_l) + 'workload'.center(tw_h) +
            'initialRedundancy'.center(tw_l) +
            'nominalLoadFactor'.center(tw_l) + 'linkCheckBytes'.center(tw_l) +
            'activeMesgs'.center(tw_l) + 'cumTimePct'.center(tw_l) + '\n')

        # dump the results in outputfile
        for activeMesgStat in activeScheds:
            activeMesgFd.write(
                '{0}'.format(activeMesgStat[0]).ljust(tw_l) +
                '{0}'.format(activeMesgStat[1]).center(tw_l) +
                '{0}'.format(activeMesgStat[2]).center(tw_h) +
                '{0}'.format(activeMesgStat[3]).center(tw_l) +
                '{0}'.format(activeMesgStat[4]).center(tw_l) +
                '{0}'.format(activeMesgStat[5]).center(tw_l) +
                '{0}'.format(activeMesgStat[6]).center(tw_l) +
                '{0}'.format(activeMesgStat[7]).center(tw_l) + '\n')
        activeMesgFd.close()

