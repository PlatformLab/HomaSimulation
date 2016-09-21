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

resultDir = os.environ['HOME'] + '/Research/RpcTransportDesign/OMNeT++Simulation/homatransport/src/dcntopo/results/paper/manyReceivers'
f = open(resultDir + '/fileList.txt')
resultFiles = [line.rstrip('\n') for line in f]
f.close()
f = open(os.environ['HOME'] + "/Research/RpcTransportDesign"+
    "/OMNeT++Simulation/analysis/PlotScripts/wastedBw.txt", 'w')
tw_h = 40
tw_l = 40
f.write('workload'.center(tw_l) + 'redundancyFactor'.center(tw_h) +
    'loadFactor'.center(tw_l) + 'wastedBw'.center(tw_l) + 'activeWastedBw'.center(tw_l)+'\n')


wastedBw = []
for dirFile in resultFiles:
    filename = resultDir + '/' + dirFile
    sp = ScalarParser(filename) 
    parsedStats = AttrDict()
    parsedStats.hosts = sp.hosts
    parsedStats.tors = sp.tors
    parsedStats.aggrs = sp.aggrs
    parsedStats.cores = sp.cores
    parsedStats.generalInfo = sp.generalInfo
    redundancyFactor = parsedStats.generalInfo['numSendersToKeepGranted'] 
    workloadType =  parsedStats.generalInfo['workloadType']
    xmlConfigFile =  os.environ['HOME'] + '/Research/RpcTransportDesign/OMNeT++Simulation/homatransport/src/dcntopo/config.xml'
    xmlParsedDic = AttrDict()
    xmlParsedDic = parseXmlFile(xmlConfigFile,parsedStats.generalInfo)
    activeAndWasted = calculateWastedTimesAndBw(parsedStats, xmlParsedDic)
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
    ethInterArrivalGapBit = 12*8.0
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
    digestTrafficInfo(trafficDic.rxHostsTraffic.nics.rx, 'RX NICs Recv:')
    loadFactor =  100* trafficDic.rxHostsTraffic.nics.rx.trafficDigest.avgRate/ int(parsedStats.generalInfo.nicLinkSpeed.strip('Gbps')) 
    wastedBw.append((workloadType, redundancyFactor, loadFactor, activeAndWasted.rx.fracTotalTime, activeAndWasted.rx.fracActiveTime))

for wasteTuple in wastedBw:
    f.write('{0}'.format(wasteTuple[0]).center(tw_l) + '{0}'.format(wasteTuple[1]).center(tw_h) +
            '{0}'.format(wasteTuple[2]).center(tw_l) + '{0}'.format(wasteTuple[3]).center(tw_l) + '{0}'.format(wasteTuple[4]).center(tw_l)+ '\n')

f.close()

