#!/usr/bin/python

"""
This script provides a set of tools to parse the vector and scalar files dumped
out of omnet simulation and indexed data and stats for use of other modules.
"""

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
            container = container[name]
        return container[names[-1]]


class VectorParser():
    """
    open vector and index files and provide interface for seeking and reading
    through them
    """
    def __init__(self, vciFile, vecFile):
        self.vciFile = vciFile
        self.vecFile = vecFile
        self.paramDic = AttrDict()
        self.vecDesc = AttrDict()
        self.vecBlockInfo = AttrDict()
        self.parseIndex()

    def parseIndex(self):
        vciFd = open(self.vciFile)
        vciFd.seek(0)
        paramListEnd = False
        net=""
        vecId = -1
        vecPath = ""
        for line in vciFd:
            if not(paramListEnd):
                # At the top of the index file, the general information about
                # parameter list of the simulation is expected. This list is
                # expected to be separated by a blank line from the rest of the
                # file
                if not line or line.isspace():
                    paramListEnd = True

                match = re.match('attr\s+(\S+)\s+(".+"|\S+)', line)
                if match:
                    self.paramDic.assign(match.group(1), match.group(2))
                    if match.group(1) == 'network':
                        net = match.group(2)
            else:
                # After the parameter list, we expect only one of the three
                # types of line in the index file:
                #
                # 1. Lines that starts with 'vector' and contain information
                #    about id number, source, and name of the vector
                # 2. Each line that starts with vector, will be followed by few
                #    other lines that start with 'attr'
                # 3. lines that start with a vector id number of a vector and
                #    stores the location and stats of blocks for that vector in
                #    the vectror result file

                match1 = re.match(
                    '{0}\s+(\d+)\s+{1}\.(\S+)\s+("(.+)"|\S+)\s*(\S*)'.format(
                    'vector', net), line)
                match2 = re.match('attr\s+(\S+)\s+("(.+)"|\S+)', line)
                match3 = re.match('(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+'
                    '(\d+\.?\d*)\s+(\d+\.?\d*)\s+(\d+)\s+(\d+\.?\d*)\s+'
                    '(\d+\.?\d*)\s+(\d+\.?\d*)\s+(\d+\.?\d*e?\+?\d*)', line)
                if match1:
                    vecId = int(match1.group(1))
                    moduleName = match1.group(2)
                    vecName = match1.group(3)
                    vecPath = moduleName+'.'+vecName
                    self.vecDesc.assign(vecPath, AttrDict())
                    self.vecDesc.access(vecPath).vecId = vecId
                elif match2:
                    if match2.group(1) == 'source':
                        self.vecDesc.access(vecPath).srcSignal = match2.group(2)
                    elif match2.group(1) == 'title':
                        if match2.group(3) == None:
                            self.vecDesc.access(vecPath).title = match2.group(2)
                        else:
                            self.vecDesc.access(vecPath).title = match2.group(3)
                elif match3:
                    vecId = int(match3.group(1))
                    try:
                        self.vecBlockInfo[vecId]
                    except KeyError:
                        self.vecBlockInfo[vecId] = AttrDict()
                        self.vecBlockInfo[vecId].offsets = []
                        self.vecBlockInfo[vecId].bytelens = []
                        self.vecBlockInfo[vecId].firstEventNos = []
                        self.vecBlockInfo[vecId].lastEventNos = []
                        self.vecBlockInfo[vecId].firstSimTimes = []
                        self.vecBlockInfo[vecId].lastSimTimes = []
                        self.vecBlockInfo[vecId].counts = []
                        self.vecBlockInfo[vecId].mins = []
                        self.vecBlockInfo[vecId].maxs = []
                        self.vecBlockInfo[vecId].sums = []
                        self.vecBlockInfo[vecId].sqrsums = []
                    self.vecBlockInfo[vecId].offsets.append(int(match3.group(2)))
                    self.vecBlockInfo[vecId].bytelens.append(int(match3.group(3)))
                    self.vecBlockInfo[vecId].firstEventNos.append(int(match3.group(4)))
                    self.vecBlockInfo[vecId].lastEventNos.append(int(match3.group(5)))
                    self.vecBlockInfo[vecId].firstSimTimes.append(float(match3.group(6)))
                    self.vecBlockInfo[vecId].lastSimTimes.append(float(match3.group(7)))
                    self.vecBlockInfo[vecId].counts.append(int(match3.group(8)))
                    self.vecBlockInfo[vecId].mins.append(float(match3.group(9)))
                    self.vecBlockInfo[vecId].maxs.append(float(match3.group(10)))
                    self.vecBlockInfo[vecId].sums.append(float(match3.group(11)))
                    self.vecBlockInfo[vecId].sqrsums.append(float(match3.group(12)))
                else:
                    raise Exception, "No match pattern found for line:\n{0}".format(line)
        vciFd.close()

    def getVecDicRecursive(self, attrdict, vecName):
        """
        returns a list of tuples (t1, t2) where for each module having vector
        vecName, t1 is the module path in the network and t2 is a dictionary
        containing vecId, source signal and title of the vector correspoding to
        vecName in that module.
        """
        retList = list()
        for key in attrdict:
            if key == vecName:
                retList.append(('', attrdict.access(key)))
            else:
                nextLevelDict = attrdict.access(key)
                if not(type(nextLevelDict) == type(AttrDict())):
                    nextLevelDict = AttrDict()
                recList = self.getVecDicRecursive(nextLevelDict, vecName)
                for pair in recList:
                    if not(pair[1] == AttrDict()):
                        if pair[0] == '':
                            retList.append((key, pair[1]))
                        else:
                            retList.append((key + '.' + pair[0], pair[1]))
        if retList == list():
            return [('', AttrDict())]
        else:
            return retList

    def getIdsByNames(self, vecNames, moduleNames = ''):
        if moduleNames == '':
            # Returns all of the instances of the vecNames in vecDesc for all
            # possible modules that record results from vector 'vecName'
            vecIdDic = AttrDict()
            for vecName in vecNames:
                vecIds = []
                vecIdTupleList = self.getVecDicRecursive(self.vecDesc, vecName)
                for vecIdPair in vecIdTupleList:
                    if not(vecIdPair[0] == ''):
                        vecIds.append(vecIdPair[1].vecId)
                vecIdDic.assign(vecName, vecIds)
            return vecIdDic

        else:
            vecIdDic = AttrDict()
            for vecName in vecNames:
                vecIds = []
                for moduleName in moduleNames:
                    try:
                        vecDic = self.vecDesc.access(moduleName)
                        vecIds.append(vecDic.access(vecName).vecId)
                    except KeyError:
                        pass
                vecIdDic.assign(vecName, vecIds)
            return vecIdDic

    def getVecDataByIds(self, vecId):
        """
        Returns a list of values recorded for the 'vecId' sorted by recording
        time. Performs relavent checks to make sure the results are valid based
        on the information provided in vecBlockInfo for that vecId
        """
        vecFd = open(self.vecFile)
        blockInfo = self.vecBlockInfo[vecId]
        vecData = []
        prevEvenNo = 0
        prevSimTime = 0
        for firstEventNo in blockInfo.firstEventNos:
            currentBlockData = []
            i = blockInfo.firstEventNos.index(firstEventNo)
            offset = blockInfo.offsets[i]
            bytelen = blockInfo.bytelens[i]
            lastEventNo = blockInfo.lastEventNos[i]
            firstSimTime = blockInfo.firstSimTimes[i]
            lastSimTime = blockInfo.lastSimTimes[i]
            vecFd.seek(offset)
            for line in vecFd.read(bytelen).splitlines():
                recordedVals = line.split()
                assert int(recordedVals[0]) == vecId
                eventNo = int(recordedVals[1])
                simTime = float(recordedVals[2])
                val = recordedVals[3]
                currentBlockData.append((eventNo, simTime, val))
                assert eventNo >= prevEvenNo and simTime >= prevSimTime
                prevEvenNo = eventNo
                prevSimTime = simTime
            assert currentBlockData[0][0] == firstEventNo
            assert currentBlockData[0][1] == firstSimTime
            assert currentBlockData[-1][0] == lastEventNo
            assert currentBlockData[-1][1] == lastSimTime
            vecData += currentBlockData
        vecFd.close()
        return vecData

class ScalarParser():
    """
    Scan a result file containing scalar statistics for omnet simulation, and
    returns a list of AttrDicts, one containing the metrics for each server.
    """
    def __init__(self, scaFile):
        self.scaFile = scaFile
        self.hosts = AttrDict()
        self.tors = AttrDict()
        self.aggrs = AttrDict()
        self.cores = AttrDict()
        self.generalInfo = AttrDict()
        self.globalListener = AttrDict()
        self.parse()


    def parse(self):
        self.hosts = AttrDict()
        self.tors = AttrDict()
        self.aggrs = AttrDict()
        self.cores = AttrDict()
        self.generalInfo = AttrDict()
        self.globalListener = AttrDict()
        net = ""
        scaFd = open(self.scaFile)
        for line in scaFd:
            if not line or line.isspace():
                break;
            match = re.match('attr\s+(\S+)\s+(".+"|\S+)', line)
            if match:
                self.generalInfo.assign(match.group(1), match.group(2))
                if match.group(1) == 'network':
                    net = match.group(2)
        if net == "":
            raise Exception, 'no network name in file: {0}'.format(scaFd.name)

        currDict = AttrDict()
        for line in scaFd:
            match = re.match('(\S+)\s+{0}\.(([a-zA-Z]+).+\.\S+)\s+(".+"|\S+)\s*(\S*)'.format(net), line)
            if match:
                topLevelModule = match.group(3)
                if topLevelModule == 'tor':
                    currDict = self.tors
                elif topLevelModule == 'host':
                    currDict = self.hosts
                elif topLevelModule == 'aggRouter':
                    currDict = self.aggrs
                elif topLevelModule == 'core':
                    currDict = self.cores
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

            match = re.match('(\S+)\s+{0}\.([a-zA-Z]+)\s+(".+"|\S+)\s*(\S*)'.format(net), line)
            if match:
                topLevelModule = match.group(2)
                if topLevelModule == 'globalListener':
                    currDict = self.globalListener
                else:
                    raise Exception, 'no such module defined for parser: {0}'.format(topLevelModule)
                entryType = match.group(1)
                if entryType == 'statistic':
                    var = match.group(3)
                    currDict.assign(var+'.bins', [])
                elif entryType == 'scalar':
                    var = match.group(3)
                    subVar = var + '.value'
                    value = float(match.group(4))
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
        scaFd.close()
