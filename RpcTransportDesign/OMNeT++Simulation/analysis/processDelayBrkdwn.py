#!/usr/bin/python
import sys
import os
import re
from pprint import pprint
from numpy import *
from optparse import OptionParser

def main():
    parser = OptionParser()
    options, args = parser.parse_args()
    if len(args) > 0:
        resultFile = args[0]
    else:
        resultFile = 'homatransport/src/dcntopo/results/' +\
            'tailBreakdown/tailDelayBrkdwn.txt'

    f = open(resultFile)
    inGroup = False
    allDelays = [[0.0]*5]*5
    i = 0
    iters = 0
    sumDelays = 0.0
    for line in f:
        match = re.match("^=", line) 
        if match:
            inGroup = ~inGroup
            i = 0
            if inGroup:
                #print(sumDelays)
                sumDelays = 0.0
                iters += 1
        elif inGroup:
            try:
                tmp = [float(elem) for elem in line.rstrip().rstrip(',').split(',')]
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

    printLine(['', 'QueueDelay(%)', 'PrmtLag_LargMesg(%)', 'PrmtLab_ShortMesg(%)',
        'QueuedBytes', 'QueuedPkts'])
    rows = ['SwDelay', 'NIC', 'TorUp', 'CoreDown', 'TorDown']
    for i in range(len(allDelays)): 
        printLine([rows[i]] + allDelays[i])

    print('='*100)

    sumCol = [0.0]*3
    for i in range(len(allDelays)):
        for j in range(len(allDelays[i])):
            if j < 3:
                sumCol[j] += allDelays[i][j]
    printLine(['Sum Delays'] + [elem for elem in sumCol])

    print("\n\nTotal delays(us): " + str(sumDelays))
    print("Total Samples: " + str(iters))


if __name__ == '__main__':
    sys.exit(main());
