#!/usr/bin/python
"""
This program scans the scaler result file (.sca) and printouts some of the
statistics on the screen.
"""

from glob import glob
from optparse import OptionParser
from pprint import pprint
from functools import partial
import math
import os
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
        if not match:
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
#    for line in f:
#        match = re.match('(\S+)\s+{0}\.(([a-zA-Z]+).+\.\S+)\s+(".+"|\S+)\s*(\S*)'.format(net), line)
#        if match:
#            topLevelModule = match.group(3)
#            if topLevelModule == 'tor':
#                currDict = tors
#            elif topLevelModule == 'host':
#                currDict = hosts
#            elif topLevelModule == 'aggRouter':
#                currDict = aggrs
#            elif topLevelModule == 'core':
#                currDict = cores
#            else:
#                raise Exception, 'no such module defined for parser: {0}'.format(topLevelModule)
#            entryType = match.group(1)
#            var = re.sub([[\]""], '', match.group(2))
#            var = re.sub([^\w\.], '_', var)
#            subVar = re.sub([""], '', match.group(4))
#            subVar = re.sub([^\w\.], '_', subVar)
#            if entryType == 'statistic':
#                var = var+'.'+subVar
#                currDict.assign(var+'.bins', [])
#            elif entryType == 'scalar':
#                var = var+'.'+subVar
#                subVar = var + '.value' 
#                value = float(match.group(5))
#                currDict.assign(subVar, value)
#            else:
#                raise Exception, '{0}: not defined for this parser'.format(match.group(1))
#            continue
#        match = re.match('(\S+)\s+(".+"|\S+)\s+(".+"|\S+)', line)        
#        if not match:
#            warnings.warn('Parser cant find a match for line: {0}'.format(line), RuntimeWarning) 
#        if currDict:
#            entryType = match.group(1)
#            if entryType == 'bin':
#                subVar = var + '.bins'
#                valuePair = (float(match.group(2), float(match.group(3))))
#                currDict.access(subVar).append(valuePair)
#            subVar = re.sub([""], '', match.group(2))
#            subVar = re.sub([^\w\.], '_', subVar)
#            subVar = var + '.' + subVar 
#            value = match.group(3) 
#            if entryType == 'field':
#                currDict.assign(subVar, float(value))
#            elif entryType == 'attr':
#                currDict.assign(subVar, value)
#            
#            else:
#                warnings.warn('Entry type not known to parser: {0}'.format(entryType), RuntimeWarning) 
#    return hosts, tors, aggrs, cores


def copyExclude(source, dest, exclude):
    selectKeys = (key for key in source if key not in exclude)
    for key in selectKeys:
        if (isinstance(source[key], AttrDict)):
            dest[key] = AttrDict()
            copyExclude(source[key], dest[key], exclude)
        else:
            dest[key] = source[key]

def main():
    parser = OptionParser()
    options, args = parser.parse_args()
    if len(args) > 0:
        scalarResultFile = args[0]
    else:
        scalarResultFile = 'homatransport/src/dcntopo/results/RecordAllStats-0.sca'

    hosts, tors, aggrs, cores  = parse(open(scalarResultFile))
    hostsNoHistogram = AttrDict()
    torsNoHistogram = AttrDict()
    aggrsNoHistogram = AttrDict()
    coresNotHistogram = AttrDict()
    exclude = ['bins', 'interpolationmode', 'interpolationMode', '"simulated time"']
    copyExclude(hosts, hostsNoHistogram, exclude)
    pprint(hostsNoHistogram)
    
if __name__ == '__main__':
    sys.exit(main());
