#!/usr/bin/python

"""
This script reads pfabric simulation output file, flow.tr, and calculates
stretch statistics for different message size ranges from the information
provided in the flow.tr.
"""

from __future__ import print_function
import sys
import signal
import os
import re
from numpy import *
from glob import glob
from optparse import OptionParser
from pprint import pprint
import bisect
import pdb
import textwrap
import time
import random
import socket
import subprocess
import threading
import multiprocessing



spt = 16 # servers per tor
tors = 9 # num tors per pod
aggrs = 4 # num aggregation switches per pod
pods = 1 # num pods
nicLinkSpeed = 10 # Gbps
fabricLinkSpeed = 40 # Gbps
switchFixDelay = 2.5e-7 # seconds
edgeLinkDelay = 0 # seconds
fabricLinkDelay = 0 # seconds
hostSwTurnAroundTime = 5e-7 # seconds
hostNicSxThinkTime = 5e-7 # seconds
isFabricCutThrough = False
unschedBytes = 10000
# directory under which output files of this script will be stored
targetDir = '/home/behnamm/Research/RpcTransportDesign/'\
    'ns2_Simulations/scripts/analysis'

rangesDic = {\
    "FABRICATED_HEAVY_MIDDLE" : [51, 52, 53, 54, 55, 56, 57, 58, 59, 60,\
    61, 62, 63, 64, 65, 66, 67, 68, 69, 71, 72, 73, 75, 76, 77, 79, 81,\
    82, 84, 86, 88, 90, 92, 94, 96, 99, 101, 104, 107, 110, 113, 116,\
    120, 124, 128, 132, 136, 141, 147, 152, 158, 165, 172, 180, 189, 198,\
    209, 220, 233, 248, 265, 284, 306, 331, 362, 398, 443, 499, 571, 667,\
    804, 1002, 1061, 1128, 1203, 1290, 1389, 1506, 1644, 1809, 2012, 2266,\
    2593, 3030, 3645, 4572, 6134, 7245, 7607, 8006, 8450, 8946, 9503,\
    10135, 10857, 11689, 12659, 13806, 15180, 16858, 18954, 21645, 25226,\
    30226, 37700, 50083, 316228] ,
    "FACEBOOK_KEY_VALUE" : [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,18,\
    20,22,25,28,31,35,39,44,49,55,61,68,76,85,95,107,119,133,149,167,186,\
    208,233,260,291,325,364,406,454,508,568,635,710,794,887,992,1109,1240,\
    1386,1550,1733,1937,2166,2421,2707,3027,3384,3783,4229,4729,5286,5910,\
    6608,7388,8259,9234,10324,11542,12904,16129,25198,49208,96093,293173,\
    572511,1000000] ,
    "FACEBOOK_HADOOP_ALL" : [ 130, 227, 248, 271, 297, 303, 308, 310, 313, 315,\
    318, 321, 326, 331, 335, 345, 350, 360, 371, 376, 395, 420, 435, 452,\
    463, 480, 491, 502, 513, 525, 537, 549, 561, 574, 587, 600, 607, 615,\
    630, 646, 662, 671, 685, 702, 719, 737, 762, 787, 825, 885, 960, 1063,\
    1177, 1303, 1425, 1559, 1624, 1865, 2862, 4582, 6387, 10373, 31946, 36844,\
    40382, 42493, 44531, 45913, 47435, 48609, 49408, 50632, 57684, 69014,\
    74266, 77041, 79595, 81565, 91238, 104584, 120373, 141975, 167112,\
    197911, 228718, 273075, 406302, 832442, 1660940, 3430822, 10000000] ,
    "GOOGLE_SEARCH_RPC" : [2, 3, 4, 8, 32, 34, 36, 38, 40, 43, 46,\
    49, 53, 58, 64, 67, 71, 75, 80, 85, 91, 98, 107, 116, 128, 135, 142, 151,\
    160, 171, 183, 197, 213, 233, 256, 269, 284, 301, 320, 341, 366, 394, 427,\
    465, 512, 539, 569, 602, 640, 683, 731, 788, 853, 931, 1024, 1078, 1365,\
    2048, 3151, 4096, 5120, 6302, 7447, 8192, 9102, 10240, 11703, 12603, 13653,\
    14895, 16384, 17246, 18204, 19275, 20480, 25206, 29789, 34493, 40960,\
    46811, 50412, 54613, 59578, 65536, 72818, 81920, 93623, 100825, 119156,\
    137971, 163840, 187246, 201649, 262144, 308405, 403298, 524288, 1071908,\
    3529904],
    "GOOGLE_ALL_RPC" : [3, 32, 36, 40, 46, 53, 64, 70, 77, 85, 96, 110,\
    128, 137, 146, 158, 171, 186, 205, 228, 256, 268, 282, 296, 313, 331, 352,\
    375, 402, 433, 469, 512, 531, 573, 597, 623, 683, 717, 755, 796, 843, 956,\
    1053, 1117, 1189, 1271, 1317, 1418, 1475, 1603, 1755, 2137, 2341, 2657,\
    3511, 4535, 5521, 7256, 9078, 10335, 13435, 16861, 21984, 25170, 30468,\
    40018, 45220, 50244, 55146, 60293, 65902, 70217, 75137, 80248, 85482,\
    90049, 95133, 100825, 110247, 120372, 129632, 140605, 150160, 160275,\
    170901, 180895, 190944, 200864, 249460, 300435, 350288, 401080, 450652,\
    501350, 603313, 701172, 805723, 907174, 1008790, 5114695, 10668901,\
    20000000],
    "DCTCP" : [1442, 2884, 4326, 5768, 7210, 8652, 10094, 11536, 12978, 15862,\
    17304, 18746, 21630, 23072, 24514, 25956, 27398, 28840, 31724, 34608,\
    36050, 37492, 40376, 41818, 44702, 46144, 47586, 50470, 53354, 54796,\
    57680, 59122, 62006, 63448, 66332, 67774, 70658, 72100, 74984, 76426,\
    93730, 109592, 126896, 142758, 160062, 175924, 191786, 269654, 346080,\
    423948, 500374, 576800, 654668, 731094, 808962, 885388, 961814, 1058428,\
    1155042, 1250214, 1346828, 1442000, 1538614, 1635228, 1730400, 1827014,\
    1922186, 2210586, 2498986, 2787386, 3075786, 3364186, 3652586, 3940986,\
    4229386, 4517786, 4806186, 11537442, 13459628, 15381814, 17305442,\
    19227628, 21149814, 23073442, 24995628, 26917814, 28840000]}

Workloads = {'googleallrpc' : 'GOOGLE_ALL_RPC',
             'googlerpc' : 'GOOGLE_SEARCH_RPC',
             'heavymiddle' : 'FABRICATED_HEAVY_MIDDLE',
             'keyvalue' : 'FACEBOOK_KEY_VALUE',
             'facebookhadoop' : 'FACEBOOK_HADOOP_ALL',
             'search' : 'DCTCP'}

def serverIdToIp(id):
    numServersPerPod = int(spt*tors)
    podId = int(id)/numServersPerPod
    serverIdInPod = int(id) % numServersPerPod
    torId = serverIdInPod / spt
    serverIdInTor = serverIdInPod % spt
    ipStr = "10.{0}.{1}.{2}".format(podId, torId, serverIdInTor)
    ipInt = 10*(1<<24) + podId*(1<<16) + torId*(1<<8) + serverIdInTor
    return ipInt

def getMinMct_generalized(txStart, txPkts, sendrIntIp, recvrIntIp,
        prevPktArrivals=None):
    """
    For a train of txPkts as [pkt0.byteOnWire, pkt1.byteOnWire,.. ] that start
    transmission at sender at time txStart, this method returns the arrival time
    of last bit of this train at the receiver. prevPktArrivals is the arrival
    times of txPkts train prior this one for this message.
    """
    # Returns the arrival time of last pkt in pkts list when pkts are going
    # across the network through links with speeds in linkSpeeds list
    def pktsDeliveryTime(linkSpeeds, pkts):
        K = len(linkSpeeds) # index of hops starts at zero for sender
                            # host and K for receiver host
        N = len(pkts) # number of packets

        # arrivals[n][k] gives arrival time of pkts[n-1] at hop k.
        arrivals = [[0]*(K+1) for i in range(N+1)]

        # arrivals[0][:] = 0 (ie. arrival of an imaginary first packet of
        # mesg or last pkt of previous txPkts train) and
        # arrivals[:][0] = txStart (ie. all packets at first hop that is the
        # sender). These are considered for correct boundary conditions of
        # the formula
        if prevPktArrivals:
            arrivals[0][:] = prevPktArrivals

        for i in range(N+1):
            arrivals[i][0] = txStart

        for n in range(1,N+1):
            for k in range(1,K+1):
                arrivals[n][k] = pkts[n-1] * 8.0 * 1e-9 / linkSpeeds[k-1] +\
                    max(arrivals[n-1][k], arrivals[n][k-1])

        return arrivals[-1][:]

    if recvrIntIp == sendrIntIp:
        # When sender and receiver are the same machine
        return txStart

    edgeSwitchFixDelay = switchFixDelay
    fabricSwitchFixDelay = switchFixDelay
    linkDelays = []
    switchFixDelays = []
    linkSpeeds = []
    totalDelay = 0

    if ((sendrIntIp >> 8) & 255) == ((recvrIntIp >> 8) & 255):
        # receiver and sender are in same rack

        switchFixDelays += [edgeSwitchFixDelay]
        linkDelays += [edgeLinkDelay] * 2

        if not(isFabricCutThrough):
            linkSpeeds = [nicLinkSpeed] * 2
        else:
            # In order to abuse the pktsDeliveryTime function for finding
            # network serializaiton delay when switches are cut through, we need
            # to define linkSpeeds like below
            linkSpeeds = [nicLinkSpeed]

    elif ((sendrIntIp >> 16) & 255) == ((recvrIntIp >> 16) & 255):
        # receiver and sender are in same pod

        switchFixDelays +=\
            [edgeSwitchFixDelay, fabricSwitchFixDelay, edgeSwitchFixDelay]
        linkDelays +=\
            [edgeLinkDelay, fabricLinkDelay,\
            fabricLinkDelay, edgeLinkDelay]

        if not(isFabricCutThrough):
            linkSpeeds =\
                [nicLinkSpeed, fabricLinkSpeed,\
                fabricLinkSpeed, nicLinkSpeed]
        else:
            linkSpeeds = [nicLinkSpeed, fabricLinkSpeed]

    elif ((sendrIntIp >> 24) & 255) == ((recvrIntIp >> 24) & 255):
        # receiver and sender in two different pod

        switchFixDelays +=\
            [edgeSwitchFixDelay, fabricSwitchFixDelay, fabricSwitchFixDelay,\
            fabricSwitchFixDelay, edgeSwitchFixDelay]
        linkDelays +=\
            [edgeLinkDelay, fabricLinkDelay,\
            fabricLinkDelay, fabricLinkDelay,\
            fabricLinkDelay, edgeLinkDelay]

        if not(isFabricCutThrough):
            linkSpeeds =\
                [nicLinkSpeed, fabricLinkSpeed,\
                fabricLinkSpeed, fabricLinkSpeed,\
                fabricLinkSpeed, nicLinkSpeed]
        else:
            linkSpeeds = [nicLinkSpeed, fabricLinkSpeed]
    else:
        raise Exception, 'Sender and receiver IPs dont abide the rules in'\
            ' config.xml file.'

    # Add network serialization delays at switches and sender nic
    pktArrivalsAtHops = pktsDeliveryTime(linkSpeeds, txPkts)
    totalDelay += pktArrivalsAtHops[-1]

    # Add fixed delays:
    totalDelay +=\
        hostSwTurnAroundTime * 2 + hostNicSxThinkTime +\
        sum(linkDelays) + sum(switchFixDelays)

    return totalDelay, pktArrivalsAtHops

def getMinMct_simplified(txStart, firstPkt, totalBytes, sendrIntIp, recvrIntIp):
    """
    For a train of txPkts as with total bytes on wire equal to totalBytes and
    first pakcet size of the train equal to firstPkt, this method returns the
    arrival time of last bit of this train at the receiver. This method assumes
    all packets except the last one are full-packet size and the core link
    speeds are greater than edge link speeds.
    """
    if recvrIntIp == sendrIntIp:
        # When sender and receiver are the same machine
        return txStart

    edgeSwitchFixDelay = switchFixDelay
    fabricSwitchFixDelay = switchFixDelay
    linkDelays = []
    switchFixDelays = []
    linkSpeeds = []
    totalDelay = totalBytes * 8.0 * 1e-9 / nicLinkSpeed

    if ((sendrIntIp >> 8) & 255) == ((recvrIntIp >> 8) & 255):
        # receiver and sender are in same rack

        switchFixDelays += [edgeSwitchFixDelay]
        linkDelays += [edgeLinkDelay] * 2

        if isFabricCutThrough:
            pass
        else:
            linkSpeeds = [nicLinkSpeed]

        totalDelay +=\
            sum(firstPkt* 8.0 * 1e-9 / linkSpeed for linkSpeed in linkSpeeds)

    elif ((sendrIntIp >> 16) & 255) == ((recvrIntIp >> 16) & 255):
        # receiver and sender are in same pod

        switchFixDelays += [edgeSwitchFixDelay, fabricSwitchFixDelay,
                            edgeSwitchFixDelay]
        linkDelays += [edgeLinkDelay, fabricLinkDelay, fabricLinkDelay,
                       edgeLinkDelay]

        if isFabricCutThrough:
            linkSpeeds = [nicLinkSpeed]
        else:
            linkSpeeds = [nicLinkSpeed, fabricLinkSpeed, fabricLinkSpeed,
                          nicLinkSpeed]
        totalDelay +=\
            sum(firstPkt* 8.0 * 1e-9 / linkSpeed for linkSpeed in linkSpeeds)

    elif ((sendrIntIp >> 24) & 255) == ((recvrIntIp >> 24) & 255):
        # receiver and sender in two different pod

        switchFixDelays += [edgeSwitchFixDelay, fabricSwitchFixDelay,
                            fabricSwitchFixDelay, fabricSwitchFixDelay,
                            edgeSwitchFixDelay]
        linkDelays += [edgeLinkDelay, fabricLinkDelay, fabricLinkDelay,
                       fabricLinkDelay, fabricLinkDelay, edgeLinkDelay]

        if isFabricCutThrough:
            linkSpeeds = [nicLinkSpeed]
        else:
            linkSpeeds = [nicLinkSpeed, fabricLinkSpeed, fabricLinkSpeed,
                          fabricLinkSpeed, fabricLinkSpeed, nicLinkSpeed]
        totalDelay +=\
            sum(firstPkt[0]* 8.0 * 1e-9 / linkSpeed for linkSpeed in linkSpeeds)


    else:
        raise Exception, 'Sender and receiver IPs dont abide the rules \
            in config.xml file.'

    # Add fixed delays:
    totalDelay +=\
        hostSwTurnAroundTime * 2 + hostNicSxThinkTime +\
        sum(linkDelays) + sum(switchFixDelays)

    return totalDelay

def workerProcessResultFile(flowFile, outputDir, semaphore):
    """
    process result file from pfabric simulation, compute stretch for different
    ranges of message sizes, and record statistics measures of stretch in output
    files.
    """
    semaphore.acquire()
    #t0 = time.time()
    match = re.match('.*empirical_(\S+)_pfabric.*load(0?\.\d+).*', flowFile)
    wltype = match.group(1)
    loadFactor = float(match.group(2))
    workload = Workloads[wltype]
    ranges = rangesDic[workload]
    fd = open(flowFile)
    stretchs = dict()
    N = 0.0
    B = 0.0
    sumBytes = dict()
    sumStretch = 0.0
    for line in fd :
        words = line.split()
        try:
            outputs = [float(word) for word in words[0:5]]
            mesgBytes = int(outputs[0]) # message size in bytes
            mct = outputs[1] # message completion time
            rttimes = int(outputs[2]) # retransmission times
            srcId = int(outputs[3]) # id of source node
            destId = int(outputs[4]) # id of destination node

        except:
            print(line)
            continue
        srcIp = serverIdToIp(srcId)
        destIp = serverIdToIp(destId)

        mesgPkts = [1500 for i in range(mesgBytes/1460)] +\
                ([(mesgBytes%1460) + 40] if (mesgBytes%1460) else [])
        minMct, pktArrivalsAtHops = getMinMct_generalized(
            0, mesgPkts, srcIp, destIp)
        #minMct = getMinMct_simplified(
        #    0, mesgPkts[0], sum(mesgPkts), srcIp, destIp)
        stretch = mct/minMct
        sumStretch += stretch
        msgrangeInd = bisect.bisect_left(ranges, mesgBytes)
        msgrange = 'inf' if msgrangeInd==len(ranges) else ranges[msgrangeInd]

        N += 1
        B += sum(mesgPkts)
        if msgrange not in stretchs:
            stretchs[msgrange] = []
            sumBytes[msgrange] = 0.0
        #bisect.insort(stretchs[msgrange], stretch)
        stretchs[msgrange].append(stretch)
        sumBytes[msgrange] += sum(mesgPkts)
        #print(str(N) + "\n")
    fd.close()

    # find the statistical metrics for stretches of different message ranges
    recordLines = []
    for msgrange,stretchList in sorted(stretchs.iteritems()):
        stretch = sorted(stretchList)
        n = len(stretch)
        medianInd = int(floor(n*0.5))
        ninety9Ind = int(floor(n*0.99))
        sizePerc = n / N * 100
        bytesPerc = sumBytes[msgrange] / B * 100
        recordLine = '{0}\t\t{1}\t\t{2}\t\t{3}\t\t{4}\t\t'\
            '{5}\t\t{6}\t\t{7}\t\t{8}\t\t{9}\n'.format('pfabric', loadFactor,
            workload, msgrange, sizePerc, bytesPerc, unschedBytes,
            mean(stretch), 'NA', 'NA')
        recordLines.append(recordLine)

        recordLine = '{0}\t\t{1}\t\t{2}\t\t{3}\t\t{4}\t\t'\
            '{5}\t\t{6}\t\t{7}\t\t{8}\t\t{9}\n'.format('pfabric', loadFactor,
            workload, msgrange, sizePerc, bytesPerc, unschedBytes, 'NA',
            stretch[medianInd], 'NA')
        recordLines.append(recordLine)

        recordLine = '{0}\t\t{1}\t\t{2}\t\t{3}\t\t{4}\t\t'\
            '{5}\t\t{6}\t\t{7}\t\t{8}\t\t{9}\n'.format( 'pfabric', loadFactor,
            workload, msgrange, sizePerc, bytesPerc, unschedBytes, 'NA', 'NA',
            stretch[ninety9Ind])
        recordLines.append(recordLine)
    #t1 = time.time()
    #print "total processing time: {0}, for flow file:\n\t{1}".format(
    #t1-t0, flowFile)

    # use number of files in the directory as suffix to the result file name
    # and write the stretch metrics in the file
    numFiles = subprocess.check_output('ls %s -1| wc -l'%outputDir, shell=True)
    numFiles = int(numFiles.strip())
    resultFile = os.path.join(outputDir,\
        'StretchVsTransport%d-%s-%.2f.txt'%(numFiles, wltype, loadFactor))
    resultFd = open(resultFile, 'w')
    [resultFd.write(recordLine) for recordLine in recordLines]
    resultFd.write("## Average Stretch: {0}, Average Size: {1}".format(
        sumStretch/N, sum(
        [sumbytes for msgrange, sumbytes in sumBytes.iteritems()])/N))
    resultFd.flush()
    resultFd.close()

    semaphore.release()

def workerMain():
    """
    For the host this script is running on, this function reads tasks info
    specified in workerInfo file and then creates threads to run the tasks.
    """

    fd = open(os.path.join(targetDir, 'workerInfo'))
    hostName = socket.gethostname()
    numcores = ''
    workerName = ''
    while True:
        line = fd.readline().rstrip('\n')
        if line == "":
            break
        if line[0] == '#':
            line = fd.readline().split()
            assert(line[0] == 'hostname')
            workerName = line[1]
            if workerName == hostName:
                line = fd.readline().split()
                assert(line[0] == 'numcores')
                numcores = line[1]
                break
    if not(workerName) or not(numcores):
        return
        fd.close()
    flowFiles = []
    while True:
        line = textwrap.dedent(fd.readline()).rstrip('\n')
        if line == '' or line == '#':
            break
        flowFiles.append(line)

    fd.close()
    localtime = time.localtime()
    outputDir = os.path.join(targetDir, '%.4d%.2d%.2d_%.2d%.2d' % (
        localtime.tm_year, localtime.tm_mon, localtime.tm_mday, 
        localtime.tm_hour,localtime.tm_min))
    devNull = open(os.devnull,"w")
    subprocess.check_call('mkdir -p %s' % (outputDir),
        shell=True, stdout=devNull)
    devNull.close()

    semaphore = multiprocessing.Semaphore(
        min(multiprocessing.cpu_count, int(numcores)))
    processes = [multiprocessing.Process(target=workerProcessResultFile, args=(
        flowFileName, outputDir, semaphore)) for flowFileName in flowFiles] 

    [p.start() for p in processes]
    [p.join() for t in processes]

if __name__ == '__main__':
    parser = OptionParser(description='This script distributes processing of\
        ns2\'s flowfiles among multiple worker servers. Each worker server\
        processes the files assigned to it and records the results in output\
        files. The script blocks until all servers finish their job, at which\
        point this script merges all the result files into one single output\
        file and return.')

    parser.add_option('-r', '--resultListFile', metavar='FILENAME',
        help='the file containing names of ns2 flow.tr result files.',
        dest='resultListFile', default='')

    parser.add_option('-s', '--workerSpecsFile', metavar='FILENAME',
        help='the file containing the information about worker that are to\
        process the ns2 flow.tr result files.',
        dest='workerSpecsFile', default='')
    parser.add_option('-w', '--workerMode', action='store_true',
        dest='workerMode', default=False,
        help='run the script for worker mode.')

    options, args = parser.parse_args()
    resultListFile = options.resultListFile
    workerSpecsFile = options.workerSpecsFile
    if options.workerMode:
        workerMain()
        exit('worker done') 

    if not resultListFile or not workerSpecsFile:
        exit('options --resultListFile and --workerSpecsFile are required.')

    # read and store all flowfiles
    flowFd = open(resultListFile)
    flowFiles = []
    for flowFile in flowFd:
        flowFile = textwrap.dedent(flowFile).rstrip('\n')
        if flowFile == '' or flowFile[0] == '#':
            continue
        # check if we can open flowfile and it's good
        fd = open(flowFile)
        fd.close()
        flowFiles.append(flowFile)
    flowFd.close()

    if flowFiles == []:
        exit("no flow files specified in resultListFile")

    # read worker information from file
    workerFd = open(workerSpecsFile)
    workerList = []
    for line in workerFd:
        line = textwrap.dedent(line).rstrip('\n')
        if line == '' or line[0] == '#':
            continue
        workerInfo = line.split()
        workerName = workerInfo[0]
        cores = int(workerInfo[1])
        workerList.append([workerName, cores])

    if workerList == {}:
        exit("no worker specified in the workerListFile")

    # Distribute the jobs among the workers and
    # create a file for workers to read their assigned jobs
    fd = open(os.path.join(targetDir,'workerInfo'), 'w')
    random.shuffle(flowFiles)
    i = 0
    while flowFiles:
        flowFile = flowFiles.pop()
        workerList[i%len(workerList)].append(flowFile)
        i += 1

    for wrkrInfo in workerList:
        workerInfo = list(wrkrInfo)
        fd.write('#\n')
        fd.write('hostname  {}\n'.format(workerInfo.pop(0)))
        fd.write('numcores  {}\n'.format(workerInfo.pop(0)))
        while workerInfo:
            fd.write(workerInfo.pop(0)+'\n')
    fd.flush()
    fd.close()
    time.sleep(5)

    # ssh into workers and run the script in the worker mode to
    # execute their jobs
    for i in range(len(workerList)):
        hostName = workerList[i][0]
        try:
            devNull = open(os.devnull,"w")
            scriptAbsName = os.path.abspath(__file__)
            scriptPath = os.path.dirname(scriptAbsName)
            scriptName = os.path.basename(scriptAbsName)
            cmdToRun = './%s --workerMode ' % (scriptName)
            sshCmd = ("""ssh -n -f %s "sh -c 'cd %s; nohup %s > /dev/null"""
                """ 2>&1 &'" """ % (hostName, scriptPath, cmdToRun))
            print(sshCmd)
            subprocess.check_call(sshCmd, shell=True, stdout=devNull)
            devNull.close()
            time.sleep(5)
        except Exception as e:
            devNull.close()
            print('Can not ssh to worker node: {}'.format(hostName), file=sys.stderr)
            exit()
