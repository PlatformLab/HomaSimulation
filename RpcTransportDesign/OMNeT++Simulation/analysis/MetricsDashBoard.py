#!/usr/bin/python
"""
This program scans the scaler result file (.sca) and printouts some of the
statistics on the screen.
"""

from numpy import *
from glob import glob
from optparse import OptionParser
from pprint import pprint
from functools import partial
from xml.dom import minidom
import math
import os
import random
import re
import sys
import warnings

sys.path.insert(0, os.environ['HOME'] + '/Research/RpcTransportDesign/OMNeT++Simulation/analysis')
from parseResultFiles import *

def copyExclude(source, dest, exclude):
    selectKeys = (key for key in source if key not in exclude)
    for key in selectKeys:
        if (isinstance(source[key], AttrDict)):
            dest[key] = AttrDict()
            copyExclude(source[key], dest[key], exclude)
        else:
            dest[key] = source[key]

def getStatsFromHist(bins, cumProb, idx):
    if idx == 0 and bins[idx] == -inf:
        return bins[idx + 1]
    return bins[idx]

def getInterestingModuleStats(moduleDic, statsKey, histogramKey):
    moduleStats = AttrDict()
    moduleStats = moduleStats.fromkeys(['count','min','mean','stddev','max','median','threeQuartile','ninety9Percentile'], 0.0)
    histogram = moduleDic.access(histogramKey)
    stats = moduleDic.access(statsKey)
    bins = [tuple[0] for tuple in histogram]
    if stats.count != 0:
        cumProb = cumsum([tuple[1]/stats.count for tuple in histogram])
        moduleStats.count = stats.count
        moduleStats.min = stats.min
        moduleStats.mean = stats.mean
        moduleStats.stddev = stats.stddev
        moduleStats.max = stats.max
        medianIdx = next(idx for idx,value in enumerate(cumProb) if value >= 0.5)
        moduleStats.median = max(getStatsFromHist(bins, cumProb, medianIdx), stats.min)
        threeQuartileIdx = next(idx for idx,value in enumerate(cumProb) if value >= 0.75)
        moduleStats.threeQuartile = max(getStatsFromHist(bins, cumProb, threeQuartileIdx), stats.min)
        ninety9PercentileIdx = next(idx for idx,value in enumerate(cumProb) if value >= 0.99)
        moduleStats.ninety9Percentile = max(getStatsFromHist(bins, cumProb, ninety9PercentileIdx), stats.min)
    return moduleStats

def digestModulesStats(modulesStatsList):
    statsDigest = AttrDict()
    if len(modulesStatsList) > 0:
        statsDigest = statsDigest.fromkeys(modulesStatsList[0].keys(), 0.0)
        statsDigest.min = inf
        for targetStat in modulesStatsList:
            statsDigest.count += targetStat.count
            statsDigest.min = min(targetStat.min, statsDigest.min)
            statsDigest.max = max(targetStat.max, statsDigest.max)
            statsDigest.mean += targetStat.mean * targetStat.count
            statsDigest.stddev += targetStat.stddev * targetStat.count
            statsDigest.median += targetStat.median * targetStat.count
            statsDigest.threeQuartile += targetStat.threeQuartile  * targetStat.count
            statsDigest.ninety9Percentile += targetStat.ninety9Percentile  * targetStat.count

        divideNoneZero = lambda stats, count: stats * 1.0/count if count !=0 else 0.0
        statsDigest.mean = divideNoneZero(statsDigest.mean, statsDigest.count)
        statsDigest.stddev = divideNoneZero(statsDigest.stddev, statsDigest.count)
        statsDigest.median = divideNoneZero(statsDigest.median, statsDigest.count)
        statsDigest.threeQuartile = divideNoneZero(statsDigest.threeQuartile, statsDigest.count)
        statsDigest.ninety9Percentile = divideNoneZero(statsDigest.ninety9Percentile, statsDigest.count)
    else:
        statsDigest.count = 0
        statsDigest.min = inf
        statsDigest.max = 0
        statsDigest.mean = 0
        statsDigest.stddev = 0
        statsDigest.median = 0
        statsDigest.threeQuartile = 0
        statsDigest.ninety9Percentile = 0

    return statsDigest


def hostQueueWaitTimes(hosts, xmlParsedDic, reportDigest):
    senderIds = xmlParsedDic.senderIds
    # find the queueWaitTimes for different types of packets. Current types
    # considered are request, grant and data packets. Also queueingTimes in the
    # senders NIC.
    keyStrings = ['queueingTime','unschedDataQueueingTime','schedDataQueueingTime','grantQueueingTime','requestQueueingTime']
    for keyString in keyStrings:
        queuingTimeStats = list()
        for host in hosts.keys():
            hostId = int(re.match('host\[([0-9]+)]', host).group(1))
            queuingTimeHistogramKey = 'host[{0}].eth[0].queue.dataQueue.{1}:histogram.bins'.format(hostId, keyString)
            queuingTimeStatsKey = 'host[{0}].eth[0].queue.dataQueue.{1}:stats'.format(hostId,keyString)
            hostStats = AttrDict()
            if keyString != 'queueingTime' or (keyString == 'queueingTime' and hostId in senderIds):
                hostStats = getInterestingModuleStats(hosts, queuingTimeStatsKey, queuingTimeHistogramKey)
                queuingTimeStats.append(hostStats)

        queuingTimeDigest = AttrDict()
        queuingTimeDigest = digestModulesStats(queuingTimeStats)
        reportDigest.assign('queueWaitTime.hosts.{0}'.format(keyString), queuingTimeDigest)

def torsQueueWaitTime(tors, generalInfo, xmlParsedDic, reportDigest):
    numServersPerTor = int(generalInfo.numServersPerTor)
    fabricLinkSpeed = int(generalInfo.fabricLinkSpeed.strip('Gbps'))
    nicLinkSpeed = int(generalInfo.nicLinkSpeed.strip('Gbps'))
    senderHostIds = xmlParsedDic.senderIds
    senderTorIds = [elem for elem in set([int(id / numServersPerTor) for id in senderHostIds])]
    numTorUplinkNics = int(floor(numServersPerTor * nicLinkSpeed / fabricLinkSpeed))
    receiverHostIds = xmlParsedDic.receiverIds
    receiverTorIdsIfaces = [(int(id / numServersPerTor), id % numServersPerTor) for id in receiverHostIds]
    keyStrings = ['queueingTime','unschedDataQueueingTime','schedDataQueueingTime','grantQueueingTime','requestQueueingTime']
    for keyString in keyStrings:
        torsUpwardQueuingTimeStats = list()
        torsDownwardQueuingTimeStats = list()
        for torKey in tors.keys():
            torId = int(re.match('tor\[([0-9]+)]', torKey).group(1))
            tor = tors[torKey]
            # Find the queue waiting times for the upward NICs of sender tors
            # as well as the queue waiting times for various packet types.
            # For the first one we have to find torIds for all the tors
            # connected to the sender hosts
            for ifaceId in range(numServersPerTor, numServersPerTor + numTorUplinkNics):
                # Find the queuewait time only for the upward tor NICs
                queuingTimeHistogramKey = 'eth[{0}].queue.dataQueue.{1}:histogram.bins'.format(ifaceId, keyString)
                queuingTimeStatsKey = 'eth[{0}].queue.dataQueue.{1}:stats'.format(ifaceId, keyString)
                if keyString != 'queueingTime' or (keyString == 'queueingTime' and torId in senderTorIds):
                    torUpwardStat = AttrDict()
                    torUpwardStat = getInterestingModuleStats(tor, queuingTimeStatsKey, queuingTimeHistogramKey)
                    torsUpwardQueuingTimeStats.append(torUpwardStat)

            # Find the queue waiting times for the downward NICs of receiver tors
            # as well as the queue waiting times for various packet types.
            # For the first one we have to find torIds for all the tors
            # connected to the receiver hosts
            for ifaceId in range(0, numServersPerTor):
                # Find the queuewait time only for the downward tor NICs
                queuingTimeHistogramKey = 'eth[{0}].queue.dataQueue.{1}:histogram.bins'.format(ifaceId, keyString)
                queuingTimeStatsKey = 'eth[{0}].queue.dataQueue.{1}:stats'.format(ifaceId, keyString)
                if keyString != 'queueingTime' or (keyString == 'queueingTime' and (torId, ifaceId) in receiverTorIdsIfaces):
                    torDownwardStat = AttrDict()
                    torDownwardStat = getInterestingModuleStats(tor, queuingTimeStatsKey, queuingTimeHistogramKey)
                    torsDownwardQueuingTimeStats.append(torDownwardStat)

        torsUpwardQueuingTimeDigest = AttrDict()
        torsUpwardQueuingTimeDigest = digestModulesStats(torsUpwardQueuingTimeStats)
        reportDigest.assign('queueWaitTime.tors.upward.{0}'.format(keyString), torsUpwardQueuingTimeDigest)

        torsDownwardQueuingTimeDigest = AttrDict()
        torsDownwardQueuingTimeDigest = digestModulesStats(torsDownwardQueuingTimeStats)
        reportDigest.assign('queueWaitTime.tors.downward.{0}'.format(keyString), torsDownwardQueuingTimeDigest)

def aggrsQueueWaitTime(aggrs, generalInfo, xmlParsedDic, reportDigest):
    # Find the queue waiting for aggrs switches NICs
    keyStrings = ['queueingTime','unschedDataQueueingTime','schedDataQueueingTime','grantQueueingTime','requestQueueingTime']
    for keyString in keyStrings:
        aggrsQueuingTimeStats = list()
        for aggr in aggrs.keys():
            for ifaceId in range(0, int(generalInfo.numTors)):
                queuingTimeHistogramKey = '{0}.eth[{1}].queue.dataQueue.{2}:histogram.bins'.format(aggr, ifaceId,  keyString)
                queuingTimeStatsKey = '{0}.eth[{1}].queue.dataQueue.{2}:stats'.format(aggr, ifaceId, keyString)
                aggrsStats = AttrDict()
                aggrsStats = getInterestingModuleStats(aggrs, queuingTimeStatsKey, queuingTimeHistogramKey)
                aggrsQueuingTimeStats.append(aggrsStats)

        aggrsQueuingTimeDigest = AttrDict()
        aggrsQueuingTimeDigest = digestModulesStats(aggrsQueuingTimeStats)
        reportDigest.assign('queueWaitTime.aggrs.{0}'.format(keyString), aggrsQueuingTimeDigest)

def parseXmlFile(xmlConfigFile, generalInfo):
    xmlConfig = minidom.parse(xmlConfigFile)
    xmlParsedDic = AttrDict()

    senderIds = list()
    receiverIds = list()
    allHostsReceive = False
    for hostConfig in xmlConfig.getElementsByTagName('hostConfig'):
        isSender = hostConfig.getElementsByTagName('isSender')[0]
        if isSender.childNodes[0].data == 'true':
            hostId = int(hostConfig.getAttribute('id'))
            senderIds.append(hostId)
            if allHostsReceive is False:
                destIdsNode = hostConfig.getElementsByTagName('destIds')[0]
                destIds = list()
                if destIdsNode.firstChild != None:
                    destIds = [int(destId) for destId in destIdsNode.firstChild.data.split()]
                if destIds == []:
                    allHostsReceive = True
                elif -1 in destIds:
                    receiverIds += [idx for idx in range(0, int(generalInfo.numTors)*int(generalInfo.numServersPerTor)) if idx != hostId]
                else:
                    receiverIds += destIds
    xmlParsedDic.senderIds = senderIds
    if allHostsReceive is True:
        receiverIds = range(0, int(generalInfo.numTors)*int(generalInfo.numServersPerTor))
    xmlParsedDic.receiverIds = [elem for elem in set(receiverIds)]
    return xmlParsedDic

def printStatsLine(statsDic, rowTitle, tw, fw, unit, printKeys):
    if unit == 'us':
        scaleFac = 1e6
    elif unit == 'KB':
        scaleFac = 2**-10
    elif unit == '':
        scaleFac = 1

    printStr = rowTitle.ljust(tw)
    for key in printKeys:
        if key in statsDic.keys():
            if key == 'count':
                printStr += '{0}'.format(int(statsDic.access(key))).center(fw)
            elif key in ['cntPercent', 'bytesPercent']:
                printStr += '{0:.2f}'.format(statsDic.access(key)).center(fw)
            elif key == 'meanFrac':
                printStr += '{0:.2f}'.format(statsDic.access(key)).center(fw)
            elif key == 'bytes' and unit != 'KB':
                printStr += '{0:.0f}'.format(statsDic.access(key)).center(fw)
            else:
                printStr += '{0:.2f}'.format(statsDic.access(key) * scaleFac).center(fw)
    print(printStr)

def printQueueTimeStats(queueWaitTimeDigest, unit):

    printKeys = ['mean', 'meanFrac', 'stddev', 'min', 'median', 'threeQuartile', 'ninety9Percentile', 'max', 'count']
    tw = 20
    fw = 9
    lineMax = 100
    title = 'Queue Wait Time Stats'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))

    print('\n' + "Packet Type: Requst".center(lineMax,' ') + '\n' + "="*lineMax)
    print("Queue Location".ljust(tw) + 'mean'.format(unit).center(fw) + 'mean'.center(fw) + 'stddev'.format(unit).center(fw) +
            'min'.format(unit).center(fw) + 'median'.format(unit).center(fw) + '75%ile'.format(unit).center(fw) +
            '99%ile'.format(unit).center(fw) + 'max'.format(unit).center(fw) + 'count'.center(fw))
    print("".ljust(tw) + '({0})'.format(unit).center(fw) + '(%)'.center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + ''.center(fw))

    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.requestQueueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.requestQueueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.requestQueueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.requestQueueingTime
    meanSum = hostStats.mean + torsUpStats.mean + torsDownStats.mean + aggrsStats.mean
    meanFracSum = 0.0
    for moduleStats in [hostStats, torsUpStats, torsDownStats, aggrsStats]:
        moduleStats.meanFrac = 0 if meanSum==0 else 100*moduleStats.mean/meanSum
        meanFracSum += moduleStats.meanFrac

    printStatsLine(hostStats, 'Host NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsUpStats, 'TORs upward NICs:', tw, fw, unit, printKeys)
    printStatsLine(aggrsStats, 'Aggr Switch NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsDownStats, 'TORs downward NICs:', tw, fw, unit, printKeys)
    print('_'*2*tw + '\n' + 'Total:'.ljust(tw) + '{0:.2f}'.format(meanSum*1e6).center(fw) + '{0:.2f}'.format(meanFracSum).center(fw))

    print('\n\n' + "Packet Type: Unsched. Data".center(lineMax,' ') + '\n'  + "="*lineMax)
    print("Queue Location".ljust(tw) + 'mean'.format(unit).center(fw) + 'mean'.center(fw) + 'stddev'.format(unit).center(fw) +
            'min'.format(unit).center(fw) + 'median'.format(unit).center(fw) + '75%ile'.format(unit).center(fw) +
            '99%ile'.format(unit).center(fw) + 'max'.format(unit).center(fw) + 'count'.center(fw))
    print("".ljust(tw) + '({0})'.format(unit).center(fw) + '(%)'.center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + ''.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.unschedDataQueueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.unschedDataQueueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.unschedDataQueueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.unschedDataQueueingTime
    if hostStats != {}:
        meanSum = hostStats.mean + torsUpStats.mean + torsDownStats.mean + aggrsStats.mean
        meanFracSum = 0.0
        for moduleStats in [hostStats, torsUpStats, torsDownStats, aggrsStats]:
            moduleStats.meanFrac = 0 if meanSum==0 else 100*moduleStats.mean/meanSum
            meanFracSum += moduleStats.meanFrac

        printStatsLine(hostStats, 'Host NICs:', tw, fw, unit, printKeys)
        printStatsLine(torsUpStats, 'TORs upward NICs:', tw, fw, unit, printKeys)
        printStatsLine(aggrsStats, 'Aggr Switch NICs:', tw, fw, unit, printKeys)
        printStatsLine(torsDownStats, 'TORs downward NICs:', tw, fw, unit, printKeys)
        print('_'*2*tw + '\n' + 'Total'.ljust(tw) + '{0:.2f}'.format(meanSum*1e6).center(fw) + '{0:.2f}'.format(meanFracSum).center(fw))


    print('\n\n' + "Packet Type: Grant".center(lineMax,' ') + '\n' + "="*lineMax)
    print("Queue Location".ljust(tw) + 'mean'.format(unit).center(fw) + 'mean'.center(fw) + 'stddev'.format(unit).center(fw) +
            'min'.format(unit).center(fw) + 'median'.format(unit).center(fw) + '75%ile'.format(unit).center(fw) +
            '99%ile'.format(unit).center(fw) + 'max'.format(unit).center(fw) + 'count'.center(fw))
    print("".ljust(tw) + '({0})'.format(unit).center(fw) + '(%)'.center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + ''.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.grantQueueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.grantQueueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.grantQueueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.grantQueueingTime
    meanSum = hostStats.mean + torsUpStats.mean + torsDownStats.mean + aggrsStats.mean
    meanFracSum = 0.0
    for moduleStats in [hostStats, torsUpStats, torsDownStats, aggrsStats]:
        moduleStats.meanFrac = 0 if meanSum==0 else 100*moduleStats.mean/meanSum
        meanFracSum += moduleStats.meanFrac


    printStatsLine(hostStats, 'Host NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsUpStats, 'TORs upward NICs:', tw, fw, unit, printKeys)
    printStatsLine(aggrsStats, 'Aggr Switch NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsDownStats, 'TORs downward NICs:', tw, fw, unit, printKeys)
    print('_'*2*tw + '\n' + 'Total:'.ljust(tw) + '{0:.2f}'.format(meanSum*1e6).center(fw) + '{0:.2f}'.format(meanFracSum).center(fw))

    print('\n\n' + "Packet Type: Sched. Data".center(lineMax,' ') + '\n'  + "="*lineMax)
    print("Queue Location".ljust(tw) + 'mean'.format(unit).center(fw) + 'mean'.center(fw) + 'stddev'.format(unit).center(fw) +
            'min'.format(unit).center(fw) + 'median'.format(unit).center(fw) + '75%ile'.format(unit).center(fw) +
            '99%ile'.format(unit).center(fw) + 'max'.format(unit).center(fw) + 'count'.center(fw))
    print("".ljust(tw) + '({0})'.format(unit).center(fw) + '(%)'.center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + ''.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.schedDataQueueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.schedDataQueueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.schedDataQueueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.schedDataQueueingTime
    meanSum = hostStats.mean + torsUpStats.mean + torsDownStats.mean + aggrsStats.mean
    meanFracSum = 0.0
    for moduleStats in [hostStats, torsUpStats, torsDownStats, aggrsStats]:
        moduleStats.meanFrac = 0 if meanSum==0 else 100*moduleStats.mean/meanSum
        meanFracSum += moduleStats.meanFrac

    printStatsLine(hostStats, 'Host NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsUpStats, 'TORs upward NICs:', tw, fw, unit, printKeys)
    printStatsLine(aggrsStats, 'Aggr Switch NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsDownStats, 'TORs downward NICs:', tw, fw, unit, printKeys)
    print('_'*2*tw + '\n' + 'Total'.ljust(tw) + '{0:.2f}'.format(meanSum*1e6).center(fw) + '{0:.2f}'.format(meanFracSum).center(fw))

    print('\n\n' + "packet Type: All Pkts".center(lineMax,' ') + '\n' + "="*lineMax)
    print("Queue Location".ljust(tw) + 'mean'.format(unit).center(fw) + 'mean'.center(fw) + 'stddev'.format(unit).center(fw) +
            'min'.format(unit).center(fw) + 'median'.format(unit).center(fw) + '75%ile'.format(unit).center(fw) +
            '99%ile'.format(unit).center(fw) + 'max'.format(unit).center(fw) + 'count'.center(fw))
    print("".ljust(tw) + '({0})'.format(unit).center(fw) + '(%)'.center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + ''.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.queueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.queueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.queueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.queueingTime
    meanSum = hostStats.mean + torsUpStats.mean + torsDownStats.mean + aggrsStats.mean
    meanFracSum = 0.0
    for moduleStats in [hostStats, torsUpStats, torsDownStats, aggrsStats]:
        moduleStats.meanFrac = 0 if meanSum==0 else 100*moduleStats.mean/meanSum
        meanFracSum += moduleStats.meanFrac

    printStatsLine(hostStats, 'SX Host NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsUpStats, 'SX TORs UP NICs:', tw, fw, unit, printKeys)
    printStatsLine(aggrsStats, 'Aggr Switch NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsDownStats, 'RX TORs Down NICs:', tw, fw, unit, printKeys)
    print('_'*2*tw + '\n' + 'Total'.ljust(tw) + '{0:.2f}'.format(meanSum*1e6).center(fw) + '{0:.2f}'.format(meanFracSum).center(fw))

def printE2EStretchAndDelay(e2eStretchAndDelayDigest, unit):
    printKeys = ['mean', 'meanFrac', 'stddev', 'min', 'median', 'threeQuartile', 'ninety9Percentile', 'max', 'count', 'cntPercent', 'bytes', 'bytesPercent']
    tw = 19
    fw = 10
    lineMax = 130
    title = 'End To End Message Latency For Different Ranges of Message Sizes'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))

    print("="*lineMax)
    print("Msg Size Range".ljust(tw) + 'mean'.center(fw) + 'stddev'.center(fw) + 'min'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) +
            'max'.center(fw) + 'count'.center(fw) + 'count'.center(fw) + 'bytes'.center(fw) + 'bytes'.center(fw))
    print("".ljust(tw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + ''.center(fw) + '(%)'.center(fw) + '(KB)'.center(fw) + '(%)'.center(fw))

    print("_"*lineMax)

    end2EndDelayDigest = e2eStretchAndDelayDigest.latency
    sizeLowBound = ['0'] + [e2eSizedDelay.sizeUpBound for e2eSizedDelay in end2EndDelayDigest[0:len(end2EndDelayDigest)-1]]
    for i, e2eSizedDelay in enumerate(end2EndDelayDigest):
        printStatsLine(e2eSizedDelay, '({0}, {1}]'.format(sizeLowBound[i], e2eSizedDelay.sizeUpBound), tw, fw, 'us', printKeys)

    title = 'Total Queue Delay (ie. real_e2e_latency - ideal_e2e_latency) For Different Ranges of Message Sizes'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))

    print("="*lineMax)
    print("Msg Size Range".ljust(tw) + 'mean'.center(fw) + 'stddev'.center(fw) + 'min'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) +
            'max'.center(fw) + 'count'.center(fw) + 'count'.center(fw) + 'bytes'.center(fw) + 'bytes'.center(fw))
    print("".ljust(tw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + ''.center(fw) + '(%)'.center(fw) + '(KB)'.center(fw) + '(%)'.center(fw))

    print("_"*lineMax)

    queuingDelayDigest = e2eStretchAndDelayDigest.delay
    sizeLowBound = ['0'] + [queuingSizedDelay.sizeUpBound for queuingSizedDelay in queuingDelayDigest[0:len(queuingDelayDigest)-1]]
    for i, queuingSizedDelay in enumerate(queuingDelayDigest):
        printStatsLine(queuingSizedDelay, '({0}, {1}]'.format(sizeLowBound[i], queuingSizedDelay.sizeUpBound), tw, fw, 'us', printKeys)

    title = 'End To End Message Stretch For Different Ranges of Message Sizes'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))

    print("="*lineMax)
    print("Msg Size Range".ljust(tw) + 'mean'.center(fw) + 'stddev'.center(fw) + 'min'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) + 'max'.center(fw) +
            'count'.center(fw) + 'count%'.center(fw) + 'bytes(KB)'.center(fw) + 'bytes%'.center(fw))
    print("_"*lineMax)

    end2EndStretchDigest = e2eStretchAndDelayDigest.stretch
    sizeLowBound = ['0'] + [e2eSizedStretch.sizeUpBound for e2eSizedStretch in end2EndStretchDigest[0:len(end2EndStretchDigest)-1]]
    for i, e2eSizedStretch in enumerate(end2EndStretchDigest):
        printStatsLine(e2eSizedStretch, '({0}, {1}]'.format(sizeLowBound[i], e2eSizedStretch.sizeUpBound), tw, fw, '', printKeys)

def msgBytesOnWire(parsedStats, xmlParsedDic, msgBytesOnWireDigest):
    if 'globalListener' in parsedStats:
        return globalMesgBytesOnWire(parsedStats, xmlParsedDic,
            msgBytesOnWireDigest)

    hosts = parsedStats.hosts
    generalInfo = parsedStats.generalInfo
    receiverHostIds = xmlParsedDic.receiverIds
    sizes = generalInfo.msgSizeRanges.strip('\"').split(' ')[:]
    sizes.append('Huge')
    totalBytes = 0.0
    totalCnt = 0.0
    for size in sizes:
        bytesOnWire = AttrDict()
        bytesOnWire.bytes = 0.0
        bytesOnWire.cnt = 0.0
        for id in receiverHostIds:
            bytesStatsKey = 'host[{0}].trafficGeneratorApp[0].msg{1}BytesOnWire:stats'.format(id, size)
            bytesOnWire.cnt += int(hosts.access(bytesStatsKey + '.count'))
            totalCnt +=  int(hosts.access(bytesStatsKey + '.count'))
            bytesOnWire.bytes += int(hosts.access(bytesStatsKey + '.sum'))
            totalBytes += int(hosts.access(bytesStatsKey + '.sum'))

        msgBytesOnWireDigest['{0}'.format(size)] = bytesOnWire
    for size in msgBytesOnWireDigest.keys():
        msgBytesOnWireDigest[size].cntPercent = 100 * msgBytesOnWireDigest[size].cnt / totalCnt
        msgBytesOnWireDigest[size].bytesPercent = 100 * msgBytesOnWireDigest[size].bytes / totalBytes
    return

def globalMesgBytesOnWire(parsedStats, xmlParsedDic, msgBytesOnWireDigest):
    if not('globalListener' in parsedStats):
        msgBytesOnWireDigest.clear()
        return msgBytesOnWireDigest

    receiverHostIds = xmlParsedDic.receiverIds
    sizes = parsedStats.generalInfo.msgSizeRanges.strip('\"').split(' ')[:]
    #sizes.append('Huge')
    totalBytes = 0.0
    totalCnt = 0.0
    for size in sizes:
        bytesOnWire = AttrDict()
        bytesOnWire.bytes = 0.1
        bytesOnWire.cnt = 0.0

        bytesStatsKey = 'globalListener.mesg{0}BytesOnWire:histogram'.format(size)
        bytesOnWire.cnt += int(parsedStats.access(bytesStatsKey + '.count'))
        totalCnt +=  int(parsedStats.access(bytesStatsKey + '.count'))
        bytesOnWire.bytes += int(parsedStats.access(bytesStatsKey + '.sum'))
        totalBytes += int(parsedStats.access(bytesStatsKey + '.sum'))

        msgBytesOnWireDigest['{0}'.format(size)] = bytesOnWire

    for size in msgBytesOnWireDigest.keys():
        msgBytesOnWireDigest[size].cntPercent = 100 * msgBytesOnWireDigest[size].cnt / totalCnt
        msgBytesOnWireDigest[size].bytesPercent = 100 * msgBytesOnWireDigest[size].bytes / totalBytes

    #pprint(msgBytesOnWireDigest)
    return

def e2eStretchAndDelay(parsedStats, xmlParsedDic, msgBytesOnWireDigest, e2eStretchAndDelayDigest):
    if 'globalListener' in parsedStats:
        return globalE2eStretchAndDelay(parsedStats, xmlParsedDic,
            msgBytesOnWireDigest, e2eStretchAndDelayDigest)

    hosts = parsedStats.hosts
    generalInfo = parsedStats.generalInfo

    # For the hosts that are receivers, find the stretch and endToend stats and
    # return them.
    receiverHostIds = xmlParsedDic.receiverIds
    sizes = generalInfo.msgSizeRanges.strip('\"').split(' ')[:]
    sizes.append('Huge')

    e2eStretchAndDelayDigest.delay = []
    e2eStretchAndDelayDigest.latency = []
    e2eStretchAndDelayDigest.stretch = []
    for size in sizes:
        e2eDelayList = list()
        queuingDelayList = list()
        e2eStretchList = list()
        for id in receiverHostIds:
            e2eDelayHistogramKey = 'host[{0}].trafficGeneratorApp[0].msg{1}E2EDelay:histogram.bins'.format(id, size)
            e2eDelayStatsKey = 'host[{0}].trafficGeneratorApp[0].msg{1}E2EDelay:stats'.format(id, size)
            queuingDelayHistogramKey = 'host[{0}].trafficGeneratorApp[0].msg{1}QueuingDelay:histogram.bins'.format(id, size)
            queuingDelayStatsKey = 'host[{0}].trafficGeneratorApp[0].msg{1}QueuingDelay:stats'.format(id, size)
            e2eStretchHistogramKey = 'host[{0}].trafficGeneratorApp[0].msg{1}E2EStretch:histogram.bins'.format(id, size)
            e2eStretchStatsKey = 'host[{0}].trafficGeneratorApp[0].msg{1}E2EStretch:stats'.format(id, size)
            e2eDelayForSize = AttrDict()
            queuingDelayForSize = AttrDict()
            e2eStretchForSize = AttrDict()
            e2eDelayForSize = getInterestingModuleStats(hosts, e2eDelayStatsKey, e2eDelayHistogramKey)
            queuingDelayForSize = getInterestingModuleStats(hosts, queuingDelayStatsKey, queuingDelayHistogramKey)
            e2eStretchForSize = getInterestingModuleStats(hosts, e2eStretchStatsKey, e2eStretchHistogramKey)
            e2eDelayList.append(e2eDelayForSize)
            queuingDelayList.append(queuingDelayForSize)
            e2eStretchList.append(e2eStretchForSize)

        e2eDelayDigest = AttrDict()
        queuingDelayDigest = AttrDict()
        e2eStretchDigest = AttrDict()
        e2eDelayDigest = digestModulesStats(e2eDelayList)
        queuingDelayDigest = digestModulesStats(queuingDelayList)
        e2eStretchDigest = digestModulesStats(e2eStretchList)

        e2eDelayDigest.sizeUpBound = '{0}'.format(size)
        e2eDelayDigest.bytesPercent = msgBytesOnWireDigest[size].bytesPercent
        e2eDelayDigest.bytes = msgBytesOnWireDigest[size].bytes * 2**-10 #in KB
        e2eStretchAndDelayDigest.latency.append(e2eDelayDigest)
        queuingDelayDigest.sizeUpBound = '{0}'.format(size)
        queuingDelayDigest.bytesPercent = msgBytesOnWireDigest[size].bytesPercent
        queuingDelayDigest.bytes = msgBytesOnWireDigest[size].bytes * 2**-10 #in KB
        e2eStretchAndDelayDigest.delay.append(queuingDelayDigest)
        e2eStretchDigest.sizeUpBound = '{0}'.format(size)
        e2eStretchDigest.bytesPercent = msgBytesOnWireDigest[size].bytesPercent
        e2eStretchDigest.bytes = msgBytesOnWireDigest[size].bytes * 2**-10 #in KB
        e2eStretchAndDelayDigest.stretch.append(e2eStretchDigest)

    totalDelayCnt = sum([delay.count for delay in e2eStretchAndDelayDigest.delay])
    totalStretchCnt = sum([stretch.count for stretch in e2eStretchAndDelayDigest.stretch])
    totalLatencyCnt = sum([latency.count for latency in e2eStretchAndDelayDigest.latency])
    for sizedDelay, sizedStretch, sizedLatency in zip(e2eStretchAndDelayDigest.delay, e2eStretchAndDelayDigest.stretch, e2eStretchAndDelayDigest.latency):
        sizedDelay.cntPercent = sizedDelay.count * 100.0 / totalDelayCnt
        sizedStretch.cntPercent = sizedStretch.count * 100.0 / totalStretchCnt
        sizedLatency.cntPercent = sizedLatency.count * 100.0 / totalLatencyCnt

    return e2eStretchAndDelayDigest

def globalE2eStretchAndDelay(parsedStats, xmlParsedDic, msgBytesOnWireDigest, e2eStretchAndDelayDigest):
    if not('globalListener' in parsedStats):
        e2eStretchAndDelayDigest.clear()
        return e2eStretchAndDelayDigest

    sizes = parsedStats.generalInfo.msgSizeRanges.strip('\"').split(' ')[:]
    #sizes.append('Huge')
    e2eStretchAndDelayDigest.delay = []
    e2eStretchAndDelayDigest.latency = []
    e2eStretchAndDelayDigest.stretch = []

    for size in sizes:
        e2eDelayHistogramKey = 'globalListener.mesg{0}Delay:histogram.bins'.format(size)
        e2eDelayStatsKey = 'globalListener.mesg{0}Delay:histogram'.format(size)
        queuingDelayHistogramKey = 'globalListener.mesg{0}QueueDelay:histogram.bins'.format(size)
        queuingDelayStatsKey = 'globalListener.mesg{0}QueueDelay:histogram'.format(size)
        e2eStretchHistogramKey = 'globalListener.mesg{0}Stretch:histogram.bins'.format(size)
        e2eStretchStatsKey = 'globalListener.mesg{0}Stretch:histogram'.format(size)
        e2eDelay = getInterestingModuleStats(parsedStats, e2eDelayStatsKey, e2eDelayHistogramKey)
        queuingDelay = getInterestingModuleStats(parsedStats, queuingDelayStatsKey, queuingDelayHistogramKey)
        e2eStretch = getInterestingModuleStats(parsedStats, e2eStretchStatsKey, e2eStretchHistogramKey)

        e2eDelay.sizeUpBound = '{0}'.format(size)
        e2eDelay.bytesPercent = msgBytesOnWireDigest[size].bytesPercent
        e2eDelay.bytes = msgBytesOnWireDigest[size].bytes * 2**-10 #in KB
        e2eStretchAndDelayDigest.latency.append(e2eDelay)
        queuingDelay.sizeUpBound = '{0}'.format(size)
        queuingDelay.bytesPercent = msgBytesOnWireDigest[size].bytesPercent
        queuingDelay.bytes = msgBytesOnWireDigest[size].bytes * 2**-10 #in KB
        e2eStretchAndDelayDigest.delay.append(queuingDelay)
        e2eStretch.sizeUpBound = '{0}'.format(size)
        e2eStretch.bytesPercent = msgBytesOnWireDigest[size].bytesPercent
        e2eStretch.bytes = msgBytesOnWireDigest[size].bytes * 2**-10 #in KB
        e2eStretchAndDelayDigest.stretch.append(e2eStretch)

    totalDelayCnt = sum([delay.count for delay in e2eStretchAndDelayDigest.delay])
    totalStretchCnt = sum([stretch.count for stretch in e2eStretchAndDelayDigest.stretch])
    totalLatencyCnt = sum([latency.count for latency in e2eStretchAndDelayDigest.latency])
    for sizedDelay, sizedStretch, sizedLatency in zip(e2eStretchAndDelayDigest.delay, e2eStretchAndDelayDigest.stretch, e2eStretchAndDelayDigest.latency):
        sizedDelay.cntPercent = sizedDelay.count * 100.0 / totalDelayCnt
        sizedStretch.cntPercent = sizedStretch.count * 100.0 / totalStretchCnt
        sizedLatency.cntPercent = sizedLatency.count * 100.0 / totalLatencyCnt

    return e2eStretchAndDelayDigest

def printTransportSchedDelay(transportSchedDelayDigest, unit):
    printKeys = ['mean', 'meanFrac', 'stddev', 'min', 'median', 'threeQuartile', 'ninety9Percentile', 'max', 'count', 'cntPercent', 'bytes', 'bytesPercent']
    tw = 19
    fw = 10
    lineMax = 130
    title = 'Receiver Scheduler Overhead For Different Ranges of Message Sizes'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))

    print("="*lineMax)
    print("Msg Size Range".ljust(tw) + 'mean'.center(fw) + 'stddev'.center(fw) + 'min'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) +
            'max'.center(fw) + 'count'.center(fw) + 'count'.center(fw) + 'bytes'.center(fw) + 'bytes'.center(fw) )
    print("".ljust(tw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + ''.center(fw) + '(%)'.center(fw) + '(KB)'.center(fw) + '(%)'.center(fw))
    print("_"*lineMax)

    delays = transportSchedDelayDigest.totalDelay
    sizeLowBound = ['0'] + [delay.sizeUpBound for delay in delays[0:len(delays)-1]]
    for i, delay in enumerate(delays):
        printStatsLine(delay, '({0}, {1}]'.format(sizeLowBound[i], delay.sizeUpBound), tw, fw, 'us', printKeys)


def transportSchedDelay(parsedStats, xmlParsedDic, msgBytesOnWireDigest, transportSchedDelayDigest):
    if 'globalListener' in parsedStats:
        transportSchedDelayDigest.clear()
        return globalTransportSchedDelay(parsedStats, xmlParsedDic,
            msgBytesOnWireDigest, transportSchedDelayDigest)

    hosts = parsedStats.hosts
    generalInfo = parsedStats.generalInfo
    receiverHostIds = xmlParsedDic.receiverIds
    sizes = generalInfo.msgSizeRanges.strip('\"').split(' ')[:]
    sizes.append('Huge')
    transportSchedDelayDigest.totalDelay = []

    for size in sizes:
        delayList = list()
        for id in receiverHostIds:
            delayHistogramKey = 'host[{0}].trafficGeneratorApp[0].msg{1}TransportSchedDelay:histogram.bins'.format(id, size)
            delayStatsKey = 'host[{0}].trafficGeneratorApp[0].msg{1}TransportSchedDelay:stats'.format(id, size)
            delayForSize = AttrDict()
            delayForSize = getInterestingModuleStats(hosts, delayStatsKey, delayHistogramKey)
            delayList.append(delayForSize)

        delayDigest = AttrDict()
        delayDigest = digestModulesStats(delayList)
        delayDigest.sizeUpBound = '{0}'.format(size)
        delayDigest.bytesPercent = msgBytesOnWireDigest[size].bytesPercent
        delayDigest.bytes = msgBytesOnWireDigest[size].bytes * 2**-10 #in KB
        transportSchedDelayDigest.totalDelay.append(delayDigest)

    totalDelayCnt = sum([delay.count for delay in transportSchedDelayDigest.totalDelay])
    for sizedDelay in transportSchedDelayDigest.totalDelay:
        sizedDelay.cntPercent = sizedDelay.count * 100.0 / totalDelayCnt

    return transportSchedDelayDigest

def globalTransportSchedDelay(parsedStats, xmlParsedDic, msgBytesOnWireDigest, transportSchedDelayDigest):
    if not('globalListener' in parsedStats):
        transportSchedDelayDigest.clear()
        return transportSchedDelayDigest
    globalListener = parsedStats.globalListener
    generalInfo = parsedStats.generalInfo
    sizes = generalInfo.msgSizeRanges.strip('\"').split(' ')[:]
    transportSchedDelayDigest.totalDelay = []

    for size in sizes:
        delayHistogramKey = 'globalListener.mesg{0}TransportSchedDelay:histogram.bins'.format(size)
        delayStatsKey = 'globalListener.mesg{0}TransportSchedDelay:histogram'.format(size)
        delayDigest = getInterestingModuleStats(parsedStats, delayStatsKey, delayHistogramKey)
        delayDigest.sizeUpBound = '{0}'.format(size)
        delayDigest.bytesPercent = msgBytesOnWireDigest[size].bytesPercent
        delayDigest.bytes = msgBytesOnWireDigest[size].bytes * 2**-10 #in KB
        transportSchedDelayDigest.totalDelay.append(delayDigest)

    totalDelayCnt = sum([delay.count for delay in transportSchedDelayDigest.totalDelay])
    for sizedDelay in transportSchedDelayDigest.totalDelay:
        sizedDelay.cntPercent = sizedDelay.count * 100.0 / totalDelayCnt

    return transportSchedDelayDigest

def printGenralInfo(xmlParsedDic, generalInfo):
    tw = 20
    fw = 12
    lineMax = 100
    title = 'General Simulation Information'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))
    print('Servers Per TOR:'.ljust(tw) + '{0}'.format(generalInfo.numServersPerTor).center(fw) + 'Sender Hosts:'.ljust(tw) +
        '{0}'.format(len(xmlParsedDic.senderIds)).center(fw) + 'Load Factor:'.ljust(tw) + '{0}'.format('%'+str((float(generalInfo.rlf)*100))).center(fw))
    print('TORs:'.ljust(tw) + '{0}'.format(generalInfo.numTors).center(fw) + 'Receiver Hosts:'.ljust(tw) + '{0}'.format(len(xmlParsedDic.receiverIds)).center(fw)
        + 'Start Time:'.ljust(tw) + '{0}'.format(generalInfo.startTime).center(fw))
    print('Host Link Speed:'.ljust(tw) + '{0}'.format(generalInfo.nicLinkSpeed).center(fw) + 'InterArrival Dist:'.ljust(tw) +
        '{0}'.format(generalInfo.interArrivalDist).center(fw) + 'Stop Time:'.ljust(tw) + '{0}'.format(generalInfo.stopTime).center(fw))
    print('Fabric Link Speed:'.ljust(tw) + '{0}'.format(generalInfo.fabricLinkSpeed).center(fw) + 'Edge Link Delay'.ljust(tw) +
        '{0}'.format(generalInfo.edgeLinkDelay).center(fw) + 'Switch Fix Delay:'.ljust(tw) + '{0}'.format(generalInfo.switchFixDelay).center(fw))
    print('Fabric Link Delay'.ljust(tw) + '{0}'.format(generalInfo.fabricLinkDelay).center(fw) + 'SW Turnaround Time:'.ljust(tw) +
        '{0}'.format(generalInfo.hostSwTurnAroundTime).center(fw) + 'NIC Sx ThinkTime:'.ljust(tw) + '{0}'.format(generalInfo.hostNicSxThinkTime).center(fw))
    print('TransportScheme:'.ljust(tw) + '{0} '.format(generalInfo.transportSchemeType).center(fw) + 'Workload Type:'.ljust(tw) +
        '{0}'.format(generalInfo.workloadType).center(fw))

def digestTrafficInfo(trafficBytesAndRateDic, title):
    trafficDigest = trafficBytesAndRateDic.trafficDigest
    trafficDigest.title = title
    if 'bytes' in trafficBytesAndRateDic.keys() and len(trafficBytesAndRateDic.bytes) > 0:
        trafficDigest.cumBytes = sum(trafficBytesAndRateDic.bytes)/1e6
    if 'rates' in trafficBytesAndRateDic.keys() and len(trafficBytesAndRateDic.rates) > 0:
        trafficDigest.cumRate = sum(trafficBytesAndRateDic.rates)
        trafficDigest.avgRate = trafficDigest.cumRate/float(len(trafficBytesAndRateDic.rates))
        trafficDigest.minRate = min(trafficBytesAndRateDic.rates)
        trafficDigest.maxRate = max(trafficBytesAndRateDic.rates)
    if 'dutyCycles' in trafficBytesAndRateDic.keys() and len(trafficBytesAndRateDic.dutyCycles) > 0:
        trafficDigest.avgDutyCycle = sum(trafficBytesAndRateDic.dutyCycles)/float(len(trafficBytesAndRateDic.dutyCycles))
        trafficDigest.minDutyCycle = min(trafficBytesAndRateDic.dutyCycles)
        trafficDigest.maxDutyCycle = max(trafficBytesAndRateDic.dutyCycles)

def computeBytesAndRates(parsedStats, xmlParsedDic):
    trafficDic = AttrDict()
    txAppsBytes = trafficDic.sxHostsTraffic.apps.sx.bytes = []
    txAppsRates = trafficDic.sxHostsTraffic.apps.sx.rates = []
    rxAppsBytes = trafficDic.rxHostsTraffic.apps.rx.bytes = []
    rxAppsRates = trafficDic.rxHostsTraffic.apps.rx.rates = []
    txNicsBytes = trafficDic.sxHostsTraffic.nics.sx.bytes = []
    txNicsRates = trafficDic.sxHostsTraffic.nics.sx.rates = []
    txNicsDutyCycles = trafficDic.sxHostsTraffic.nics.sx.dutyCycles = []
    rxNicsBytes = trafficDic.rxHostsTraffic.nics.rx.bytes = []
    rxNicsRates = trafficDic.rxHostsTraffic.nics.rx.rates = []
    rxNicsDutyCycles = trafficDic.rxHostsTraffic.nics.rx.dutyCycles = []

    nicTxBytes = trafficDic.hostsTraffic.nics.sx.bytes = []
    nicTxRates = trafficDic.hostsTraffic.nics.sx.rates = []
    nicTxDutyCycles = trafficDic.hostsTraffic.nics.sx.dutyCycles = []
    nicRxBytes = trafficDic.hostsTraffic.nics.rx.bytes = []
    nicRxRates = trafficDic.hostsTraffic.nics.rx.rates = []
    nicRxDutyCycles = trafficDic.hostsTraffic.nics.rx.dutyCycles = []

    ethInterArrivalGapBit = 12*8.0 + 8.0*8
    for host in parsedStats.hosts.keys():
        hostId = int(re.match('host\[([0-9]+)]', host).group(1))
        hostStats = parsedStats.hosts[host]
        nicSendBytes = hostStats.access('eth[0].mac.txPk:sum(packetBytes).value')
        nicSendRate = hostStats.access('eth[0].mac.\"bits/sec sent\".value')/1e9
        # Include the 12Bytes ethernet inter arrival gap in the bit rate
        nicSendRate += (hostStats.access('eth[0].mac.\"frames/sec sent\".value') * ethInterArrivalGapBit / 1e9)
        nicSendDutyCycle = hostStats.access('eth[0].mac.\"tx channel utilization (%)\".value')
        nicRcvBytes = hostStats.access('eth[0].mac.rxPkOk:sum(packetBytes).value')
        nicRcvRate = hostStats.access('eth[0].mac.\"bits/sec rcvd\".value')/1e9
        # Include the 12Bytes ethernet inter arrival gap in the bit rate
        nicRcvRate += (hostStats.access('eth[0].mac.\"frames/sec rcvd\".value') * ethInterArrivalGapBit / 1e9)
        nicRcvDutyCycle = hostStats.access('eth[0].mac.\"rx channel utilization (%)\".value')
        nicTxBytes.append(nicSendBytes)
        nicTxRates.append(nicSendRate)
        nicTxDutyCycles.append(nicSendDutyCycle)
        nicRxBytes.append(nicRcvBytes)
        nicRxRates.append(nicRcvRate)
        nicRxDutyCycles.append(nicRcvDutyCycle)

        if hostId in xmlParsedDic.senderIds:
            txAppsBytes.append(hostStats.access('trafficGeneratorApp[0].sentMsg:sum(packetBytes).value'))
            txAppsRates.append(hostStats.access('trafficGeneratorApp[0].sentMsg:last(sumPerDuration(packetBytes)).value')*8.0/1e9)
            txNicsBytes.append(nicSendBytes)
            txNicsRates.append(nicSendRate)
            txNicsDutyCycles.append(nicSendDutyCycle)
        if hostId in xmlParsedDic.receiverIds:
            rxAppsBytes.append(hostStats.access('trafficGeneratorApp[0].rcvdMsg:sum(packetBytes).value'))
            rxAppsRates.append(hostStats.access('trafficGeneratorApp[0].rcvdMsg:last(sumPerDuration(packetBytes)).value')*8.0/1e9)
            rxNicsBytes.append(nicRcvBytes)
            rxNicsRates.append(nicRcvRate)
            rxNicsDutyCycles.append(nicRcvDutyCycle)

    upNicsTxBytes = trafficDic.torsTraffic.upNics.sx.bytes = []
    upNicsTxRates = trafficDic.torsTraffic.upNics.sx.rates = []
    upNicsTxDutyCycle = trafficDic.torsTraffic.upNics.sx.dutyCycles = []
    upNicsRxBytes = trafficDic.torsTraffic.upNics.rx.bytes = []
    upNicsRxRates = trafficDic.torsTraffic.upNics.rx.rates = []
    upNicsRxDutyCycle = trafficDic.torsTraffic.upNics.rx.dutyCycles = []
    downNicsTxBytes =  trafficDic.torsTraffic.downNics.sx.bytes = []
    downNicsTxRates =  trafficDic.torsTraffic.downNics.sx.rates = []
    downNicsTxDutyCycle = trafficDic.torsTraffic.downNics.sx.dutyCycles = []
    downNicsRxBytes = trafficDic.torsTraffic.downNics.rx.bytes = []
    downNicsRxRates = trafficDic.torsTraffic.downNics.rx.rates = []
    downNicsRxDutyCycle = trafficDic.torsTraffic.downNics.rx.dutyCycles = []

    numServersPerTor = int(parsedStats.generalInfo.numServersPerTor)
    fabricLinkSpeed = int(parsedStats.generalInfo.fabricLinkSpeed.strip('Gbps'))
    nicLinkSpeed = int(parsedStats.generalInfo.nicLinkSpeed.strip('Gbps'))
    numTorUplinkNics = int(floor(numServersPerTor * nicLinkSpeed / fabricLinkSpeed))
    for torKey in parsedStats.tors.keys():
        tor = parsedStats.tors[torKey]
        for ifaceId in range(0, numServersPerTor + numTorUplinkNics):
            nicRecvBytes = tor.access('eth[{0}].mac.rxPkOk:sum(packetBytes).value'.format(ifaceId))
            nicRecvRates = tor.access('eth[{0}].mac.\"bits/sec rcvd\".value'.format(ifaceId))/1e9
            # Include the 12Bytes ethernet inter arrival gap in the bit rate
            nicRecvRates += (tor.access('eth[{0}].mac.\"frames/sec rcvd\".value'.format(ifaceId)) * ethInterArrivalGapBit / 1e9)
            nicRecvDutyCycle = tor.access('eth[{0}].mac.\"rx channel utilization (%)\".value'.format(ifaceId))
            nicSendBytes = tor.access('eth[{0}].mac.txPk:sum(packetBytes).value'.format(ifaceId))
            nicSendRates = tor.access('eth[{0}].mac.\"bits/sec sent\".value'.format(ifaceId))/1e9
            # Include the 12Bytes ethernet inter arrival gap in the bit rate
            nicSendRates += (tor.access('eth[{0}].mac.\"frames/sec sent\".value'.format(ifaceId)) * ethInterArrivalGapBit / 1e9)
            nicSendDutyCycle = tor.access('eth[{0}].mac.\"tx channel utilization (%)\".value'.format(ifaceId))
            if ifaceId < numServersPerTor:
                downNicsRxBytes.append(nicRecvBytes)
                downNicsRxRates.append(nicRecvRates)
                downNicsRxDutyCycle.append(nicRecvDutyCycle)
                downNicsTxBytes.append(nicSendBytes)
                downNicsTxRates.append(nicSendRates)
                downNicsTxDutyCycle.append(nicSendDutyCycle)
            else :
                upNicsRxBytes.append(nicRecvBytes)
                upNicsRxRates.append(nicRecvRates)
                upNicsRxDutyCycle.append(nicRecvDutyCycle)
                upNicsTxBytes.append(nicSendBytes)
                upNicsTxRates.append(nicSendRates)
                upNicsTxDutyCycle.append(nicSendDutyCycle)
    return trafficDic

def printBytesAndRates(trafficDic):
    printKeys = ['avgRate', 'cumRate', 'minRate', 'maxRate', 'cumBytes', 'avgDutyCycle', 'minDutyCycle', 'maxDutyCycle']
    tw = 15
    fw = 10
    lineMax = 100
    title = 'Traffic Characteristic (Rates, Bytes, and DutyCycle)'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))

    print("="*lineMax)
    print("Measurement".ljust(tw) + 'AvgRate'.center(fw) + 'CumRate'.center(fw) + 'MinRate'.center(fw) + 'MaxRate'.center(fw) +
             'CumBytes'.center(fw) + 'Avg Duty'.center(fw) + 'Min Duty'.center(fw) + 'Max Duty'.center(fw))
    print("Point".ljust(tw) + '(Gb/s)'.center(fw) + '(Gb/s)'.center(fw) + '(Gb/s)'.center(fw) + '(Gb/s)'.center(fw) +
            '(MB)'.center(fw) + 'Cycle(%)'.center(fw) + 'Cycle(%)'.center(fw) + 'Cycle(%)'.center(fw))

    print("_"*lineMax)
    digestTrafficInfo(trafficDic.sxHostsTraffic.apps.sx, 'SX Apps Send:')
    printStatsLine(trafficDic.sxHostsTraffic.apps.sx.trafficDigest, trafficDic.sxHostsTraffic.apps.sx.trafficDigest.title, tw, fw, '', printKeys)
    digestTrafficInfo(trafficDic.sxHostsTraffic.nics.sx, 'SX NICs Send:')
    printStatsLine(trafficDic.sxHostsTraffic.nics.sx.trafficDigest, trafficDic.sxHostsTraffic.nics.sx.trafficDigest.title, tw, fw, '', printKeys)
    digestTrafficInfo(trafficDic.hostsTraffic.nics.sx, 'All NICs Send:')
    printStatsLine(trafficDic.hostsTraffic.nics.sx.trafficDigest, trafficDic.hostsTraffic.nics.sx.trafficDigest.title, tw, fw, '', printKeys)


    digestTrafficInfo(trafficDic.torsTraffic.downNics.rx, 'TORs Down Recv:')
    printStatsLine(trafficDic.torsTraffic.downNics.rx.trafficDigest, trafficDic.torsTraffic.downNics.rx.trafficDigest.title, tw, fw, '', printKeys)
    digestTrafficInfo(trafficDic.torsTraffic.upNics.sx, 'TORs Up Send:')
    printStatsLine(trafficDic.torsTraffic.upNics.sx.trafficDigest, trafficDic.torsTraffic.upNics.sx.trafficDigest.title, tw, fw, '', printKeys)
    digestTrafficInfo(trafficDic.torsTraffic.upNics.rx, 'TORs Up Recv:')
    printStatsLine(trafficDic.torsTraffic.upNics.rx.trafficDigest, trafficDic.torsTraffic.upNics.rx.trafficDigest.title, tw, fw, '', printKeys)
    digestTrafficInfo(trafficDic.torsTraffic.downNics.sx, 'TORs Down Send:')
    printStatsLine(trafficDic.torsTraffic.downNics.sx.trafficDigest, trafficDic.torsTraffic.downNics.sx.trafficDigest.title, tw, fw, '', printKeys)


    digestTrafficInfo(trafficDic.hostsTraffic.nics.rx, 'ALL NICs Recv:')
    printStatsLine(trafficDic.hostsTraffic.nics.rx.trafficDigest, trafficDic.hostsTraffic.nics.rx.trafficDigest.title, tw, fw, '', printKeys)
    digestTrafficInfo(trafficDic.rxHostsTraffic.nics.rx, 'RX NICs Recv:')
    printStatsLine(trafficDic.rxHostsTraffic.nics.rx.trafficDigest, trafficDic.rxHostsTraffic.nics.rx.trafficDigest.title, tw, fw, '', printKeys)
    digestTrafficInfo(trafficDic.rxHostsTraffic.apps.rx, 'RX Apps Recv:')
    printStatsLine(trafficDic.rxHostsTraffic.apps.rx.trafficDigest, trafficDic.rxHostsTraffic.apps.rx.trafficDigest.title, tw, fw, '', printKeys)

def computeHomaRates(parsedStats, xmlParsedDic):
    trafficDic = AttrDict()
    sxHostsNicUnschedRates = trafficDic.sxHostsTraffic.nics.sx.unschedPkts.rates = []
    sxHostsNicReqRates = trafficDic.sxHostsTraffic.nics.sx.reqPkts.rates = []
    sxHostsNicSchedRates = trafficDic.sxHostsTraffic.nics.sx.schedPkts.rates = []
    rxHostsNicGrantRates = trafficDic.rxHostsTraffic.nics.sx.grantPkts.rates = []

    ethInterArrivalGapBit = 12*8.0 + 8.0*8
    for host in parsedStats.hosts.keys():
        hostId = int(re.match('host\[([0-9]+)]', host).group(1))
        hostStats = parsedStats.hosts[host]
        simTime = hostStats.access('eth[0].mac.\"simulated time\".value')
        nicSxUnschedBitRate = hostStats.access('eth[0].mac.\"Homa Unsched bits/sec sent\".value')
        nicSxUnschedFrameRate = hostStats.access('eth[0].mac.\"Homa Unsched frames/sec sent\".value')
        # Include the  12Bytes ethernet inter arrival gap in the bit rate
        nicSxUnschedBitRate += (nicSxUnschedFrameRate * ethInterArrivalGapBit)
        nicSendUnschedRate = (nicSxUnschedBitRate/1e6, nicSxUnschedFrameRate/1e3)

        nicSxSchedBitRate = hostStats.access('eth[0].mac.\"Homa Sched bits/sec sent\".value')
        nicSxSchedFrameRate = hostStats.access('eth[0].mac.\"Homa Sched frames/sec sent\".value')
        # Include the  12Bytes ethernet inter arrival gap in the bit rate
        nicSxSchedBitRate += (nicSxSchedFrameRate * ethInterArrivalGapBit)
        nicSendSchedRate = (nicSxSchedBitRate/1e6, nicSxSchedFrameRate/1e3)

        nicSxReqBitRate = hostStats.access('eth[0].mac.\"Homa Req bits/sec sent\".value')
        nicSxReqFrameRate = hostStats.access('eth[0].mac.\"Homa Req frames/sec sent\".value')
        # Include the  12Bytes ethernet inter arrival gap in the bit rate
        nicSxReqBitRate += (nicSxReqFrameRate * ethInterArrivalGapBit)
        nicSendReqRate = (nicSxReqBitRate/1e6, nicSxReqFrameRate/1e3)

        nicSxGrantBitRate = hostStats.access('eth[0].mac.\"Homa  Grant bits/sec sent\".value')
        nicSxGrantFrameRate = hostStats.access('eth[0].mac.\"Homa  Grant frames/sec sent\".value')
        # Include the  12Bytes ethernet inter arrival gap in the bit rate
        nicSxGrantBitRate += (nicSxGrantFrameRate * ethInterArrivalGapBit)
        nicSendGrantRate = (nicSxGrantBitRate/1e6, nicSxGrantFrameRate/1e3)

        if hostId in xmlParsedDic.senderIds:
            sxHostsNicUnschedRates.append(nicSendUnschedRate)
            sxHostsNicSchedRates.append(nicSendSchedRate)
            sxHostsNicReqRates.append(nicSendReqRate)

        if hostId in xmlParsedDic.receiverIds:
            rxHostsNicGrantRates.append(nicSendGrantRate)
    return trafficDic

def digestHomaRates(homaTrafficDic, title):
    trafficDigest = homaTrafficDic.rateDigest
    trafficDigest.title = title
    if 'rates' in homaTrafficDic.keys():
        trafficDigest.cumBitRate = sum([rate[0] for rate in homaTrafficDic.rates])
        trafficDigest.avgBitRate = trafficDigest.cumBitRate/float(len(homaTrafficDic.rates))
        trafficDigest.minBitRate = min([rate[0] for rate in homaTrafficDic.rates])
        trafficDigest.maxBitRate = max([rate[0] for rate in homaTrafficDic.rates])
        trafficDigest.cumFrameRate = sum([rate[1] for rate in homaTrafficDic.rates])
        trafficDigest.avgFrameRate = trafficDigest.cumFrameRate/float(len(homaTrafficDic.rates))

def printHomaRates(trafficDic):
    printKeys = ['avgFrameRate', 'cumFrameRate', 'avgBitRate', 'cumBitRate', 'minBitRate', 'maxBitRate']
    tw = 25
    fw = 11
    lineMax = 90
    title = 'Homa Packets Traffic Rates'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))
    print("="*lineMax)
    print("Homa Packet Type".ljust(tw) + 'AvgRate'.center(fw) + 'CumRate'.center(fw) + 'AvgRate'.center(fw) +
        'CumRate'.center(fw) + 'MinRate'.center(fw) + 'MaxRate'.center(fw))
    print("".ljust(tw) + '(kPkt/s)'.center(fw)+ '(kPkt/s)'.center(fw)  + '(Mb/s)'.center(fw) +
        '(Mb/s)'.center(fw) + '(Mb/s)'.center(fw) + '(Mb/s)'.center(fw))

    print("_"*lineMax)
    digestHomaRates(trafficDic.sxHostsTraffic.nics.sx.reqPkts, 'SX NICs Req. SxRate:')
    printStatsLine(trafficDic.sxHostsTraffic.nics.sx.reqPkts.rateDigest, trafficDic.sxHostsTraffic.nics.sx.reqPkts.rateDigest.title, tw, fw, '', printKeys)
    digestHomaRates(trafficDic.sxHostsTraffic.nics.sx.unschedPkts, 'SX NICs Unsched. SxRate:')
    printStatsLine(trafficDic.sxHostsTraffic.nics.sx.unschedPkts.rateDigest, trafficDic.sxHostsTraffic.nics.sx.unschedPkts.rateDigest.title, tw, fw, '', printKeys)
    digestHomaRates(trafficDic.sxHostsTraffic.nics.sx.schedPkts, 'SX NICs Sched. SxRate:')
    printStatsLine(trafficDic.sxHostsTraffic.nics.sx.schedPkts.rateDigest, trafficDic.sxHostsTraffic.nics.sx.schedPkts.rateDigest.title, tw, fw, '', printKeys)
    digestHomaRates(trafficDic.rxHostsTraffic.nics.sx.grantPkts, 'RX NICs Grant SxRate:')
    printStatsLine(trafficDic.rxHostsTraffic.nics.sx.grantPkts.rateDigest, trafficDic.rxHostsTraffic.nics.sx.grantPkts.rateDigest.title, tw, fw, '', printKeys)

def printHomaOutstandingBytes(parsedStats, xmlParsedDic, unit):
    tw = 25
    fw = 8
    lineMax = 90
    title = 'Receiver\'s Outstanding Bytes(@ packet arrivals) and Grants (@ new grants)'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))
    trafficDic = AttrDict()

    outstandingGrantsStats = []
    outstandingByteStats = []
    for host in parsedStats.hosts.keys():
        hostId = int(re.match('host\[([0-9]+)]', host).group(1))
        if hostId in xmlParsedDic.receiverIds:
            outstandingGrantsHistogramKey = 'host[{0}].transportScheme.outstandingGrantBytes:histogram.bins'.format(hostId)
            outstandingGrantsStatsKey = 'host[{0}].transportScheme.outstandingGrantBytes:stats'.format(hostId)
            outstandingGrantsStats.append(getInterestingModuleStats(parsedStats.hosts, outstandingGrantsStatsKey, outstandingGrantsHistogramKey))

            outstandingBytesHistogramKey = 'host[{0}].transportScheme.totalOutstandingBytes:histogram.bins'.format(hostId)
            outstandingBytesStatsKey = 'host[{0}].transportScheme.totalOutstandingBytes:stats'.format(hostId)
            outstandingByteStats.append(getInterestingModuleStats(parsedStats.hosts, outstandingBytesStatsKey, outstandingBytesHistogramKey))

    outstandingBytes = AttrDict()
    outstandingBytes.totalBytes = digestModulesStats(outstandingByteStats)
    outstandingBytes.grantBytes = digestModulesStats(outstandingGrantsStats)
    printKeys = ['mean', 'stddev', 'min', 'median', 'threeQuartile', 'ninety9Percentile', 'max', 'count']
    print("="*lineMax)
    print("In Flight Bytes".ljust(tw) + 'mean'.format(unit).center(fw) + 'stddev'.format(unit).center(fw) +
            'min'.format(unit).center(fw) + 'median'.format(unit).center(fw) + '75%ile'.format(unit).center(fw) +
            '99%ile'.format(unit).center(fw) + 'max'.format(unit).center(fw) + 'count'.center(fw))

    print("".ljust(tw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) +
            '({0})'.format(unit).center(fw) + '({0})'.format(unit).center(fw) + ''.center(fw))

    print("_"*lineMax)

    printStatsLine(outstandingBytes.grantBytes, 'Outstanding Grant Bytes:', tw, fw, unit, printKeys)
    printStatsLine(outstandingBytes.totalBytes, 'Outstanding Total Bytes:', tw, fw, unit, printKeys)

def computeWastedTimesAndBw(parsedStats, xmlParsedDic):
    nicLinkSpeed = int(parsedStats.generalInfo.nicLinkSpeed.strip('Gbps'))
    senderHostIds = xmlParsedDic.senderIds
    receiverHostIds = xmlParsedDic.receiverIds

    activeAndWasted = AttrDict()
    activeAndWasted.rx.totalTime = 0
    activeAndWasted.rx.activeTime = 0
    activeAndWasted.rx.wastedTime = 0
    activeAndWasted.rx.expectedBytes = 0
    activeAndWasted.rx.realBytes = 0
    activeAndWasted.rx.fracTotalTime = 0
    activeAndWasted.rx.fracActiveTime = 0

    activeAndWasted.rx.oversubTime = 0
    activeAndWasted.rx.oversubByte = 0
    activeAndWasted.rx.oversubWastedTime = 0
    activeAndWasted.rx.oversubWastedFracTotalTime = 0
    activeAndWasted.rx.oversubWastedFracOversubTime = 0


    activeAndWasted.sx.activeTime = 0
    activeAndWasted.sx.wastedTime = 0
    activeAndWasted.sx.expectedBytes = 0
    activeAndWasted.sx.realBytes = 0
    activeAndWasted.sx.totalTime = 0
    activeAndWasted.sx.fracTotalTime = 0
    activeAndWasted.sx.fracActiveTime = 0
    activeAndWasted.sx.schedDelayFrac = 0
    activeAndWasted.sx.unschedDelayFrac = 0
    activeAndWasted.sx.delayFrac = 0

    activeAndWasted.nicLinkSpeed = nicLinkSpeed
    activeAndWasted.simTime = 0
    simTime = 0
    totalSimTime = 0
    divideNoneZero = lambda dividend, divisor: dividend * 1.0/divisor if divisor !=0 else 0.0
    for host in parsedStats.hosts.keys():
        hostId = int(re.match('host\[([0-9]+)]', host).group(1))
        hostStats = parsedStats.hosts[host]
        simTime = float(hostStats.access('eth[0].mac.\"simulated time\".value'))
        totalSimTime += simTime
        lastSxTime = hostStats.access('eth[0].mac.\"last transmission time\".value')
        lastRxTime = hostStats.access('eth[0].mac.\"last reception time\".value')
        if hostId in receiverHostIds:
            rxActiveTimeStats = hostStats.access('transportScheme.rxActiveTime:stats')
            rxBytesStats = hostStats.access('transportScheme.rxActiveBytes:stats')
            activeTime = rxActiveTimeStats.sum
            expectedBytes = (rxActiveTimeStats.sum)*nicLinkSpeed*1e9/8.0
            realBytes = 1.0 * rxBytesStats.sum
            wastedTime = (expectedBytes-realBytes)*8.0/(nicLinkSpeed*1e9)

            activeAndWasted.rx.activeTime += activeTime
            activeAndWasted.rx.expectedBytes += expectedBytes
            activeAndWasted.rx.realBytes += realBytes
            activeAndWasted.rx.wastedTime += wastedTime
            activeAndWasted.rx.totalTime += lastRxTime


            rxOversubTimeStats = hostStats.access('transportScheme.oversubscriptionTime:stats')
            rxOversubByteStats = hostStats.access('transportScheme.oversubscriptionBytes:stats')
            oversubTime = rxOversubTimeStats.sum
            oversubExpectedBytes = (rxOversubTimeStats.sum)*nicLinkSpeed*1e9/8.0
            oversubRealBytes = 1.0 * rxOversubByteStats.sum
            wastedOversubTime = (oversubExpectedBytes-oversubRealBytes)*8.0/(nicLinkSpeed*1e9)


            activeAndWasted.rx.oversubTime +=  oversubTime
            activeAndWasted.rx.oversubByte += oversubRealBytes
            activeAndWasted.rx.oversubWastedTime += wastedOversubTime


        if hostId in senderHostIds:
            sxActiveTimeStats = hostStats.access('transportScheme.sxActiveTime:stats')
            sxBytesStats = hostStats.access('transportScheme.sxActiveBytes:stats')
            activeTime = sxActiveTimeStats.sum
            expectedBytes = (sxActiveTimeStats.sum)*nicLinkSpeed*1e9/8.0
            realBytes = 1.0 * sxBytesStats.sum
            wastedTime = (expectedBytes-realBytes)*8.0/(nicLinkSpeed*1e9)
            schedDelayStats = hostStats.access('transportScheme.sxSchedPktDelay:stats')
            schedDelayFrac = 1.0 * schedDelayStats.sum
            unschedDelayStats = hostStats.access('transportScheme.sxUnschedPktDelay:stats')
            unschedDelayFrac = 1.0 * unschedDelayStats.sum
            sumDelayFrac = schedDelayFrac + unschedDelayFrac

            activeAndWasted.sx.activeTime += activeTime
            activeAndWasted.sx.expectedBytes += expectedBytes
            activeAndWasted.sx.realBytes += realBytes
            activeAndWasted.sx.wastedTime += wastedTime
            activeAndWasted.sx.totalTime += lastSxTime
            activeAndWasted.sx.schedDelayFrac += schedDelayFrac
            activeAndWasted.sx.unschedDelayFrac += unschedDelayFrac
            activeAndWasted.sx.delayFrac += sumDelayFrac

    activeAndWasted.rx.fracTotalTime = 100*activeAndWasted.rx.wastedTime/activeAndWasted.rx.totalTime
    activeAndWasted.rx.fracActiveTime = 100*divideNoneZero(activeAndWasted.rx.wastedTime, activeAndWasted.rx.activeTime)
    activeAndWasted.rx.oversubWastedFracTotalTime = 100*activeAndWasted.rx.oversubWastedTime/activeAndWasted.rx.totalTime
    activeAndWasted.rx.oversubWastedFracOversubTime = 100*divideNoneZero(activeAndWasted.rx.oversubWastedTime, activeAndWasted.rx.oversubTime)


    activeAndWasted.sx.fracTotalTime = 100*activeAndWasted.sx.wastedTime/activeAndWasted.sx.totalTime
    activeAndWasted.sx.fracActiveTime = 100*divideNoneZero(activeAndWasted.sx.wastedTime, activeAndWasted.sx.activeTime)
    activeAndWasted.sx.schedDelayFrac = 100*activeAndWasted.sx.schedDelayFrac / activeAndWasted.sx.totalTime
    activeAndWasted.sx.unschedDelayFrac = 100*activeAndWasted.sx.unschedDelayFrac / activeAndWasted.sx.totalTime
    activeAndWasted.sx.delayFrac = 100*activeAndWasted.sx.delayFrac / activeAndWasted.sx.totalTime

    activeAndWasted.simTime = totalSimTime / len(parsedStats.hosts.keys())
    return activeAndWasted

def computeSelfInflictedWastedBw(parsedStats, xmlParsedDic):
    nicLinkSpeed = int(parsedStats.generalInfo.nicLinkSpeed.strip('Gbps'))
    receiverHostIds = xmlParsedDic.receiverIds
    simTime = 0
    totalSimTime = 0
    for host in parsedStats.hosts.keys():
        hostId = int(re.match('host\[([0-9]+)]', host).group(1))
        if hostId in receiverHostIds:
            hostStats = parsedStats.hosts[host]
            simTime = float(hostStats.access('eth[0].mac.\"last reception time\".value'))
            totalSimTime += simTime

    selfWasteTime = AttrDict()
    selfWasteTime.totalTime = totalSimTime

    highrWasteBwKey = 'globalListener.highrSelfBwWaste:histogram'
    selfWasteTime.highrOverEst.sum = float(parsedStats.access(highrWasteBwKey + '.sum'))

    lowerWasteBwKey = 'globalListener.lowerSelfBwWaste:histogram'
    selfWasteTime.lowerOverEst.sum = float(parsedStats.access(lowerWasteBwKey + '.sum'))

    selfWasteTime.highrOverEst.fracTotalTime = selfWasteTime.highrOverEst.sum /\
        selfWasteTime.totalTime

    selfWasteTime.lowerOverEst.fracTotalTime = selfWasteTime.lowerOverEst.sum /\
        selfWasteTime.totalTime

    return selfWasteTime

def printWastedTimeAndBw(parsedStats, xmlParsedDic, activeAndWasted, selfWasteTime):
    lineMax = 90
    tw = 12
    fw = 20
    title = 'Average Wasted Time and Bandwidth at Receivers'

    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))

    print("="*lineMax)
    print('Wasted BW'.ljust(tw) + 'Wasted BW'.center(fw) + "Oversub Wasted BW".center(fw) + 'Oversub Wasted BW '.center(fw))
    print("% of Active".ljust(tw) + '% of Total'.center(fw) + '% of Oversub'.center(fw) + '% of Total'.center(fw))
    print("_"*lineMax)
    print('{0:.2f}'.format(activeAndWasted.rx.fracActiveTime).ljust(tw) + '{0:.2f}'.format(activeAndWasted.rx.fracTotalTime).center(fw) +
          '{0:.2f}'.format(activeAndWasted.rx.oversubWastedFracOversubTime).center(fw) +
          '{0:.2f}'.format(activeAndWasted.rx.oversubWastedFracTotalTime).center(fw))

    tw = 15
    fw = 30
    print('\n'*2 + "="*lineMax)
    print("Lower Self-inflicted".ljust(tw) + 'Higher Self-inflicted'.center(fw))
    print("Wasted BW %".ljust(tw) + 'Wasted BW %'.center(fw))
    print("_"*lineMax)
    print('{0:.2f}'.format(selfWasteTime.highrOverEst.fracTotalTime * 100.0).ljust(tw) +\
        '{0:.2f}'.format(selfWasteTime.lowerOverEst.fracTotalTime * 100.0).center(fw))


    tw = 14
    fw = 14
    title = 'Average Wasted Time and Bandwidth at Senders'

    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))

    print("="*lineMax)
    print("Sx Wasted ".ljust(tw) + 'Sx Wasted '.center(fw) + 'Sx Sched'.center(fw) + 'Sx Unsched'.center(fw) + 'Sx Total'.center(fw))
    print("Active BW %".ljust(tw) + 'Total BW %'.center(fw) + 'Delay %'.center(fw) + 'Delay %'.center(fw) + 'Delay %'.center(fw) )
    print("_"*lineMax)
    print('{0:.2f}'.format(activeAndWasted.sx.fracActiveTime).ljust(tw) + '{0:.2f}'.format(activeAndWasted.sx.fracTotalTime).center(fw) +
        '{0:.2f}'.format(activeAndWasted.sx.schedDelayFrac).center(fw) +  '{0:.2f}'.format(activeAndWasted.sx.unschedDelayFrac).center(fw) +
        '{0:.2f}'.format(activeAndWasted.sx.delayFrac).center(fw))

def computePrioUsageStats(hosts, generalInfo, xmlParsedDic, prioUsageStatsDigest):
    receiverIds = xmlParsedDic.receiverIds
    numPrioLevels = int(generalInfo.prioLevels)
    nicLinkSpeed = int(generalInfo.nicLinkSpeed.strip('Gbps'))
    rxTimes = AttrDict()
    rxTimes.total.totalTime = 0.0
    rxTimes.total.activeTime = 0.0
    rxTimes.total.activelyRecvTime = 0.0
    rxTimes.total.activelyRecvSchedTime = 0.0
    rxTimes.total.activelyRecvUnschedTime = 0.0
    rxTimes.total.activelyRecvGrantTime = 0.0

    # loop over all receiver and find: 1)total time, 2) total active time, 3)
    # total actively receiving time, 4) total time receiving scheduled bytes,
    # 5) total time receiving unscheduled bytes.

    allBytesList = [] #for debugging and verification
    grantBytesAll = 0 #for debugging and verification
    for i in receiverIds:
        rxTimes[i] = AttrDict()
        totalTime = float(hosts.access('host[{0}].eth[0].mac.\"simulated time\".value'.format(i)))
        totalRxTime = float(hosts.access('host[{0}].eth[0].mac.\"last reception time\".value'.format(i)))
        rxTimes[i].totalTime = totalRxTime

        activeTimeStats = hosts.access('host[{0}].transportScheme.rxActiveTime:stats'.format(i))
        activeTime = activeTimeStats.sum
        rxTimes[i].activeTime = activeTime

        receivedBytesStats = hosts.access('host[{0}].transportScheme.rxActiveBytes:stats'.format(i))
        activelyRecvTime = 8.0 * receivedBytesStats.sum / (nicLinkSpeed*1e9)

        rxTimes[i].activelyRecvTime = activelyRecvTime
        allBytes = 0.0
        unschedBytes = 0.0
        grantBytes = 0.0
        schedBytes = 0.0
        for prio in range(numPrioLevels):
            pktBytesStatsKey = 'host[{0}].transportScheme.homaPktPrio{1}Signal:stats(homaPktBytes)'.format(i, prio)
            pktByteStats = hosts.access(pktBytesStatsKey)
            allBytes += pktByteStats.sum

            unschedPktBytesStatsKey = 'host[{0}].transportScheme.homaPktPrio{1}Signal:stats(homaUnschedPktBytes)'.format(i, prio)
            unschedPktByteStats = hosts.access(unschedPktBytesStatsKey)
            unschedBytes += unschedPktByteStats.sum

            grantPktBytesStatsKey = 'host[{0}].transportScheme.homaPktPrio{1}Signal:stats(homaGrantPktBytes)'.format(i, prio)
            grantPktByteStats = hosts.access(grantPktBytesStatsKey)
            grantBytes += grantPktByteStats.sum

        allBytesList.append((receivedBytesStats.sum, allBytes)) #for debugging and verification
        grantBytesAll += grantBytes  #for debugging and verification
        schedBytes = allBytes - unschedBytes - grantBytes
        rxTimes[i].activelyRecvSchedTime = 8.0 * schedBytes / (nicLinkSpeed*1e9)
        rxTimes[i].activelyRecvUnschedTime = 8.0 * unschedBytes / (nicLinkSpeed*1e9)
        rxTimes[i].activelyRecvGrantTime = 8.0 * grantBytes / (nicLinkSpeed*1e9)

        rxTimes.total.totalTime += totalRxTime
        rxTimes.total.activeTime += activeTime
        rxTimes.total.activelyRecvTime += activelyRecvTime
        rxTimes.total.activelyRecvSchedTime += rxTimes[i].activelyRecvSchedTime
        rxTimes.total.activelyRecvUnschedTime += rxTimes[i].activelyRecvUnschedTime
        rxTimes.total.activelyRecvGrantTime += rxTimes[i].activelyRecvGrantTime

    #print(allBytesList) #for debugging and verification
    #print(grantBytesAll)

    totalBytes = 0
    totalPkts = 0
    seterr(divide='ignore')
    for prio in range(numPrioLevels):
        prioUsageStats = AttrDict()
        prioUsageStats.priority = prio
        prioUsageStats.msgMinSize = inf
        prioUsageStats.msgMaxSize = 0
        prioUsageStats.totalBytes = 0
        prioUsageStats.totalPkts = 0
        prioUsageStats.unschedBytes = 0
        prioUsageStats.unschedBytesPerc = 0
        prioUsageStats.unschedPkts = 0
        prioUsageStats.unschedPktsPerc = 0
        for i in receiverIds:
            # find bytes and pkts on the prio level
            msgSizeStatsKey = 'host[{0}].transportScheme.homaPktPrio{1}Signal:stats(homaMsgSize)'.format(i, prio)
            pktBytesStatsKey = 'host[{0}].transportScheme.homaPktPrio{1}Signal:stats(homaPktBytes)'.format(i, prio)
            unschedPktBytesStatsKey = 'host[{0}].transportScheme.homaPktPrio{1}Signal:stats(homaUnschedPktBytes)'.format(i, prio)
            grantPktBytesStatsKey = 'host[{0}].transportScheme.homaPktPrio{1}Signal:stats(homaGrantPktBytes)'.format(i, prio)
            msgSizeStats = hosts.access(msgSizeStatsKey)
            prioUsageStats.msgMinSize = min(prioUsageStats.msgMinSize, msgSizeStats.min)
            prioUsageStats.msgMaxSize = max(prioUsageStats.msgMaxSize, msgSizeStats.max)
            pktByteStats = hosts.access(pktBytesStatsKey)
            prioUsageStats.totalBytes += pktByteStats.sum
            totalBytes += pktByteStats.sum
            prioUsageStats.totalPkts += pktByteStats.count
            totalPkts += pktByteStats.count
            unschedPktByteStats = hosts.access(unschedPktBytesStatsKey)
            prioUsageStats.unschedBytes += unschedPktByteStats.sum
            prioUsageStats.unschedPkts += unschedPktByteStats.count

        prioUsageStats.unschedBytesPerc = 100 * divide(prioUsageStats.unschedBytes , prioUsageStats.totalBytes)
        prioUsageStats.unschedPktsPerc = 100 * divide(prioUsageStats.unschedPkts , prioUsageStats.totalPkts)
        prioUsageStats.usageTimePct.total = 100 * divide(prioUsageStats.totalBytes * 8.0 , nicLinkSpeed * 1e9 * rxTimes.total.totalTime)
        prioUsageStats.usageTimePct.active = 100 * divide(prioUsageStats.totalBytes * 8.0 , nicLinkSpeed * 1e9 * rxTimes.total.totalTime)
        prioUsageStats.usageTimePct.activelyRecv = 100 * divide(prioUsageStats.totalBytes * 8.0 , nicLinkSpeed * 1e9 * rxTimes.total.activelyRecvTime)
        prioUsageStats.usageTimePct.activelyRecvSched = 100 * divide(prioUsageStats.totalBytes * 8.0 , nicLinkSpeed * 1e9 * rxTimes.total.activelyRecvSchedTime)

        prioUsageStatsDigest.append(prioUsageStats)

    for prioUsageStats in prioUsageStatsDigest:
        prioUsageStats.totalBytesPerc = 100 * divide(prioUsageStats.totalBytes, totalBytes)
        prioUsageStats.totalPktsPerc = 100 * divide(prioUsageStats.totalPkts, totalPkts)
    return

def printPrioUsageStats(prioUsageStatsDigest):
    printKeys = ['msgMinSize', 'msgMaxSize', 'totalBytesPerc', 'unschedBytesPerc', 'totalPktsPerc', 'unschedPktsPerc']
    tw = 12
    fw = 12
    lineMax = 85
    title = 'Bytes and Packets Received On Different Priority Levels'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))

    print("="*lineMax)
    print("Priority".ljust(tw) + 'Min Mesg'.center(fw) + 'Max Mesg'.center(fw) + 'TotalBytes'.center(fw) +
            'UnschedByte'.center(fw) + 'TotalPkts'.center(fw) + 'UnschedPkts'.center(fw))
    print(''.ljust(tw) + 'Size(B)'.center(fw) + 'Size(B)'.center(fw) + '%'.center(fw) +
            '% In Prio'.center(fw) + '%'.center(fw) + '% In Prio'.center(fw))
    print("_"*lineMax)

    for prioStats in prioUsageStatsDigest:
        printStatsLine(prioStats, 'prio {0}'.format(prioStats.priority), tw, fw, '', printKeys)


    printKeys = ['total', 'active', 'activelyRecv', 'activelyRecvSched']
    tw = 12
    fw = 18
    lineMax = 85
    title = 'Time Usage Percentages Of Different Priority Levels '
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))

    print("="*lineMax)
    print("Priority".ljust(tw) + '% of Total Time'.center(fw) + '% of Active Time'.center(fw) +
        '% of Active'.center(fw) + '% of Active'.center(fw))
    print(''.ljust(tw) + ''.center(fw) + ''.center(fw) + ' Recv Time'.center(fw) + 'Recv Sched Time'.center(fw))
    print("_"*lineMax)

    for prioStats in prioUsageStatsDigest:
        printStatsLine(prioStats.usageTimePct, 'prio {0}'.format(prioStats.priority), tw, fw, '', printKeys)

    return

def digestQueueLenInfo(queueLenDic, title):
    queueLenDigest = queueLenDic.queueLenDigest
    totalCount = sum(queueLenDic.count) * 1.0
    keyList = ['meanCnt', 'empty', 'onePkt', 'stddevCnt', 'meanBytes', 'stddevBytes']
    for key in queueLenDic.keys():
        if len(queueLenDic[key]) > 0 and key in keyList:
            queueLenDigest[key] = 0

    if totalCount != 0:
        for i,cnt in enumerate(queueLenDic.count):
            for key in queueLenDigest.keys():
                if not math.isnan(queueLenDic.access(key)[i]):
                    queueLenDigest[key] += queueLenDic.access(key)[i] * cnt

        for key in queueLenDigest.keys():
                queueLenDigest[key] /= totalCount

    for key in queueLenDic.keys():
        if len(queueLenDic[key]) == 0 and key in keyList:
            queueLenDigest[key] = nan

    queueLenDigest.title = title
    if len(queueLenDic.minCnt) > 0:
        queueLenDigest.minCnt = min(queueLenDic.minCnt)
        queueLenDigest.minBytes = min(queueLenDic.minBytes)
    if len(queueLenDic.maxCnt) > 0:
        queueLenDigest.maxCnt = max(queueLenDic.maxCnt)
        queueLenDigest.maxBytes = max(queueLenDic.maxBytes)

def computeQueueLength(parsedStats, xmlParsedDic):
    printKeys = ['meanCnt', 'stddevCnt', 'meanBytes', 'stddevBytes', 'empty', 'onePkt', 'minCnt', 'minBytes', 'maxCnt', 'maxBytes']
    queueLen = AttrDict()
    keysAll = printKeys[:]
    keysAll.append('count')
    for key in keysAll:
        queueLen.sxHosts.transport[key] = []
        queueLen.sxHosts.nic[key] = []
        queueLen.hosts.nic[key] = []
        queueLen.tors.up.nic[key] = []
        queueLen.sxTors.up.nic[key] = []
        queueLen.tors.down.nic[key] = []
        queueLen.rxTors.down.nic[key] = []
        queueLen.aggrs.nic[key] = []

    for host in parsedStats.hosts.keys():
        hostId = int(re.match('host\[([0-9]+)]', host).group(1))
        hostStats = parsedStats.hosts[host]
        transQueueLenCnt = hostStats.access('transportScheme.msgsLeftToSend:stats.count')
        transQueueLenMin = hostStats.access('transportScheme.msgsLeftToSend:stats.min')
        transQueueLenMax = hostStats.access('transportScheme.msgsLeftToSend:stats.max')
        transQueueLenMean = hostStats.access('transportScheme.msgsLeftToSend:stats.mean')
        transQueueLenStddev = hostStats.access('transportScheme.msgsLeftToSend:stats.stddev')
        transQueueBytesMin = hostStats.access('transportScheme.bytesLeftToSend:stats.min')/2**10
        transQueueBytesMax = hostStats.access('transportScheme.bytesLeftToSend:stats.max')/2**10
        transQueueBytesMean = hostStats.access('transportScheme.bytesLeftToSend:stats.mean')/2**10
        transQueueBytesStddev = hostStats.access('transportScheme.bytesLeftToSend:stats.stddev')/2**10

        nicQueueLenCnt = hostStats.access('eth[0].queue.dataQueue.queueLength:stats.count')
        nicQueueLenMin = hostStats.access('eth[0].queue.dataQueue.queueLength:stats.min')
        nicQueueLenEmpty = hostStats.access('eth[0].queue.dataQueue.\"queue empty (%)\".value')
        nicQueueLenOnePkt = hostStats.access('eth[0].queue.dataQueue.\"queue length one (%)\".value')
        nicQueueLenMax = hostStats.access('eth[0].queue.dataQueue.queueLength:stats.max')
        nicQueueLenMean = hostStats.access('eth[0].queue.dataQueue.queueLength:stats.mean')
        nicQueueLenStddev = hostStats.access('eth[0].queue.dataQueue.queueLength:stats.stddev')
        nicQueueBytesMin = hostStats.access('eth[0].queue.dataQueue.queueByteLength:stats.min')/2**10
        nicQueueBytesMax = hostStats.access('eth[0].queue.dataQueue.queueByteLength:stats.max')/2**10
        nicQueueBytesMean = hostStats.access('eth[0].queue.dataQueue.queueByteLength:stats.mean')/2**10
        nicQueueBytesStddev = hostStats.access('eth[0].queue.dataQueue.queueByteLength:stats.stddev')/2**10
        queueLen.hosts.nic.empty.append(nicQueueLenEmpty)
        queueLen.hosts.nic.onePkt.append(nicQueueLenOnePkt)
        queueLen.hosts.nic.count.append(nicQueueLenCnt)
        queueLen.hosts.nic.minCnt.append(nicQueueLenMin)
        queueLen.hosts.nic.maxCnt.append(nicQueueLenMax)
        queueLen.hosts.nic.meanCnt.append(nicQueueLenMean)
        queueLen.hosts.nic.stddevCnt.append(nicQueueLenStddev)
        queueLen.hosts.nic.minBytes.append(nicQueueBytesMin)
        queueLen.hosts.nic.maxBytes.append(nicQueueBytesMax)
        queueLen.hosts.nic.meanBytes.append(nicQueueBytesMean)
        queueLen.hosts.nic.stddevBytes.append(nicQueueBytesStddev)

        if hostId in xmlParsedDic.senderIds:
            queueLen.sxHosts.transport.minCnt.append(transQueueLenMin)
            queueLen.sxHosts.transport.count.append(transQueueLenCnt)
            queueLen.sxHosts.transport.maxCnt.append(transQueueLenMax)
            queueLen.sxHosts.transport.meanCnt.append(transQueueLenMean)
            queueLen.sxHosts.transport.stddevCnt.append(transQueueLenStddev)
            queueLen.sxHosts.transport.minBytes.append(transQueueBytesMin)
            queueLen.sxHosts.transport.maxBytes.append(transQueueBytesMax)
            queueLen.sxHosts.transport.meanBytes.append(transQueueBytesMean)
            queueLen.sxHosts.transport.stddevBytes.append(transQueueBytesStddev)

            queueLen.sxHosts.nic.minCnt.append(nicQueueLenMin)
            queueLen.sxHosts.nic.count.append(nicQueueLenCnt)
            queueLen.sxHosts.nic.empty.append(nicQueueLenEmpty)
            queueLen.sxHosts.nic.onePkt.append(nicQueueLenOnePkt)
            queueLen.sxHosts.nic.maxCnt.append(nicQueueLenMax)
            queueLen.sxHosts.nic.meanCnt.append(nicQueueLenMean)
            queueLen.sxHosts.nic.stddevCnt.append(nicQueueLenStddev)
            queueLen.sxHosts.nic.minBytes.append(nicQueueBytesMin)
            queueLen.sxHosts.nic.maxBytes.append(nicQueueBytesMax)
            queueLen.sxHosts.nic.meanBytes.append(nicQueueBytesMean)
            queueLen.sxHosts.nic.stddevBytes.append(nicQueueBytesStddev)

    numServersPerTor = int(parsedStats.generalInfo.numServersPerTor)
    fabricLinkSpeed = int(parsedStats.generalInfo.fabricLinkSpeed.strip('Gbps'))
    nicLinkSpeed = int(parsedStats.generalInfo.nicLinkSpeed.strip('Gbps'))
    numTorUplinkNics = int(floor(numServersPerTor * nicLinkSpeed / fabricLinkSpeed))
    senderHostIds = xmlParsedDic.senderIds
    senderTorIds = [elem for elem in set([int(id / numServersPerTor) for id in senderHostIds])]
    receiverHostIds = xmlParsedDic.receiverIds
    receiverTorIdsIfaces = [(int(id / numServersPerTor), id % numServersPerTor) for id in receiverHostIds]

    for torKey in parsedStats.tors.keys():
        tor = parsedStats.tors[torKey]
        torId = int(re.match('tor\[([0-9]+)]', torKey).group(1))
        for ifaceId in range(0, numServersPerTor + numTorUplinkNics):
            nicQueueLenEmpty = tor.access('eth[{0}].queue.dataQueue.\"queue empty (%)\".value'.format(ifaceId))
            nicQueueLenOnePkt = tor.access('eth[{0}].queue.dataQueue.\"queue length one (%)\".value'.format(ifaceId))
            nicQueueLenMin = tor.access('eth[{0}].queue.dataQueue.queueLength:stats.min'.format(ifaceId))
            nicQueueLenCnt = tor.access('eth[{0}].queue.dataQueue.queueLength:stats.count'.format(ifaceId))
            nicQueueLenMax = tor.access('eth[{0}].queue.dataQueue.queueLength:stats.max'.format(ifaceId))
            nicQueueLenMean = tor.access('eth[{0}].queue.dataQueue.queueLength:stats.mean'.format(ifaceId))
            nicQueueLenStddev = tor.access('eth[{0}].queue.dataQueue.queueLength:stats.stddev'.format(ifaceId))
            nicQueueBytesMin = tor.access('eth[{0}].queue.dataQueue.queueByteLength:stats.min'.format(ifaceId))/2**10
            nicQueueBytesMax = tor.access('eth[{0}].queue.dataQueue.queueByteLength:stats.max'.format(ifaceId))/2**10
            nicQueueBytesMean = tor.access('eth[{0}].queue.dataQueue.queueByteLength:stats.mean'.format(ifaceId))/2**10
            nicQueueBytesStddev = tor.access('eth[{0}].queue.dataQueue.queueByteLength:stats.stddev'.format(ifaceId))/2**10

            if ifaceId < numServersPerTor:
                queueLen.tors.down.nic.minCnt.append(nicQueueLenMin)
                queueLen.tors.down.nic.count.append(nicQueueLenCnt)
                queueLen.tors.down.nic.empty.append(nicQueueLenEmpty)
                queueLen.tors.down.nic.onePkt.append(nicQueueLenOnePkt)
                queueLen.tors.down.nic.maxCnt.append(nicQueueLenMax)
                queueLen.tors.down.nic.meanCnt.append(nicQueueLenMean)
                queueLen.tors.down.nic.stddevCnt.append(nicQueueLenStddev)
                queueLen.tors.down.nic.minBytes.append(nicQueueBytesMin)
                queueLen.tors.down.nic.maxBytes.append(nicQueueBytesMax)
                queueLen.tors.down.nic.meanBytes.append(nicQueueBytesMean)
                queueLen.tors.down.nic.stddevBytes.append(nicQueueBytesStddev)

                if (torId, ifaceId) in receiverTorIdsIfaces:
                    queueLen.rxTors.down.nic.minCnt.append(nicQueueLenMin)
                    queueLen.rxTors.down.nic.count.append(nicQueueLenCnt)
                    queueLen.rxTors.down.nic.empty.append(nicQueueLenEmpty)
                    queueLen.rxTors.down.nic.onePkt.append(nicQueueLenOnePkt)
                    queueLen.rxTors.down.nic.maxCnt.append(nicQueueLenMax)
                    queueLen.rxTors.down.nic.meanCnt.append(nicQueueLenMean)
                    queueLen.rxTors.down.nic.stddevCnt.append(nicQueueLenStddev)
                    queueLen.rxTors.down.nic.minBytes.append(nicQueueBytesMin)
                    queueLen.rxTors.down.nic.maxBytes.append(nicQueueBytesMax)
                    queueLen.rxTors.down.nic.meanBytes.append(nicQueueBytesMean)
                    queueLen.rxTors.down.nic.stddevBytes.append(nicQueueBytesStddev)

            else:
                queueLen.tors.up.nic.minCnt.append(nicQueueLenMin)
                queueLen.tors.up.nic.count.append(nicQueueLenCnt)
                queueLen.tors.up.nic.empty.append(nicQueueLenEmpty)
                queueLen.tors.up.nic.onePkt.append(nicQueueLenOnePkt)
                queueLen.tors.up.nic.maxCnt.append(nicQueueLenMax)
                queueLen.tors.up.nic.meanCnt.append(nicQueueLenMean)
                queueLen.tors.up.nic.stddevCnt.append(nicQueueLenStddev)
                queueLen.tors.up.nic.minBytes.append(nicQueueBytesMin)
                queueLen.tors.up.nic.maxBytes.append(nicQueueBytesMax)
                queueLen.tors.up.nic.meanBytes.append(nicQueueBytesMean)
                queueLen.tors.up.nic.stddevBytes.append(nicQueueBytesStddev)

                if torId in senderTorIds:
                    queueLen.sxTors.up.nic.minCnt.append(nicQueueLenMin)
                    queueLen.sxTors.up.nic.count.append(nicQueueLenCnt)
                    queueLen.sxTors.up.nic.empty.append(nicQueueLenEmpty)
                    queueLen.sxTors.up.nic.onePkt.append(nicQueueLenOnePkt)
                    queueLen.sxTors.up.nic.maxCnt.append(nicQueueLenMax)
                    queueLen.sxTors.up.nic.meanCnt.append(nicQueueLenMean)
                    queueLen.sxTors.up.nic.stddevCnt.append(nicQueueLenStddev)
                    queueLen.sxTors.up.nic.minBytes.append(nicQueueBytesMin)
                    queueLen.sxTors.up.nic.maxBytes.append(nicQueueBytesMax)
                    queueLen.sxTors.up.nic.meanBytes.append(nicQueueBytesMean)
                    queueLen.sxTors.up.nic.stddevBytes.append(nicQueueBytesStddev)

    for aggrKey in parsedStats.aggrs.keys():
        aggr = parsedStats.aggrs[aggrKey]
        aggrId = int(re.match('aggRouter\[([0-9]+)]', aggrKey).group(1))
        for ifaceId in range(0, int(parsedStats.generalInfo.numTors)):
            nicQueueLenEmpty = aggr.access('eth[{0}].queue.dataQueue.\"queue empty (%)\".value'.format(ifaceId))
            nicQueueLenOnePkt = aggr.access('eth[{0}].queue.dataQueue.\"queue length one (%)\".value'.format(ifaceId))
            nicQueueLenMin = aggr.access('eth[{0}].queue.dataQueue.queueLength:stats.min'.format(ifaceId))
            nicQueueLenCnt = aggr.access('eth[{0}].queue.dataQueue.queueLength:stats.count'.format(ifaceId))
            nicQueueLenMax = aggr.access('eth[{0}].queue.dataQueue.queueLength:stats.max'.format(ifaceId))
            nicQueueLenMean = aggr.access('eth[{0}].queue.dataQueue.queueLength:stats.mean'.format(ifaceId))
            nicQueueLenStddev = aggr.access('eth[{0}].queue.dataQueue.queueLength:stats.stddev'.format(ifaceId))
            nicQueueBytesMin = aggr.access('eth[{0}].queue.dataQueue.queueByteLength:stats.min'.format(ifaceId))/2**10
            nicQueueBytesMax = aggr.access('eth[{0}].queue.dataQueue.queueByteLength:stats.max'.format(ifaceId))/2**10
            nicQueueBytesMean = aggr.access('eth[{0}].queue.dataQueue.queueByteLength:stats.mean'.format(ifaceId))/2**10
            nicQueueBytesStddev = aggr.access('eth[{0}].queue.dataQueue.queueByteLength:stats.stddev'.format(ifaceId))/2**10

            queueLen.aggrs.nic.minCnt.append(nicQueueLenMin)
            queueLen.aggrs.nic.count.append(nicQueueLenCnt)
            queueLen.aggrs.nic.empty.append(nicQueueLenEmpty)
            queueLen.aggrs.nic.onePkt.append(nicQueueLenOnePkt)
            queueLen.aggrs.nic.maxCnt.append(nicQueueLenMax)
            queueLen.aggrs.nic.meanCnt.append(nicQueueLenMean)
            queueLen.aggrs.nic.stddevCnt.append(nicQueueLenStddev)
            queueLen.aggrs.nic.minBytes.append(nicQueueBytesMin)
            queueLen.aggrs.nic.maxBytes.append(nicQueueBytesMax)
            queueLen.aggrs.nic.meanBytes.append(nicQueueBytesMean)
            queueLen.aggrs.nic.stddevBytes.append(nicQueueBytesStddev)

    digestQueueLenInfo(queueLen.sxHosts.transport, 'SX Transports')
    digestQueueLenInfo(queueLen.sxHosts.nic, 'SX NICs')
    digestQueueLenInfo(queueLen.hosts.nic, 'All NICs')
    digestQueueLenInfo(queueLen.sxTors.up.nic, 'SX TORs Up')
    digestQueueLenInfo(queueLen.tors.up.nic, 'All TORs Up')
    digestQueueLenInfo(queueLen.rxTors.down.nic, 'RX TORs Down')
    digestQueueLenInfo(queueLen.tors.down.nic, 'All TORs Down')
    digestQueueLenInfo(queueLen.aggrs.nic, 'All AGGRs')
    return  queueLen

def printQueueLength(queueLen):
    printKeys = ['meanCnt', 'stddevCnt', 'meanBytes', 'stddevBytes', 'empty', 'onePkt', 'minCnt', 'minBytes', 'maxCnt', 'maxBytes']
    tw = 15
    fw = 9
    lineMax = 105
    title = 'Queue Length (Stats Collected At Pkt Arrivals)'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))
    print("="*lineMax)
    print("Queue Location".ljust(tw) + 'Mean'.center(fw) + 'StdDev'.center(fw) + 'Mean'.center(fw) + 'StdDev'.center(fw) +
             'Empty'.center(fw) + 'OnePkt'.center(fw) + 'Min'.center(fw) + 'Min'.center(fw) + 'Max'.center(fw) + 'Max'.center(fw))
    print("".ljust(tw) + '(Pkts)'.center(fw) + '(Pkts)'.center(fw) + '(KB)'.center(fw) + '(KB)'.center(fw) +
             '%'.center(fw) + '%'.center(fw) + '(Pkts)'.center(fw) + '(KB)'.center(fw) + '(Pkts)'.center(fw) + '(KB)'.center(fw))
    print("_"*lineMax)

    printStatsLine(queueLen.sxHosts.transport.queueLenDigest, queueLen.sxHosts.transport.queueLenDigest.title, tw, fw, '', printKeys)
    printStatsLine(queueLen.sxHosts.nic.queueLenDigest, queueLen.sxHosts.nic.queueLenDigest.title, tw, fw, '', printKeys)
    printStatsLine(queueLen.hosts.nic.queueLenDigest, queueLen.hosts.nic.queueLenDigest.title, tw, fw, '', printKeys)
    printStatsLine(queueLen.sxTors.up.nic.queueLenDigest, queueLen.sxTors.up.nic.queueLenDigest.title, tw, fw, '', printKeys)
    printStatsLine(queueLen.tors.up.nic.queueLenDigest, queueLen.tors.up.nic.queueLenDigest.title, tw, fw, '', printKeys)
    printStatsLine(queueLen.aggrs.nic.queueLenDigest, queueLen.aggrs.nic.queueLenDigest.title, tw, fw, '', printKeys)
    printStatsLine(queueLen.rxTors.down.nic.queueLenDigest, queueLen.rxTors.down.nic.queueLenDigest.title, tw, fw, '', printKeys)
    printStatsLine(queueLen.tors.down.nic.queueLenDigest, queueLen.tors.down.nic.queueLenDigest.title, tw, fw, '', printKeys)

def main():
    parser = OptionParser()
    options, args = parser.parse_args()
    if len(args) > 0:
        scalarResultFile = args[0]
    else:
        scalarResultFile = 'homatransport/src/dcntopo/results/RecordAllStats-0.sca'

    if len(args) > 1:
        xmlConfigFile = args[1]
    else:
        xmlConfigFile = 'homatransport/src/dcntopo/config.xml'

    sp = ScalarParser(scalarResultFile)
    parsedStats = AttrDict()
    parsedStats.hosts = sp.hosts
    parsedStats.tors = sp.tors
    parsedStats.aggrs = sp.aggrs
    parsedStats.cores = sp.cores
    parsedStats.generalInfo = sp.generalInfo
    parsedStats.globalListener = sp.globalListener

    xmlParsedDic = AttrDict()
    xmlParsedDic = parseXmlFile(xmlConfigFile, parsedStats.generalInfo)

    queueWaitTimeDigest = AttrDict()
    hostQueueWaitTimes(parsedStats.hosts, xmlParsedDic, queueWaitTimeDigest)
    torsQueueWaitTime(parsedStats.tors, parsedStats.generalInfo, xmlParsedDic, queueWaitTimeDigest)
    aggrsQueueWaitTime(parsedStats.aggrs, parsedStats.generalInfo, xmlParsedDic, queueWaitTimeDigest)
    printGenralInfo(xmlParsedDic, parsedStats.generalInfo)
    if parsedStats.generalInfo.transportSchemeType == 'HomaTransport':
        printHomaOutstandingBytes(parsedStats, xmlParsedDic, 'KB')
    trafficDic = computeHomaRates(parsedStats, xmlParsedDic)
    printHomaRates(trafficDic)
    if parsedStats.generalInfo.transportSchemeType == 'HomaTransport':
        activeAndWasted = computeWastedTimesAndBw(parsedStats, xmlParsedDic)
        selfWasteTime = computeSelfInflictedWastedBw(parsedStats, xmlParsedDic)
        printWastedTimeAndBw(parsedStats, xmlParsedDic, activeAndWasted, selfWasteTime)
    if parsedStats.generalInfo.transportSchemeType == 'HomaTransport':
        prioUsageStatsDigest = list()
        computePrioUsageStats(parsedStats.hosts, parsedStats.generalInfo, xmlParsedDic, prioUsageStatsDigest)
        printPrioUsageStats(prioUsageStatsDigest)
    trafficDic = computeBytesAndRates(parsedStats, xmlParsedDic)
    printBytesAndRates(trafficDic)
    queueLen = computeQueueLength(parsedStats, xmlParsedDic)
    printQueueLength(queueLen)
    printQueueTimeStats(queueWaitTimeDigest, 'us')
    msgBytesOnWireDigest = AttrDict()
    msgBytesOnWire(parsedStats, xmlParsedDic, msgBytesOnWireDigest)

    transportSchedDelayDigest = AttrDict()
    transportSchedDelay(parsedStats, xmlParsedDic, msgBytesOnWireDigest, transportSchedDelayDigest)
    printTransportSchedDelay(transportSchedDelayDigest, 'us')

    e2eStretchAndDelayDigest = AttrDict()
    e2eStretchAndDelay(parsedStats, xmlParsedDic, msgBytesOnWireDigest, e2eStretchAndDelayDigest)
    printE2EStretchAndDelay(e2eStretchAndDelayDigest, 'us')

if __name__ == '__main__':
    sys.exit(main());
