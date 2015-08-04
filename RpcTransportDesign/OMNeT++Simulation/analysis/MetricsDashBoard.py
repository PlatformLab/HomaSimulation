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

__all__ = ['parse', 'copyExclude']

class AttrDict(dict):
    """A mapping with string keys that aliases x.y syntax to x['y'] syntax.
    The attribute syntax is easier to read and type than the item syntax.
    """
    def __getattr__(self, name):
        if name not in self:
            self[name] = AttrDict()
        return self[name]
    def __setattr__(self, name, value):
        self[name] = value
    def __delattr__(self, name):
        del self[name]
    def assign(self, path, value):
        """
        Given a hierarchical path such as 'x.y.z' and
        a value, perform an assignment as if the statement
        self.x.y.z had been invoked.
        """
        names = path.split('.')
        container = self
        for name in names[0:-1]:
            if name not in container:
                container[name] = AttrDict()
            container = container[name]
        container[names[-1]] = value
    def access(self, path):
        """
        Given a hierarchical path such as 'x.y.z' returns the value as if the
        statement self.x.y.z had been invoked.
        """
        names = path.split('.')
        container = self
        for name in names[0:-1]:
            if name not in container:
                raise Exception, 'path does not exist: {0}'.format(path)
                container[name] = AttrDict()
            container = container[name]
        return container[names[-1]]

def parse(f):
    """
    Scan a result file containing scalar statistics for omnetsimulation, and
    returns a list of AttrDicts, one containing the metrics for each server.
    """
    hosts = AttrDict() 
    tors = AttrDict()
    aggrs = AttrDict()
    cores = AttrDict()
    currDict = AttrDict()
    for line in f:
        match = re.match('attr\s+network\s+(\S+)',line)
        if match:
            net = match.group(1)
            break
    if not match:
        raise Exception, 'no network name in file: {0}'.format(f.name)
    for line in f:
        match = re.match('(\S+)\s+{0}\.(([a-zA-Z]+).+\.\S+)\s+(".+"|\S+)\s*(\S*)'.format(net), line)
        if match:
            topLevelModule = match.group(3)
            if topLevelModule == 'tor':
                currDict = tors
            elif topLevelModule == 'host':
                currDict = hosts
            elif topLevelModule == 'aggRouter':
                currDict = aggrs
            elif topLevelModule == 'core':
                currDict = cores
            else:
                raise Exception, 'no such module defined for parser: {0}'.format(topLevelModule)
            entryType = match.group(1)
            if entryType == 'statistic':
                var = match.group(2)+'.'+match.group(4)
                currDict.assign(var+'.bins', [])
            elif entryType == 'scalar':
                var = match.group(2)+'.'+match.group(4)
                subVar = var + '.value' 
                value = float(match.group(5))
                currDict.assign(subVar, value)
            else:
                raise Exception, '{0}: not defined for this parser'.format(match.group(1))
            continue
        match = re.match('(\S+)\s+(".+"|\S+)\s+(".+"|\S+)', line)        
        if not match and not line.isspace():
            warnings.warn('Parser cant find a match for line: {0}'.format(line), RuntimeWarning) 
        if currDict:
            entryType = match.group(1)
            subVar = var + '.' + match.group(2)
            value = match.group(3) 
            if entryType == 'field':
                currDict.assign(subVar, float(value))
            elif entryType == 'attr':
                currDict.assign(subVar, value)
            elif entryType == 'bin':
                subVar = var + '.bins'
                valuePair = (float(match.group(2)), float(match.group(3)))
                currDict.access(subVar).append(valuePair)
            else:
                warnings.warn('Entry type not known to parser: {0}'.format(entryType), RuntimeWarning) 
    return hosts, tors, aggrs, cores

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
    if idx == len(bins)-1:
        return bins[idx]
    return (bins[idx] + bins[idx + 1])/2

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
        moduleStats.median = getStatsFromHist(bins, cumProb, medianIdx)
        threeQuartileIdx = next(idx for idx,value in enumerate(cumProb) if value >= 0.75)
        moduleStats.threeQuartile = getStatsFromHist(bins, cumProb, threeQuartileIdx)
        ninety9PercentileIdx = next(idx for idx,value in enumerate(cumProb) if value >= 0.99)
        moduleStats.ninety9Percentile = getStatsFromHist(bins, cumProb, ninety9PercentileIdx)
    return moduleStats

def digestModulesStats(modulesStatsList):
    statsDigest = AttrDict()
    statsDigest = statsDigest.fromkeys(modulesStatsList[0].keys(), 0.0) 
    for targetStat in modulesStatsList:
        statsDigest.count += targetStat.count 
        statsDigest.min += targetStat.min / len(modulesStatsList) 
        statsDigest.max = max(targetStat.max, statsDigest.max)
        statsDigest.mean += targetStat.mean / len(modulesStatsList) 
        statsDigest.stddev += targetStat.stddev / len(modulesStatsList) 
        statsDigest.median += targetStat.median / len(modulesStatsList) 
        statsDigest.threeQuartile += targetStat.threeQuartile / len(modulesStatsList) 
        statsDigest.ninety9Percentile += targetStat.ninety9Percentile / len(modulesStatsList) 
    return statsDigest


def hostQueueWaitTimes(hosts, xmlParsedDic, reportDigest):
    senderIds = xmlParsedDic.senderIds
    # find the queueWaitTimes for different types of packets. Current types
    # considered are request, grant and data packets. Also queueingTimes in the
    # senders NIC.
    keyStrings = ['queueingTime','dataQueueingTime','grantQueueingTime','requestQueueingTime']
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
        if queuingTimeDigest.count != 0:
            reportDigest.assign('queueWaitTime.hosts.{0}'.format(keyString), queuingTimeDigest)

def torsQueueWaitTime(tors, xmlParsedDic, reportDigest):
    senderHostIds = xmlParsedDic.senderIds
    senderTorIds = [elem for elem in set([int(id / xmlParsedDic.numServersPerTor) for id in senderHostIds])]
    numTorUplinkNics = int(floor(xmlParsedDic.numServersPerTor * xmlParsedDic.nicLinkSpeed / xmlParsedDic.fabricLinkSpeed))
    numServersPerTor = xmlParsedDic.numServersPerTor
    receiverHostIds = xmlParsedDic.receiverIds
    receiverTorIdsIfaces = [(int(id / xmlParsedDic.numServersPerTor), id % xmlParsedDic.numServersPerTor) for id in receiverHostIds]
    keyStrings = ['queueingTime','dataQueueingTime','grantQueueingTime','requestQueueingTime']
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
        if torsUpwardQueuingTimeDigest.count != 0:
            reportDigest.assign('queueWaitTime.tors.upward.{0}'.format(keyString), torsUpwardQueuingTimeDigest)

        torsDownwardQueuingTimeDigest = AttrDict()
        torsDownwardQueuingTimeDigest = digestModulesStats(torsDownwardQueuingTimeStats)
        if torsDownwardQueuingTimeDigest.count != 0:
            reportDigest.assign('queueWaitTime.tors.downward.{0}'.format(keyString), torsDownwardQueuingTimeDigest)

def aggrsQueueWaitTime(aggrs, xmlParsedDic, reportDigest):
    # Find the queue waiting for aggrs switches NICs
    keyStrings = ['queueingTime','dataQueueingTime','grantQueueingTime','requestQueueingTime']
    for keyString in keyStrings:
        aggrsQueuingTimeStats = list()
        for aggr in aggrs.keys():
            for ifaceId in range(0, xmlParsedDic.numTors):
                queuingTimeHistogramKey = '{0}.eth[{1}].queue.dataQueue.{2}:histogram.bins'.format(aggr, ifaceId,  keyString)
                queuingTimeStatsKey = '{0}.eth[{1}].queue.dataQueue.{2}:stats'.format(aggr, ifaceId, keyString)
                aggrsStats = AttrDict()
                aggrsStats = getInterestingModuleStats(aggrs, queuingTimeStatsKey, queuingTimeHistogramKey)
                aggrsQueuingTimeStats.append(aggrsStats)
        
        aggrsQueuingTimeDigest = AttrDict() 
        aggrsQueuingTimeDigest = digestModulesStats(aggrsQueuingTimeStats)
        if aggrsQueuingTimeDigest.count != 0: 
            reportDigest.assign('queueWaitTime.aggrs.{0}'.format(keyString), aggrsQueuingTimeDigest)

def parseXmlFile(xmlConfigFile):
    xmlConfig = minidom.parse(xmlConfigFile)
    xmlParsedDic = AttrDict()
    numServersPerTor = int(xmlConfig.getElementsByTagName('numServersPerTor')[0].firstChild.data)
    numTors = int(xmlConfig.getElementsByTagName('numTors')[0].firstChild.data)
    fabricLinkSpeed = int(xmlConfig.getElementsByTagName('fabricLinkSpeed')[0].firstChild.data)
    nicLinkSpeed = int(xmlConfig.getElementsByTagName('nicLinkSpeed')[0].firstChild.data)
    numTors = int(xmlConfig.getElementsByTagName('numTors')[0].firstChild.data)
    xmlParsedDic.numServersPerTor = numServersPerTor
    xmlParsedDic.numTors = numTors 
    xmlParsedDic.fabricLinkSpeed = fabricLinkSpeed 
    xmlParsedDic.nicLinkSpeed = nicLinkSpeed 
    senderIds = list()
    receiverIds = list()
    allHostsReceive = False
    for hostConfig in xmlConfig.getElementsByTagName('hostConfig'):
        isSender = hostConfig.getElementsByTagName('isSender')[0]
        if isSender.childNodes[0].data == 'true':
            senderIds.append(int(hostConfig.getAttribute('id'))) 
            if allHostsReceive is False:
                destIdsNode = hostConfig.getElementsByTagName('destIds')[0]
                destIds = list()
                if destIdsNode.firstChild != None:
                    destIds = [int(destId) for destId in destIdsNode.firstChild.data.split()]
                if destIds == []:
                    allHostsReceive = True     
                else:
                    receiverIds += destIds
    xmlParsedDic.senderIds = senderIds
    if allHostsReceive is True: 
        receiverIds = range(0, numTors*numServersPerTor)
    xmlParsedDic.receiverIds = [elem for elem in set(receiverIds)]
    return xmlParsedDic

def printStatsLine(statsDic, rowTitle, tw, fw, unit):
    if unit == 'us':
        scaleFac = 1e6 
    elif unit == '':
        scaleFac = 1

    print(rowTitle.ljust(tw)  + '{0}'.format(int(statsDic.count)).center(fw)+ '{0:.2f}{1}'.format(statsDic.min * scaleFac, unit).center(fw) + 
            '{0:.2f}{1}'.format(statsDic.mean * scaleFac, unit).center(fw) + '{0:.2f}{1}'.format(statsDic.stddev * scaleFac, unit).center(fw) +
            '{0:.2f}{1}'.format(statsDic.median * scaleFac, unit).center(fw) + '{0:.2f}{1}'.format(statsDic.threeQuartile * scaleFac, unit).center(fw) +
            '{0:.2f}{1}'.format(statsDic.ninety9Percentile * scaleFac, unit).center(fw) + '{0:.2f}{1}'.format(statsDic.max * scaleFac, unit).center(fw))

def printQueueTimeStats(queueWaitTimeDigest, unit):

    tw = 50
    fw = 14
    lineMax = 160
    print('\n'*2 + ''.center(lineMax,'*') + '\n' + 'Queue Wait Time Stats'.center(lineMax,'*') + '\n' + ''.center(lineMax,'*'))
    print('\n\n' + "="*lineMax)
    print("Request Pkts Queue Wait Time Stats:".ljust(tw) + 'count'.center(fw) + 'min'.center(fw) + 'mean'.center(fw) + 'stddev'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) + 'max'.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.requestQueueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.requestQueueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.requestQueueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.requestQueueingTime
    printStatsLine(hostStats, 'At Host NICs:', tw, fw, unit)
    printStatsLine(torsUpStats, 'At TORs upward NICs:', tw, fw, unit)
    printStatsLine(aggrsStats, 'At Aggr Switch NICs:', tw, fw, unit)
    printStatsLine(torsDownStats, 'At TORs downward NICs:', tw, fw, unit)

    print('\n\n' + "="*lineMax)
    print("Grant Pkts Queue Wait Time Stats:".ljust(tw) + 'count'.center(fw) + 'min'.center(fw) + 'mean'.center(fw) + 'stddev'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) + 'max'.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.grantQueueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.grantQueueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.grantQueueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.grantQueueingTime
    printStatsLine(hostStats, 'At Host NICs:', tw, fw, unit)
    printStatsLine(torsUpStats, 'At TORs upward NICs:', tw, fw, unit)
    printStatsLine(aggrsStats, 'At Aggr Switch NICs:', tw, fw, unit)
    printStatsLine(torsDownStats, 'At TORs downward NICs:', tw, fw, unit)

    print('\n\n' + "="*lineMax)
    print("Data Pkts Queue Wait Time Stats:".ljust(tw) + 'count'.center(fw) + 'min'.center(fw) + 'mean'.center(fw) + 'stddev'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) + 'max'.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.dataQueueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.dataQueueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.dataQueueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.dataQueueingTime
    printStatsLine(hostStats, 'At Host NICs:', tw, fw, unit)
    printStatsLine(torsUpStats, 'At TORs upward NICs:', tw, fw, unit)
    printStatsLine(aggrsStats, 'At Aggr Switch NICs:', tw, fw, unit)
    printStatsLine(torsDownStats, 'At TORs downward NICs:', tw, fw, unit)

    print('\n\n' + "="*lineMax)
    print("Total Pkts Queue Wait Time Stats:".ljust(tw) + 'count'.center(fw) + 'min'.center(fw) + 'mean'.center(fw) + 'stddev'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) + 'max'.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.queueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.queueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.queueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.queueingTime
    printStatsLine(hostStats, 'At Sender Host NICs:', tw, fw, unit)
    printStatsLine(torsUpStats, 'At Sender TORs upward NICs:', tw, fw, unit)
    printStatsLine(aggrsStats, 'At Aggr Switch NICs:', tw, fw, unit)
    printStatsLine(torsDownStats, 'At Receiver TORs downward NICs:', tw, fw, unit)


def printE2EStretchAndDelay(e2eStretchAndDelayDigest, unit):
    tw = 50
    fw = 14
    lineMax = 160
    print('\n'*2 + ''.center(lineMax,'*') + '\n' + 'End To End Message Delays For Different Ranges of Message Sizes'.center(lineMax, '*') + '\n' + ''.center(lineMax,'*'))
    print('\n\n' + "="*lineMax)
    print("End to End Delay Stats:".ljust(tw) + 'count'.center(fw) + 'min'.center(fw) + 'mean'.center(fw) + 'stddev'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) + 'max'.center(fw))
    print("_"*lineMax)
    
    end2EndDelayDigest = e2eStretchAndDelayDigest.delay
    for e2eSizedDelay in end2EndDelayDigest:
        printStatsLine(e2eSizedDelay, e2eSizedDelay.sizeUpBound, tw, fw, 'us')

    print('\n'*2 + ''.center(lineMax,'*') + '\n' + 'End To End Message Stretch For Different Ranges of Message Sizes'.center(lineMax,'*') + '\n' + ''.center(lineMax,'*'))
    print('\n\n' + "="*lineMax)
    print("End to End Stretch Stats:".ljust(tw) + 'count'.center(fw) + 'min'.center(fw) + 'mean'.center(fw) + 'stddev'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) + 'max'.center(fw))
    print("_"*lineMax)
    
    end2EndStretchDigest = e2eStretchAndDelayDigest.stretch
    for e2eSizedStretch in end2EndStretchDigest:
        printStatsLine(e2eSizedStretch, e2eSizedStretch.sizeUpBound, tw, fw, '')



def e2eStretchAndDelay(hosts, xmlParsedDic, e2eStretchAndDelayDigest):
    # For the hosts that are receivers, find the stretch and endToend stats and
    # return them. 
    receiverHostIds = xmlParsedDic.receiverIds 
    sizes = ['1Pkt', '3Pkts', '6Pkts', '13Pkts', '33Pkts', '133Pkts', '1333Pkts', 'Huge']
    e2eStretchAndDelayDigest.delay = []
    e2eStretchAndDelayDigest.stretch = []
    for size in sizes:
        e2eDelayList = list()
        e2eStretchList = list()
        for id in receiverHostIds:
            e2eDelayHistogramKey = 'host[{0}].trafficGeneratorApp[0].msg{1}E2EDelay:histogram.bins'.format(id, size)
            e2eDelayStatsKey = 'host[{0}].trafficGeneratorApp[0].msg{1}E2EDelay:stats'.format(id, size)
            e2eStretchHistogramKey = 'host[{0}].trafficGeneratorApp[0].msg{1}E2EStretch:histogram.bins'.format(id, size)
            e2eStretchStatsKey = 'host[{0}].trafficGeneratorApp[0].msg{1}E2EStretch:stats'.format(id, size)
            e2eDelayForSize = AttrDict()
            e2eStretchForSize = AttrDict()
            e2eDelayForSize = getInterestingModuleStats(hosts, e2eDelayStatsKey, e2eDelayHistogramKey)
            e2eStretchForSize = getInterestingModuleStats(hosts, e2eStretchStatsKey, e2eStretchHistogramKey)
            e2eDelayList.append(e2eDelayForSize)
            e2eStretchList.append(e2eStretchForSize)

        e2eDelayDigest = AttrDict()
        e2eStretchDigest = AttrDict()
        e2eDelayDigest = digestModulesStats(e2eDelayList)
        e2eStretchDigest = digestModulesStats(e2eStretchList)
        if e2eDelayDigest.count != 0:
            e2eDelayDigest.sizeUpBound = 'Msg Size (Less than){0}'.format(size)
            e2eStretchAndDelayDigest.delay.append(e2eDelayDigest)
        if e2eStretchDigest.count != 0: 
            e2eStretchDigest.sizeUpBound = 'Msg Size (Less than){0}'.format(size)
            e2eStretchAndDelayDigest.stretch.append(e2eStretchDigest)
    return e2eStretchAndDelayDigest

def main():
    parser = OptionParser()
    options, args = parser.parse_args()
    if len(args) > 0:
        scalarResultFile = args[0]
    else:
        scalarResultFile = 'homatransport/src/dcntopo/results/RecordAllStats-0.sca'

    xmlConfigFile = 'homatransport/src/dcntopo/config.xml' 
    xmlParsedDic = AttrDict()
    xmlParsedDic = parseXmlFile(xmlConfigFile)
    
    hosts, tors, aggrs, cores  = parse(open(scalarResultFile))
    queueWaitTimeDigest = AttrDict()
    hostQueueWaitTimes(hosts, xmlParsedDic, queueWaitTimeDigest)
    torsQueueWaitTime(tors, xmlParsedDic, queueWaitTimeDigest)
    aggrsQueueWaitTime(aggrs, xmlParsedDic, queueWaitTimeDigest)
    printQueueTimeStats(queueWaitTimeDigest, 'us')

    e2eStretchAndDelayDigest = AttrDict()
    e2eStretchAndDelay(hosts, xmlParsedDic, e2eStretchAndDelayDigest)
    printE2EStretchAndDelay(e2eStretchAndDelayDigest, 'us')

if __name__ == '__main__':
    sys.exit(main());
