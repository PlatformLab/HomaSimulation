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

def printStatsLine(statsDic, rowTitle, tw, fw, unit, printKeys):
    if unit == 'us':
        scaleFac = 1e6 
    elif unit == '':
        scaleFac = 1

    printStr = rowTitle.ljust(tw)
    for key in printKeys:
        if key in statsDic.keys():
            if key == 'count':
                printStr += '{0}'.format(int(statsDic.access('count'))).center(fw)
            elif key == 'meanFrac':
                printStr += '{0:.2f}'.format(statsDic.access(key)).center(fw)
            else:
                printStr += '{0:.2f}'.format(statsDic.access(key) * scaleFac).center(fw)
    print(printStr)


def printQueueTimeStats(queueWaitTimeDigest, unit):

    printKeys = ['mean', 'meanFrac', 'stddev', 'min', 'median', 'threeQuartile', 'ninety9Percentile', 'max', 'count']
    tw = 20
    fw = 12
    lineMax = 140
    title = 'Queue Wait Time Stats'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' ')) 

    print('\n' + "Packet Type: Requst".center(lineMax,' ') + '\n' + "="*lineMax)
    print("Queue Location".ljust(tw) + 'mean({0})'.format(unit).center(fw) + 'mean%'.center(fw) + 'stddev({0})'.format(unit).center(fw) +
            'min({0})'.format(unit).center(fw) + 'median({0})'.format(unit).center(fw) + '75%ile({0})'.format(unit).center(fw) +
            '99%ile({0})'.format(unit).center(fw) + 'max({0})'.format(unit).center(fw) + 'count'.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.requestQueueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.requestQueueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.requestQueueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.requestQueueingTime
    meanSum = hostStats.mean + torsUpStats.mean + torsDownStats.mean + aggrsStats.mean
    for moduleStats in [hostStats, torsUpStats, torsDownStats, aggrsStats]:
        moduleStats.meanFrac = 0 if meanSum==0 else 100*moduleStats.mean/meanSum

    printStatsLine(hostStats, 'Host NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsUpStats, 'TORs upward NICs:', tw, fw, unit, printKeys)
    printStatsLine(aggrsStats, 'Aggr Switch NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsDownStats, 'TORs downward NICs:', tw, fw, unit, printKeys)
    print('_'*2*tw + '\n' + 'Sum:'.ljust(tw) + '{0:.2f}'.format(meanSum*1e6).center(fw))

    print('\n\n' + "Packet Type: Grant".center(lineMax,' ') + '\n' + "="*lineMax)
    print("Queue Location".ljust(tw) + 'mean({0})'.format(unit).center(fw) + 'mean%'.center(fw) + 'stddev({0})'.format(unit).center(fw) +
            'min({0})'.format(unit).center(fw) + 'median({0})'.format(unit).center(fw) + '75%ile({0})'.format(unit).center(fw) +
            '99%ile({0})'.format(unit).center(fw) + 'max({0})'.format(unit).center(fw) + 'count'.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.grantQueueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.grantQueueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.grantQueueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.grantQueueingTime
    meanSum = hostStats.mean + torsUpStats.mean + torsDownStats.mean + aggrsStats.mean
    for moduleStats in [hostStats, torsUpStats, torsDownStats, aggrsStats]:
        moduleStats.meanFrac = 0 if meanSum==0 else 100*moduleStats.mean/meanSum


    printStatsLine(hostStats, 'Host NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsUpStats, 'TORs upward NICs:', tw, fw, unit, printKeys)
    printStatsLine(aggrsStats, 'Aggr Switch NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsDownStats, 'TORs downward NICs:', tw, fw, unit, printKeys)
    print('_'*2*tw + '\n' + 'Sum:'.ljust(tw) + '{0:.2f}'.format(meanSum*1e6).center(fw))

    print('\n\n' + "Packet Type: Data".center(lineMax,' ') + '\n'  + "="*lineMax)
    print("Queue Location".ljust(tw) + 'mean({0})'.format(unit).center(fw) + 'mean%'.center(fw) + 'stddev({0})'.format(unit).center(fw) +
            'min({0})'.format(unit).center(fw) + 'median({0})'.format(unit).center(fw) + '75%ile({0})'.format(unit).center(fw) +
            '99%ile({0})'.format(unit).center(fw) + 'max({0})'.format(unit).center(fw) + 'count'.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.dataQueueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.dataQueueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.dataQueueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.dataQueueingTime
    meanSum = hostStats.mean + torsUpStats.mean + torsDownStats.mean + aggrsStats.mean
    for moduleStats in [hostStats, torsUpStats, torsDownStats, aggrsStats]:
        moduleStats.meanFrac = 0 if meanSum==0 else 100*moduleStats.mean/meanSum

    printStatsLine(hostStats, 'Host NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsUpStats, 'TORs upward NICs:', tw, fw, unit, printKeys)
    printStatsLine(aggrsStats, 'Aggr Switch NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsDownStats, 'TORs downward NICs:', tw, fw, unit, printKeys)
    print('_'*2*tw + '\n' + 'Sum:'.ljust(tw) + '{0:.2f}'.format(meanSum*1e6).center(fw))

    print('\n\n' + "packet Type: All Pkts".center(lineMax,' ') + '\n' + "="*lineMax)
    print("Queue Location".ljust(tw) + 'mean({0})'.format(unit).center(fw) + 'mean%'.center(fw) + 'stddev({0})'.format(unit).center(fw) +
            'min({0})'.format(unit).center(fw) + 'median({0})'.format(unit).center(fw) + '75%ile({0})'.format(unit).center(fw) +
            '99%ile({0})'.format(unit).center(fw) + 'max({0})'.format(unit).center(fw) + 'count'.center(fw))
    print("_"*lineMax)
    hostStats = queueWaitTimeDigest.queueWaitTime.hosts.queueingTime
    torsUpStats = queueWaitTimeDigest.queueWaitTime.tors.upward.queueingTime
    torsDownStats = queueWaitTimeDigest.queueWaitTime.tors.downward.queueingTime
    aggrsStats = queueWaitTimeDigest.queueWaitTime.aggrs.queueingTime
    meanSum = hostStats.mean + torsUpStats.mean + torsDownStats.mean + aggrsStats.mean
    for moduleStats in [hostStats, torsUpStats, torsDownStats, aggrsStats]:
        moduleStats.meanFrac = 0 if meanSum==0 else 100*moduleStats.mean/meanSum

    printStatsLine(hostStats, 'SX Host NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsUpStats, 'SX TORs UP NICs:', tw, fw, unit, printKeys)
    printStatsLine(aggrsStats, 'Aggr Switch NICs:', tw, fw, unit, printKeys)
    printStatsLine(torsDownStats, 'RX TORs Down NICs:', tw, fw, unit, printKeys)
    print('_'*2*tw + '\n' + 'Sum:'.ljust(tw) + '{0:.2f}'.format(meanSum*1e6).center(fw))


def printE2EStretchAndDelay(e2eStretchAndDelayDigest, unit):
    printKeys = ['mean', 'meanFrac', 'stddev', 'min', 'median', 'threeQuartile', 'ninety9Percentile', 'max', 'count']
    tw = 20
    fw = 12
    lineMax = 140
    title = 'End To End Message Delays For Different Ranges of Message Sizes'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' ')) 

    print("="*lineMax)
    print("Msg Size Range".ljust(tw) + 'mean({0})'.format(unit).center(fw) + 'stddev({0})'.format(unit).center(fw) + 'min({0})'.format(unit).center(fw) +
            'median({0})'.format(unit).center(fw) + '75%ile({0})'.format(unit).center(fw) + '99%ile({0})'.format(unit).center(fw) +
            'max({0})'.format(unit).center(fw) + 'count'.center(fw))
    print("_"*lineMax)
    
    end2EndDelayDigest = e2eStretchAndDelayDigest.delay
    sizeLowBound = ['0'] + [e2eSizedDelay.sizeUpBound for e2eSizedDelay in end2EndDelayDigest[0:len(end2EndDelayDigest)-1]]
    for i, e2eSizedDelay in enumerate(end2EndDelayDigest):
        printStatsLine(e2eSizedDelay, '({0}, {1}]'.format(sizeLowBound[i], e2eSizedDelay.sizeUpBound), tw, fw, 'us', printKeys)

    title = 'End To End Message Stretch For Different Ranges of Message Sizes'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' ')) 

    print("="*lineMax)
    print("Msg Size Range".ljust(tw) + 'mean'.center(fw) + 'stddev'.center(fw) + 'min'.center(fw) +
            'median'.center(fw) + '75%ile'.center(fw) + '99%ile'.center(fw) + 'max'.center(fw) + 'count'.center(fw))
    print("_"*lineMax)
    
    end2EndStretchDigest = e2eStretchAndDelayDigest.stretch
    sizeLowBound = ['0'] + [e2eSizedStretch.sizeUpBound for e2eSizedStretch in end2EndStretchDigest[0:len(end2EndStretchDigest)-1]]
    for i, e2eSizedStretch in enumerate(end2EndStretchDigest):
        printStatsLine(e2eSizedStretch, '({0}, {1}]'.format(sizeLowBound[i], e2eSizedStretch.sizeUpBound), tw, fw, '', printKeys)



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

        e2eDelayDigest.sizeUpBound = '{0}'.format(size)
        e2eStretchAndDelayDigest.delay.append(e2eDelayDigest)
        e2eStretchDigest.sizeUpBound = '{0}'.format(size)
        e2eStretchAndDelayDigest.stretch.append(e2eStretchDigest)
    return e2eStretchAndDelayDigest

def printGenralInfo(xmlParsedDic):
    tw = 20
    fw = 12
    lineMax = 140
    title = 'General Simulation Information'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' ')) 
    print('Num Servers Per TOR:'.ljust(tw) + '{0}'.format(xmlParsedDic.numServersPerTor).center(fw))
    print('Num TORs:'.ljust(tw) + '{0}'.format(xmlParsedDic.numTors).center(fw))
    print('Server Link Speed:'.ljust(tw) + '{0}Gb/s'.format(xmlParsedDic.nicLinkSpeed).center(fw))
    print('Fabric Link Speed:'.ljust(tw) + '{0}Gb/s'.format(xmlParsedDic.fabricLinkSpeed).center(fw))

def digestTrafficInfo(trafficBytesAndRateDic, title):
    trafficDigest = trafficBytesAndRateDic.trafficDigest
    trafficDigest.title = title 
    if 'bytes' in trafficBytesAndRateDic.keys():
        trafficDigest.cumBytes = sum(trafficBytesAndRateDic.bytes)
    if 'rates' in trafficBytesAndRateDic.keys():
        trafficDigest.cumRate = sum(trafficBytesAndRateDic.rates)
        trafficDigest.avgRate = trafficDigest.cumRate/float(len(trafficBytesAndRateDic.rates)) 
        trafficDigest.minRate = min(trafficBytesAndRateDic.rates)
        trafficDigest.maxRate = max(trafficBytesAndRateDic.rates)
    if 'dutyCycles' in trafficBytesAndRateDic.keys():
        trafficDigest.avgDutyCycle = sum(trafficBytesAndRateDic.dutyCycles)/float(len(trafficBytesAndRateDic.dutyCycles)) 
        trafficDigest.minDutyCycle = min(trafficBytesAndRateDic.dutyCycles)
        trafficDigest.maxDutyCycle = max(trafficBytesAndRateDic.dutyCycles)

def printBytesAndRates(parsedStats, xmlParsedDic):
    printKeys = ['avgRate', 'cumRate', 'minRate', 'maxRate', 'cumBytes', 'avgDutyCycle', 'minDutyCycle', 'maxDutyCycle']
    tw = 20
    fw = 15
    lineMax = 140
    title = 'Generated Bytes and Rates'
    print('\n'*2 + ('-'*len(title)).center(lineMax,' ') + '\n' + ('|' + title + '|').center(lineMax, ' ') +
            '\n' + ('-'*len(title)).center(lineMax,' '))
    trafficDic = AttrDict()
    trafficDic.hostsTraffic.tx.apps.bytes = []
    trafficDic.hostsTraffic.tx.apps.rates = []
    trafficDic.hostsTraffic.rx.apps.bytes = []
    trafficDic.hostsTraffic.rx.apps.rates = []
    trafficDic.hostsTraffic.tx.nics.bytes = []
    trafficDic.hostsTraffic.tx.nics.rates = []
    trafficDic.hostsTraffic.tx.nics.dutyCycles = []
    trafficDic.hostsTraffic.rx.nics.bytes = []
    trafficDic.hostsTraffic.rx.nics.rates = []
    trafficDic.hostsTraffic.rx.nics.dutyCycles = []

    for host in parsedStats.hosts.keys():
        hostId = int(re.match('host\[([0-9]+)]', host).group(1))
        if hostId in xmlParsedDic.senderIds:
            senderStats = parsedStats.hosts[host]
            txAppsBytes = trafficDic.hostsTraffic.tx.apps.bytes 
            txAppsRates = trafficDic.hostsTraffic.tx.apps.rates
            txAppsBytes.append(senderStats.access('trafficGeneratorApp[0].sentMsg:sum(packetBytes).value'))
            txAppsRates.append(senderStats.access('trafficGeneratorApp[0].sentMsg:last(sumPerDuration(packetBytes)).value')*8.0/1e9)
            txNicsBytes = trafficDic.hostsTraffic.tx.nics.bytes 
            txNicsRates = trafficDic.hostsTraffic.tx.nics.rates
            txNicsDutyCycles = trafficDic.hostsTraffic.tx.nics.dutyCycles
            txNicsBytes.append(senderStats.access('eth[0].mac.txPk:sum(packetBytes).value'))
            txNicsRates.append(senderStats.access('eth[0].mac.\"bits/sec sent\".value')/1e9)
            txNicsDutyCycles.append(senderStats.access('eth[0].mac.\"tx channel utilization (%)\".value'))
        if hostId in xmlParsedDic.receiverIds:
            receiverStats = parsedStats.hosts[host] 
            rxAppsBytes = trafficDic.hostsTraffic.rx.apps.bytes 
            rxAppsRates = trafficDic.hostsTraffic.rx.apps.rates
            rxAppsBytes.append(receiverStats.access('trafficGeneratorApp[0].rcvdMsg:sum(packetBytes).value'))
            rxAppsRates.append(receiverStats.access('trafficGeneratorApp[0].rcvdMsg:last(sumPerDuration(packetBytes)).value')*8.0/1e9)
            rxNicsBytes = trafficDic.hostsTraffic.rx.nics.bytes 
            rxNicsRates = trafficDic.hostsTraffic.rx.nics.rates
            rxNicsDutyCycles = trafficDic.hostsTraffic.rx.nics.dutyCycles
            rxNicsBytes.append(receiverStats.access('eth[0].mac.rxPkOk:sum(packetBytes).value'))
            rxNicsRates.append(receiverStats.access('eth[0].mac.\"bits/sec rcvd\".value')/1e9)
            rxNicsDutyCycles.append(receiverStats.access('eth[0].mac.\"rx channel utilization (%)\".value'))





    print("="*lineMax)
    print("Measurement Point".ljust(tw) + 'AvgRate(Gb/s)'.center(fw) + 'CumRate(Gb/s)'.center(fw) + 'MinRate(Gb/s)'.center(fw) + 'MaxRate(Gb/s)'.center(fw) +
             'CumBytes'.center(fw) + '%AvgDutyCycle'.center(fw) + '%MinDutyCycle'.center(fw) + '%MaxDutyCycle'.center(fw))
    print("_"*lineMax)
    digestTrafficInfo(trafficDic.hostsTraffic.tx.apps, 'TX Host Apps:')
    printStatsLine(trafficDic.hostsTraffic.tx.apps.trafficDigest, trafficDic.hostsTraffic.tx.apps.trafficDigest.title, tw, fw, '', printKeys)
    digestTrafficInfo(trafficDic.hostsTraffic.tx.nics, 'TX Host NICs:')
    printStatsLine(trafficDic.hostsTraffic.tx.nics.trafficDigest, trafficDic.hostsTraffic.tx.nics.trafficDigest.title, tw, fw, '', printKeys)


    digestTrafficInfo(trafficDic.hostsTraffic.rx.nics, 'RX Host NICs:')
    printStatsLine(trafficDic.hostsTraffic.rx.nics.trafficDigest, trafficDic.hostsTraffic.rx.nics.trafficDigest.title, tw, fw, '', printKeys)
    digestTrafficInfo(trafficDic.hostsTraffic.rx.apps, 'RX Host Apps:')
    printStatsLine(trafficDic.hostsTraffic.rx.apps.trafficDigest, trafficDic.hostsTraffic.rx.apps.trafficDigest.title, tw, fw, '', printKeys)

   
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
    parsedStats = AttrDict() 
    parsedStats.hosts, parsedStats.tors, parsedStats.aggrs, parsedStats.cores  = parse(open(scalarResultFile))
    #pprint(parsedStats)
    queueWaitTimeDigest = AttrDict()
    hostQueueWaitTimes(parsedStats.hosts, xmlParsedDic, queueWaitTimeDigest)
    torsQueueWaitTime(parsedStats.tors, xmlParsedDic, queueWaitTimeDigest)
    aggrsQueueWaitTime(parsedStats.aggrs, xmlParsedDic, queueWaitTimeDigest)
    printGenralInfo(xmlParsedDic)
    printBytesAndRates(parsedStats, xmlParsedDic)
    printQueueTimeStats(queueWaitTimeDigest, 'us')
    e2eStretchAndDelayDigest = AttrDict()
    e2eStretchAndDelay(parsedStats.hosts, xmlParsedDic, e2eStretchAndDelayDigest)
    printE2EStretchAndDelay(e2eStretchAndDelayDigest, 'us')

if __name__ == '__main__':
    sys.exit(main());
