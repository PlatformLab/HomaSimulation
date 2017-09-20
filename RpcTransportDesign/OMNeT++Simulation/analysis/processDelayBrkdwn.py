#!/usr/bin/python
import sys
import os
import re
from pprint import pprint
from numpy import *
from optparse import OptionParser

def main(resultFile, workload):
    f = open(resultFile)
    inGroup = False
    allDelays = [[0.0]*5]*5
    i = 0
    iters = 0
    sumDelays = 0.0
    baseLatency = 0.0
    for line in f:
        match = re.match("^=", line)
        if match:
            inGroup = ~inGroup
            i = 0
            if inGroup:
                #print(sumDelays)
                sumDelays = 0.0
                mesgLatency = 0.0
                iters += 1
            else:
                baseLatency += (mesgLatency - sumDelays)
        elif inGroup:
            try:
                match = re.match(
                    'delay 99%ile : ([\d.e-]+), mesg size: ([\d]+)', line)
                if match:
                    mesgLatency = float(match.group(1))
                    mesgSize = float(match.group(2))
                    continue
                tmp = [float(elem) for elem in \
                    line.rstrip().rstrip(',').split(',')]
            except:
                continue
            sumDelays += sum(tmp[0:3])
            allDelays[i] = [x+y for x,y in zip(tmp, allDelays[i])]
            i += 1
        else:
            continue

    sumDelays = 0.0
    #pprint(allDelays)
    for i in range(len(allDelays)):
        for j in range(len(allDelays[i])):
            allDelays[i][j] = allDelays[i][j] / iters
        sumDelays += sum(allDelays[i][0:3])

    #pprint(allDelays)
    sumDelays *= 1e6 # in microseconds
    def printLine(line):
        tw = 20
        l = ''
        for i in range(len(line)):
            elem = line[i]
            if type(elem) is float:
                if i < 4:
                    elem = elem*1e6 # in micro seconds
                    l += ('{:.3f}'.format(elem)+ '(' +
                        '{:.2f}'.format(elem/sumDelays*100) + '%)').center(tw)
                else:
                    l += '{:.4e}'.format(elem).center(tw)

            else:
                l += str(elem).center(tw)

        print l

    printLine(['Workload', 'Location', 'QueueDelay(%)', 'PrmtLag_LargMesg(%)',
        'PrmtLab_ShortMesg(%)', 'QueuedBytes', 'QueuedPkts'])
    locations = ['SwDelay', 'NIC', 'TorUp', 'CoreDown', 'TorDown']
    for i in range(len(allDelays)):
        printLine([workloadType] + [locations[i]] + allDelays[i])

    print('='*100)
    sumCol = [0.0]*3
    for i in range(len(allDelays)):
        for j in range(len(allDelays[i])):
            if j < 3:
                sumCol[j] += allDelays[i][j]
    printLine([' ']+['Sum Delays'] + [elem for elem in sumCol])

    print("\n\n")
    tl=30
    print("All delay overhead(us): {0:.3f}".format(sumDelays).ljust(tl) +
        "Base latency(us): {0:.3f}".format(baseLatency/iters*1e6).center(tl) +
        "Total Samples: {0}".format(str(iters)).center(tl))


if __name__ == '__main__':
    parser = OptionParser('./processDelayBrkdwn RESULT_FILE_NAME WORKLOAD_TYPE')
    options, args = parser.parse_args()
    if len(args) != 2:
        parser.error("incorrect number of arguments")
    resultFile = args[0]
    workloadType = args[1]

    sys.exit(main(resultFile, workloadType));
