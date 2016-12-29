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
sys.path.insert(0, os.environ['HOME'] + '/Research/RpcTransportDesign/OMNeT++Simulation/analysis')
from parseResultFiles import *
from MetricsDashBoard import *

if __name__ == '__main__':
    parser = OptionParser(description='This scripts takes in a fileList file'
        ' name argument containing homa\'s scalar result files filenames, one'
        ' per each line. Computes the wasted bandwidth from all of those result'
        ' files and dump results in outputFile argument. The output file format'
        ' is tabulated similar to R dataframe format.')

    parser.add_option('--fileList', metavar='FILENAME',
            default = '', dest='fileList',
            help='name of file, containing scalar result files name, one item'
            ' per line')
    parser.add_option('--ouputFile', metavar='FILENAME', default = 'wastedBw.txt',
            dest='outputFile',
            help='name of output file for computation results of this script.')
    
    options, args = parser.parse_args()
    fileList = options.fileList
    outputFile = options.outputFile

    if fileList == '':
        sys.exit('argument --fileList is required!')
    absPath = os.path.abspath(fileList)
    absDir = os.path.dirname(absPath)
    f = open(absPath)
    resultFiles = [line.rstrip('\n') for line in f]
    f.close()
    f = open(os.environ['HOME'] + "/Research/RpcTransportDesign" +
        "/OMNeT++Simulation/analysis/PlotScripts/" + outputFile, 'w')
    tw_h = 40
    tw_l = 40
    f.write('workload'.center(tw_l) + 'redundancyFactor'.center(tw_h) +
        'loadFactor'.center(tw_l) + 'wastedBw'.center(tw_l) + 'activeWastedBw'.center(tw_l)+'\n')

    wastedBw = []
    for dirFile in resultFiles:
        filename = os.path.join(absDir, dirFile)
        print filename
        sp = ScalarParser(filename) 
        parsedStats = AttrDict()
        parsedStats.hosts = sp.hosts
        parsedStats.tors = sp.tors
        parsedStats.aggrs = sp.aggrs
        parsedStats.cores = sp.cores
        parsedStats.generalInfo = sp.generalInfo
        redundancyFactor = parsedStats.generalInfo['numSendersToKeepGranted'] 
        workloadType =  parsedStats.generalInfo['workloadType']
        nominalLoadFactor = float(parsedStats.generalInfo.rlf)*100
        xmlConfigFile =  os.environ['HOME'] + '/Research/RpcTransportDesign/OMNeT++Simulation/homatransport/src/dcntopo/config.xml'
        xmlParsedDic = parseXmlFile(xmlConfigFile, parsedStats.generalInfo)
        activeAndWasted = computeWastedTimesAndBw(parsedStats, xmlParsedDic)
        trafficDic = computeBytesAndRates(parsedStats, xmlParsedDic)
        digestTrafficInfo(trafficDic.rxHostsTraffic.nics.rx, 'RX NICs Recv:')
        loadFactor =  100* trafficDic.rxHostsTraffic.nics.rx.trafficDigest.avgRate/ int(parsedStats.generalInfo.nicLinkSpeed.strip('Gbps')) 
        nominalLoadFactor = float(parsedStats.generalInfo.rlf)*100
        wastedBw.append((workloadType, redundancyFactor, loadFactor, activeAndWasted.rx.fracTotalTime, activeAndWasted.rx.fracActiveTime))

    for wasteTuple in wastedBw:
        f.write('{0}'.format(wasteTuple[0]).center(tw_l) + '{0}'.format(wasteTuple[1]).center(tw_h) +
                '{0}'.format(wasteTuple[2]).center(tw_l) + '{0}'.format(wasteTuple[3]).center(tw_l) + '{0}'.format(wasteTuple[4]).center(tw_l)+ '\n')

    f.close()
