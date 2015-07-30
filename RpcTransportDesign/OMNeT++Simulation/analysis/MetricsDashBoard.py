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


def hostQueueWaitTimes(hosts, xmlParsedDic):
    senderIds = xmlParsedDic.senderIds
    sendersQueuingTimeStats = list()
    for host in hosts.keys():
        hostId = int(re.match('host\[([0-9]+)]', host).group(1))
        if hostId in senderIds:
            queuingTimeHistogramKey = 'host[{0}].eth[0].queue.dataQueue.queueingTime:histogram.bins'.format(hostId)
            queuingTimeStatsKey = 'host[{0}].eth[0].queue.dataQueue.queueingTime:stats'.format(hostId)
            senderStats = AttrDict()
            senderStats = getInterestingModuleStats(hosts, queuingTimeStatsKey, queuingTimeHistogramKey)
            sendersQueuingTimeStats.append(senderStats)
    
    sendersQueuingTimeDigest = AttrDict() 
    sendersQueuingTimeDigest = digestModulesStats(sendersQueuingTimeStats)
    reportDigest = AttrDict()
    reportDigest.assign("Queue Waiting Time in Senders NICs", sendersQueuingTimeDigest)
    return reportDigest

def torsQueueWaitTime(tors, xmlParsedDic):
    # Find the queue waiting times for the upward NICs of sender tors.
    # For that we first need to find torIds for all the tors
    # connected to the sender hosts
    senderHostIds = xmlParsedDic.senderIds
    senderTorIds = [elem for elem in set([int(id / xmlParsedDic.numServersPerTor) for id in senderHostIds])]
    numTorUplinkNics = int(floor(xmlParsedDic.numServersPerTor * xmlParsedDic.nicLinkSpeed / xmlParsedDic.fabricLinkSpeed))
    numServersPerTor = xmlParsedDic.numServersPerTor

    torsUpwardQueuingTimeStats = list()
    for torKey in tors.keys():
        torId = int(re.match('tor\[([0-9]+)]', torKey).group(1))
        if torId in senderTorIds:
            tor = tors[torKey]
            # Find the queuewait time only for the upward tor NICs
            for ifaceId in range(numServersPerTor, numServersPerTor + numTorUplinkNics):
                queuingTimeHistogramKey = 'eth[{0}].queue.dataQueue.queueingTime:histogram.bins'.format(ifaceId)
                queuingTimeStatsKey = 'eth[{0}].queue.dataQueue.queueingTime:stats'.format(ifaceId)            
                torUpwardStat = AttrDict()
                torUpwardStat = getInterestingModuleStats(tor, queuingTimeStatsKey, queuingTimeHistogramKey)
                torsUpwardQueuingTimeStats.append(torUpwardStat)
   
    torsUpwardQueuingTimeDigest = AttrDict()
    torsUpwardQueuingTimeDigest = digestModulesStats(torsUpwardQueuingTimeStats)
    reportDigest = AttrDict()
    reportDigest.assign("Queue Waiting Time in TOR upward NICs", torsUpwardQueuingTimeDigest)

    # Find the queue waiting times for the downward NIC of the receiver tors
    # For that we fist need to find the torIds for all the tors that are 
    # connected to the receiver hosts
    receiverHostIds = xmlParsedDic.receiverIds
    receiverTorIdsIfaces = [(int(id / xmlParsedDic.numServersPerTor), id % xmlParsedDic.numServersPerTor) for id in receiverHostIds]
    torsDownwardQueuingTimeStats = list()
    for torId, ifaceId in receiverTorIdsIfaces:
        queuingTimeHistogramKey = 'tor[{0}].eth[{1}].queue.dataQueue.queueingTime:histogram.bins'.format(torId, ifaceId)
        queuingTimeStatsKey = 'tor[{0}].eth[{1}].queue.dataQueue.queueingTime:stats'.format(torId, ifaceId)            
        torDownwardStat = AttrDict() 
        torDownwardStat = getInterestingModuleStats(tors, queuingTimeStatsKey, queuingTimeHistogramKey)
        torsDownwardQueuingTimeStats.append(torDownwardStat)
 
    torsDownwardQueuingTimeDigest = AttrDict()
    torsDownwardQueuingTimeDigest = digestModulesStats(torsDownwardQueuingTimeStats)
    reportDigest.assign("Queue Waiting Time in TOR downward NICs", torsDownwardQueuingTimeDigest)
    return reportDigest

def aggrsQueueWaitTime(aggrs, xmlParsedDic):
    # Find the queue waiting for aggrs switches NICs
    aggrsQueuingTimeStats = list()
    for aggr in aggrs.keys():
        queuingTimeHistogramKey = '{0}.eth[0].queue.dataQueue.queueingTime:histogram.bins'.format(aggr)
        queuingTimeStatsKey = '{0}.eth[0].queue.dataQueue.queueingTime:stats'.format(aggr)
        aggrsStats = AttrDict()
        aggrsStats = getInterestingModuleStats(aggrs, queuingTimeStatsKey, queuingTimeHistogramKey)
        aggrsQueuingTimeStats.append(aggrsStats)
    
    aggrsQueuingTimeDigest = AttrDict() 
    aggrsQueuingTimeDigest = digestModulesStats(aggrsQueuingTimeStats)
    reportDigest = AttrDict()
    reportDigest.assign("Queue Waiting Time in Aggregate Switch NICs", aggrsQueuingTimeDigest)
    return reportDigest

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

def printStats(allStatsList, unit):
    if unit == 'us':
        scaleFac = 1e6 
    elif unit == '':
        scaleFac = 1

    statMaxTopicLen = 0
    for statDics in allStatsList:
        for key in statDics:
            statMaxTopicLen = max(len(key), statMaxTopicLen)
    statMaxTopicLen += 4
    for statDics in allStatsList:
        for key in statDics:
            print '\n'
            print (key + ':').ljust(statMaxTopicLen)
            print ''.rjust(statMaxTopicLen) + 'num sample points = ' + str(statDics[key].count)
            print ''.rjust(statMaxTopicLen) + 'minimum = ' + str(statDics[key].min) + unit
            print ''.rjust(statMaxTopicLen) + 'mean = ' + str(statDics[key].mean) + unit
            print ''.rjust(statMaxTopicLen) + 'stddev = ' + str(statDics[key].stddev) + unit
            print ''.rjust(statMaxTopicLen) + 'median = ' + str(statDics[key].median) + unit
            print ''.rjust(statMaxTopicLen) + '75percentile = ' + str(statDics[key].threeQuartile) + unit
            print ''.rjust(statMaxTopicLen) + '99percentile = ' + str(statDics[key].ninety9Percentile) + unit
            print ''.rjust(statMaxTopicLen) + 'max = '.format(unit) + str(statDics[key].max) + unit

def e2eStretchAndDelay(hosts, xmlParsedDic):
    # For the hosts that are receivers, find the stretch and endToend stats and
    # return them. 
    receiverHostIds = xmlParsedDic.receiverIds 
    sizes = ['1Pkt', '3Pkts', '6Pkts', '13Pkts', '33Pkts', '133Pkts', '1333Pkts', 'Huge']
    e2eDelayReportDigest = AttrDict()
    e2eStretchReportDigest = AttrDict()
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
        e2eDelayReportDigest.assign('End to End Delay for less than {0} messages'.format(size), e2eDelayDigest)
        e2eStretchReportDigest.assign('End to End Stretch for less than {0} messages'.format(size), e2eStretchDigest)
    return e2eDelayReportDigest, e2eStretchReportDigest

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
    #hostsNoHistogram = AttrDict()
    #torsNoHistogram = AttrDict()
    #aggrsNoHistogram = AttrDict()
    #coresNotHistogram = AttrDict()
    #exclude = ['bins', 'interpolationmode', 'interpolationMode', '"simulated time"']
    #copyExclude(hosts, hostsNoHistogram, exclude)
    #copyExclude(tors, torsNoHistogram, exclude)
    #copyExclude(aggrs, aggrsNoHistogram, exclude)
    #pprint(hosts)
    allStatsList = list()
    hostQueueWaitingDigest = AttrDict()
    hostQueueWaitingDigest = hostQueueWaitTimes(hosts, xmlParsedDic)
    allStatsList.append(hostQueueWaitingDigest)
    torQueueWaitingDigest = AttrDict()
    torQueueWaitingDigest = torsQueueWaitTime(tors, xmlParsedDic)
    allStatsList.append(torQueueWaitingDigest)
    aggrQueueWaitingDigest = AttrDict()
    aggrQueueWaitingDigest = aggrsQueueWaitTime(aggrs, xmlParsedDic)
    allStatsList.append(aggrQueueWaitingDigest)
    print("====================Packet Wait Times In The Queues====================")
    printStats(allStatsList, 'us')

    e2eDelayDigest = AttrDict()
    e2eStretchDigest = AttrDict()
    e2eDelayDigest, e2eStretchDigest = e2eStretchAndDelay(hosts, xmlParsedDic)
    print("====================End To End Delay====================")
    printStats([e2eDelayDigest], 'us')
    print("====================End To End Stretch====================")
    printStats([e2eStretchDigest], '')

if __name__ == '__main__':
    sys.exit(main());
