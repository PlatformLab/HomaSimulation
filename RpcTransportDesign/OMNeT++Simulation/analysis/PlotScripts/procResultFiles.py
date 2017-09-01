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
sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)) + '/..')

from parseResultFiles import *
from MetricsDashBoard import *

if __name__ == '__main__':
    parser = OptionParser(description='This scripts takes in a fileList file'
        ' name argument containing homa\'s scalar result files filenames, one'
        ' per each line. Computes the wasted bandwidth and/or priority usages'
        ' from all of those result files and dump results in outputFile'
        ' argument. The output file format is tabulated similar to R dataframe'
        ' format.')

    parser.add_option('--fileList', metavar='FILENAME', default = '',
        dest='fileList', help='name of file, containing scalar result'
        ' files name, one item per line')
    parser.add_option('--outputType', metavar='wp', default = '',
        dest='outputType', help="Any combination of 'w', 'p'. 'w' for wastedBw,"
        " 'p' for priority usage percentage.")

    options, args = parser.parse_args()
    fileList = options.fileList
    if fileList == '':
        sys.exit('argument --fileList is required!')

    outputType = AttrDict({'wastedBw':False, 'prioUsage':False})
    hasValidType = False
    for char in options.outputType:
        if char=='w':
            outputType.wastedBw = True
            hasValidType = True
            wastedBwFile = 'wastedBw.txt'
        if char=='p':
            hasValidType = True
            outputType.prioUsage = True
            prioUsageFile = 'prioUsage.txt'

    if not(hasValidType):
        sys.exit('Enter a valid combination for'' --outputType command line'
        ' option!')

    # open file list and read the result files
    absPath = os.path.abspath(fileList)
    absDir = os.path.dirname(absPath)
    f = open(absPath)
    resultFiles = [line.rstrip('\n') for line in f]
    f.close()

    # compute output for result files
    wastedBw = []
    prioTimeUsagesPct = []
    for dirFile in resultFiles:
        filename = os.path.join(absDir, dirFile)
        print filename
        xmlConfigFile = os.environ['HOME'] + '/Research/RpcTransportDesign/'\
            'OMNeT++Simulation/homatransport/src/dcntopo/config.xml'

        sp = ScalarParser(filename)
        parsedStats = AttrDict()
        parsedStats.hosts = sp.hosts
        parsedStats.tors = sp.tors
        parsedStats.aggrs = sp.aggrs
        parsedStats.cores = sp.cores
        parsedStats.generalInfo = sp.generalInfo
        parsedStats.globalListener = sp.globalListener
        xmlParsedDic = parseXmlFile(xmlConfigFile, parsedStats.generalInfo)
        trafficDic = computeBytesAndRates(parsedStats, xmlParsedDic)
        digestTrafficInfo(trafficDic.rxHostsTraffic.nics.rx, 'RX NICs Recv:')

        redundancyFactor = parsedStats.generalInfo['numSendersToKeepGranted']
        workloadType =  parsedStats.generalInfo['workloadType']
        nominalLoadFactor = float(parsedStats.generalInfo.rlf)*100
        loadFactor =  100 *\
            trafficDic.rxHostsTraffic.nics.rx.trafficDigest.avgRate /\
            int(parsedStats.generalInfo.nicLinkSpeed.strip('Gbps'))
        prioLevels = int(eval(parsedStats.generalInfo.prioLevels))

        if outputType.prioUsage:
            prioUsageStatsDigest = []
            computePrioUsageStats(parsedStats.hosts, parsedStats.generalInfo,
                xmlParsedDic, prioUsageStatsDigest)
            adaptiveSchedPrioLevels = int(eval(
                parsedStats.generalInfo.adaptiveSchedPrioLevels))
            for prioStats in prioUsageStatsDigest:
                priority = prioStats.priority
                ##record results only for scheduled priorities
                #if priority < prioLevels-adaptiveSchedPrioLevels:
                #    continue
                usageTimePct = prioStats.usageTimePct
                prioTimeUsagesPct.append((prioLevels, adaptiveSchedPrioLevels,
                    workloadType, redundancyFactor, loadFactor,
                    nominalLoadFactor, prioLevels-priority,
                    usageTimePct.activelyRecv, usageTimePct.activelyRecvSched))

        if outputType.wastedBw:
            activeAndWasted = computeWastedTimesAndBw(parsedStats, xmlParsedDic)
            selfWasteTime = computeSelfInflictedWastedBw(parsedStats, xmlParsedDic)
            wastedBw.append(('Homa', workloadType, prioLevels, redundancyFactor,
            loadFactor, nominalLoadFactor, activeAndWasted.sx.fracTotalTime,
            activeAndWasted.sx.fracActiveTime, activeAndWasted.rx.fracTotalTime,
            activeAndWasted.rx.fracActiveTime,
            activeAndWasted.rx.oversubWastedFracTotalTime,
            activeAndWasted.rx.oversubWastedFracOversubTime,
            selfWasteTime.highrOverEst.fracTotalTime*100.0))


    tw_h = 40
    tw_l = 40

    if outputType.prioUsage:
        # create output file
        fdPrioUsage = open(os.environ['HOME'] + "/Research/RpcTransportDesign" +
            "/OMNeT++Simulation/analysis/PlotScripts/" + prioUsageFile, 'w')
        fdPrioUsage.write('prioLevels'.center(tw_l) +
            'adaptivePrioLevels'.center(tw_l) + 'workload'.center(tw_l) +
            'redundancyFactor'.center(tw_h) + 'loadFactor'.center(tw_l) +
            'nominalLoadFactor'.center(tw_l) + 'priority'.center(tw_l) +
            'prioUsagePct_activelyRecv'.center(tw_l) +
            'prioUsagePct_activelyRecvSched'.center(tw_l) + '\n')

        # dump the results in outputfiles
        for prioUsage in prioTimeUsagesPct:
            fdPrioUsage.write(
                '{0}'.format(prioUsage[0]).center(tw_l) +
                '{0}'.format(prioUsage[1]).center(tw_h) +
                '{0}'.format(prioUsage[2]).center(tw_l) +
                '{0}'.format(prioUsage[3]).center(tw_l) +
                '{0}'.format(prioUsage[4]).center(tw_l) +
                '{0}'.format(prioUsage[5]).center(tw_l) +
                '{0}'.format(prioUsage[6]).center(tw_l) +
                '{0}'.format(prioUsage[7]).center(tw_l) +
                '{0}'.format(prioUsage[8]).center(tw_l) + '\n')
        fdPrioUsage.close()

    if outputType.wastedBw:
        # create output file
        fdWasted = open(os.environ['HOME'] + "/Research/RpcTransportDesign" +
            "/OMNeT++Simulation/analysis/PlotScripts/" + wastedBwFile, 'w')
        fdWasted.write('transport'.center(tw_h) + 'workload'.center(tw_h) +
            'prioLevels'.center(tw_l) + 'redundancyFactor'.center(tw_l) +
            'loadFactor'.center(tw_l) + 'nominalLoadFactor'.center(tw_l) +
            'sxWastedBw'.center(tw_l) + 'sxActiveWastedBw'.center(tw_l) +
            'wastedBw'.center(tw_l) + 'activeWastedBw'.center(tw_l) +
            'totalWastedOversubBw'.center(tw_l) +
            'oversubWastedOversubBw'.center(tw_l) +
            'selfInflictedWasteBw' + '\n')
        # dump the results in outputfiles
        for wasteTuple in wastedBw:
            fdWasted.write(
                '{0}'.format(wasteTuple[0]).center(tw_l) +
                '{0}'.format(wasteTuple[1]).center(tw_h) +
                '{0}'.format(wasteTuple[2]).center(tw_l) +
                '{0}'.format(wasteTuple[3]).center(tw_l) +
                '{0}'.format(wasteTuple[4]).center(tw_l) +
                '{0}'.format(wasteTuple[5]).center(tw_l) +
                '{0}'.format(wasteTuple[6]).center(tw_l) +
                '{0}'.format(wasteTuple[7]).center(tw_l) +
                '{0}'.format(wasteTuple[8]).center(tw_l) +
                '{0}'.format(wasteTuple[9]).center(tw_l) +
                '{0}'.format(wasteTuple[10]).center(tw_l) +
                '{0}'.format(wasteTuple[11]).center(tw_l) +
                '{0}'.format(wasteTuple[12]).center(tw_l) + '\n')
        fdWasted.close()

