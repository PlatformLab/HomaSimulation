#!/usr/bin/python

# Copyright (c) 2015 Stanford University
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

"""
Runs a simplified simulations for the credit based transport scheme.
"""
import os
import sys
import random
import numpy as np
from operator import sub
from optparse import OptionParser

"""
"""

## @var avgDelay: The mean value for the delay distribution function. This value
# is an integer.
avgDelay = 5 

## @var fixedDelay: The constant delay every packet will experience in the
#  network. This parameter is an integer.
fixedDelay = 0 

def generateMsgSize(distMatrix):
    """A helper function that generates a message size from the cumulative
    probability distribution that is given as the input argument to this
    function.

    @param  distMatrix: @see run()
    @type   distMatrix: @see run()

    @return: A message size value generated from the input probability
             distribution.
    @rtype:  An integer.
    """
    randomNumber = random.uniform(0,1)
    for row in distMatrix:
        if (row[0] > randomNumber):
            return row[1]

class RxQueue():
    """ Reperesent a queue of pkts that are scheduled and received in the output
    (edge) queue of the network.  This class provides interfaces to add a packet
    to the queue and remove the packet from the queue and place it out.
    Within each priority list in the queue datastruct of this class, the packets
    are served in FIFO order but between priorities, higher priority packets
    (eg. lowest priority value in the packet) are always served first. 
    """

    def __init__(self):
        """ 
        @param  prioQueue: This is a dictionary from key:priority to value:list
                           of packets in that priority as [pkt1, pkt2, ...].

        @param  size: The total number of packets in this queue at the moment.
        """

        self.prioQueue = dict() 
        self.size = 0 

    def enqueue(self, pkt):
        """Takes a packet as the input argument and place a copy of the packet
        in the priority queue corresponding to the input queue.

        @param  self: pointer object to this class.
        @type   self: self type. 

        @param  pkt: packet that is to be queued in this priority queue class.
                     A list of [message_id, message_size, priority]. Represents
                     a packet from a message along with the granted priority for
                     hat packet and the remaining size of message that this
                     packet blongs to.
        @type   pkt: list[int, int, int]
        """
        self.size += 1
        prioKey = pkt[2]
        if (prioKey in self.prioQueue):
            self.prioQueue[prioKey].append(pkt[:])
        else:
            self.prioQueue[prioKey] = list()
            self.prioQueue[prioKey].append(pkt[:])
        
    def dequeue(self, prio = -1):
        """Removes a packet from the highest priority queue in this class and
        returns that packet. If the user provides a priority input argument,
        this method will return a packet from that specific priority.

        @param  self: pointer object to this class.
        @type   self: self type. 

        @param  prio: The priority from which we want to dequeue a packet.
        @type   prio: a positive integer

        @return : a packet from the highest priority queue if no input arg. prio
                  is provided. If prio is provided, returns a packet from that
                  priority queue.
        @rtype  : either a @p pkt type or an empty list if no packet is
                  currently queue in this class.
        """
        if (self.size > 0):
            if (prio == -1):
                it = iter(sorted(self.prioQueue.iteritems())) 
                for prio, pktList in it:
                    if (len(pktList)):
                        pkt = self.prioQueue[prio].pop(0)
                        self.size -= 1
                        return pkt 
            elif (prio in self.prioQueue and len(self.prioQueue[prio])):
                self.size -= 1
                pkt = self.prioQueue[prio].pop(0)
                return pkt
        return []

    def updatePktsPrio(self, oldPrio, newPrio, msgId):
        """For all packets belonging to message with msgId with priority
        oldPrio, this function updates the priority to newPrio.
        """
        if (oldPrio in self.prioQueue):
            pktList = self.prioQueue[oldPrio]
            for pkt in pktList[:]:
                if (pkt[0] == msgId):
                    newPkt = [pkt[0], pkt[1], newPrio] 
                    pktList.remove(pkt[:])
                    if (newPrio in self.prioQueue):
                        self.prioQueue[newPrio].append(newPkt[:])
                    else:
                        self.prioQueue[newPrio] = list()
                        self.prioQueue[newPrio].append(newPkt[:])

    def getPrioQueueSize(self, prio):
        """Provides the size of priority queues in this class.

        @param  self: pointer object to this class.
        @type   self: self type. 
        
        @param  prio: priority at which the caller wants to know the number of
                      packets queued in the corresponding queue. 
        @type   prio: A non-negative integer. 
        
        @return: The size of priority queue corresponind to priority @p prio  
        @rtype:  A non-negative integer.
        """
        if prio in self.prioQueue:
            return len(self.prioQueue[prio])
        else:
            return 0

    def getTotalSize(self):
        """Return the the current total size of this queue. (sum of the number
        of packets in all priority queues in this class.)

        @param  self: pointer object to this class.
        @type   self: self type. 
        
        @return: total number of packets in this queue.
        @rtype: An integer
        """
        
        return self.size 

class Scheduler():
    """This class creates the common internal context for an arbitrary scheduler
    and implements the interface to various schedulers. If a new scheduler is to
    be added, this class is the right place to implement that scheduler as a
    function of it.

    ===Definition of internal variables===
    @var    msg: A message that will be generated at sender and must be
                 transmitted. A list of [message_id, message_size]. Both values
                 in this list are integers.

    @var    delayedPkt: Represent a packet that is taken out of txQueue and
                        should be received in rxQueue after some delay value. A
                        list of [pkt[:], delay]. For type of pkt, please see
                        @see Scheduler::addPktToDelayQueue
                        delay is an integer.
    """

    def __init__(self, txQueue = [], rxQueue = RxQueue(), delayQueue = []):
        """
        @param  txQueue: The transmit queue at the input of simulation. A list
                         of messages as [msg1, msg2, ..] that are sorted by the
                         message size from smallest to largest. Every time a new
                         messge is generated for this simulation, it will be
                         added to this queue. Messgaes are of type @see msg.
        @type   txQueue: C[msg, msg, msg, ...]

        @param  rxQueue: Reperesent a queue of pkt that are scheduled and
                         received in the output (edge) queue of the network.
                         This practically is a dict from key:priority of packets
                         to a list of packets in that priority as [pkt1, pkt2,
                         ...]. smallest priority value has the highest priority.
        @type   rxQueue: Dict{key(pkt_prio):[pkt1, pkt2, ...]}

        @param  delayQueue: This queue contains all packets that are scheduled
                            and has left txQueue but has not yet received in
                            rxQueue.  This queue essentially models the network
                            so packets in this queue are in flight and
                            experiencing some delay in the network. This is a
                            list of [delayedPkt1, delayedPkt2, ...] and this
                            list is sorted by the delay of the packets from
                            lowest to highest. delayedPkts are of type @see
                            delayedPkt .
        @type   delayQueue: C[delayedPkt, delayedPkt, ...]
        """
        self.txQueue = txQueue
        self.rxQueue = rxQueue
        self.delayQueue= delayQueue 

    def addPktToDelayQueue(self, pkt, avgDelay):
        """This function models the network in a very simple way by only taking
        the network delay into account. For every packet that is to transmitted
        over, this model assumes that the network has full bisection bandwidth
        and it only add some random delay to the transmision time of the packet.
        Every time a packet enters this model, it will be added to a the \p
        delayQueue and will leave the this queue after some random delay. The
        delay model is assumed to be some fixed delay \p avgDelay plus some
        random delay from a Poisson distribution with mean avgDelay.

        @param  pkt: A list of [message_id, message_size, priority]. Represents
                     a packet from a message along with the granted priority for
                     that packet and the remaining size of message that this
                     packet blongs to. The remaining size of the message is
                     calculated including this pkt.
        @type   pkt: list[int, int, int]

        @param  avgDelay: See @see avgDelay
        """
        msgId = pkt[0]
        msgSize = pkt[1]
        pktPrio = pkt[2]

        # Sample delay from a Poisson distribution. Then create a \p delayedPkt
        # and and add it to the \p dealyQueue.
        delay = np.random.poisson(avgDelay) + fixedDelay
        delayedPkt = list([msgId, msgSize, pktPrio, delay])
        self.delayQueue.append(delayedPkt)
        self.delayQueue.sort(cmp=lambda x, y: cmp(x[3], y[3]))

    def depleteDelayQueue(self, updatePrio = False, activeMsgsPrio = {}):
        """This function practically abstracts out the propagation of the
        packets in the network until they are received at @p rxQueue. Everytime
        a packet is completely transmitted through the network, it will add that
        packet to the @p rxQueue and makes sure the rxQueue is kept sorted with
        respect to packet priorities.

        @return pktsArrivedRxQueue: A list of packets that have completely
                                    traversed the network and removed from
                                    the @p delayQueue and added to rxQueue.
        @rtype  pktsArrivedRxQueue: list[pkt1,pkt2, pkt3, ...]
        """
        
        pktsArrivedRxQueue = list()
        for delayedPkt in self.delayQueue[:]:
            if (delayedPkt[-1] == 0):#deplete pkts
                pkt = delayedPkt[0:-1] 
                if (updatePrio) :#update priorities if requiered
                    msgId = pkt[0]
                    pkt[2] = activeMsgsPrio[msgId]

                self.rxQueue.enqueue(pkt)
                pktsArrivedRxQueue.append(pkt[:])
                self.delayQueue.remove(delayedPkt)

        for delayedPkt in self.delayQueue:#update delays
            delayedPkt[-1] -= 1
            if (updatePrio) :#update priorities if requiered
                msgId = delayedPkt[0]
                delayedPkt[2] = activeMsgsPrio[msgId]

        return pktsArrivedRxQueue

    def simpleScheduler(self, slot, pktOutTimes):
        """Impements the most simple SRPT scheduler scheme. At any simulation
        round, if there are messages pending for transmission, this module tries
        to minimize the latency for the shortest messages (SRPT - shortest
        remaining time first - scheduling) so it takes a packet from the message
        with the smallest size and propagates it throught the network by appying
        the simple network model we have implemented in this class.
        Subsequently at every round, between all the packets that has completely
        traveresed the network, this module takes a packet in FIFO mode and puts
        it out of the simulation.

        ===Additional info===
        This scheduler can only schedule one packet in every scheduling round
        (one round = one packet time). If delay was deterministic, this one
        packet per round would be enough to achieve our goal that is minimizing
        the latency of small messages. But stochastic delay could cause bubbles
        in rxQueue output port and prevent us from achieving our goal: For
        example let's consider this scenario: msg1 and msg2 both have size 1. In
        the first round the scheduler chooses packet from msg1 and in the round
        2 msg2 will be scheduled. But assume that packet from msg1 will
        exprience a large delay  and packet from msg2 that packet will
        experience the minimum delay. So if the scheduler knew in advance that
        the second pkt would experience less delay, it would have scheduled it
        in the first round and that message would have completed earlier.

        @param  slot: The simulation round for which this scheduler is run.
        @type   slot: An integer.

        @retun  pktOutTimes: @see run() 
        @rtype  pktOutTimes: @see run()
        """
                        
        # Remove a packet from head of the line in the rxQueue and record its
        # packet info in the @p pktOutTimes. Subsequently for the packets that
        # have completely taransmitted through the network, put them in the
        # rxQueue and again update their packet info in @p pktOutTimes.
        pkt = self.rxQueue.dequeue()
        if pkt:
            msgId = pkt[0]
            msgSize = pkt[1]
            pktPrio = pkt[2]
            for pktInfo in pktOutTimes[msgId]:
                if (pktInfo[0] == msgSize):
                    pktInfo.append(slot)
                    break

        pktsArrivedRxQueue = self.depleteDelayQueue()
        for pkt in pktsArrivedRxQueue:
            msgId = pkt[0]
            msgSize = pkt[1]
            pktPrio = pkt[2]
            for pktInfo in pktOutTimes[msgId]:
                if (pktInfo[0] == msgSize):
                    pktInfo.append(slot)
                    break
        
        prio = 0 # The priority of all packets are the same
        if (len(self.txQueue) > 0):

            # Take the highest priority packet from the txQueueu and propaget it
            # through the network by adding it to the delayQueue. Also for every
            # scheduled packet update packet info in @p pktOutTimes. 
            highPrioMsg = self.txQueue[0]
            msgId = highPrioMsg[0]
            msgSize = highPrioMsg[1]
            highPrioPkt = list([msgId, msgSize, prio])
            self.addPktToDelayQueue(highPrioPkt, avgDelay)    

            if msgId in pktOutTimes:
                pktOutTimes[msgId].append([msgSize, prio, slot])
            else:
                pktOutTimes[msgId] = list()
                pktOutTimes[msgId].append([msgSize, prio, slot])

            highPrioMsg[1] -= 1
            if (highPrioMsg[1] == 0):
                self.txQueue.pop(0)

    def idealScheduler(self, slot, pktOutTimes, msgDict, activeMsgsPrio):
        """Implements the ideal scheduler (Oracle scheduler).
        Because the network delay is a random variable, the simple scheduler
        might not lead to the best latency. For example, a packet from a smaller
        message might experience higher latency than a packet from a larger
        message that is scheduled in the next round therefore the packet from
        the larger message will be received earlier in the rxQueue. Since the
        rxQueue is FIFO for simple scheduler the packet from the larger message
        will cause HOL for the packet from the smaller message and therefore the
        simple scheduler will not achieve its goal that is minimizing the
        latency of the smallest message. The ideal scheduler instead have many
        priorities in the rxQueue (one priority per message size) therefore no
        HOL can happen in rxQueue. 
        On the other hand, in order to compensate for the effect of random delay
        on the messages of same size or messages that are slightly different in
        size, the ideal scheduler will schedule more than one packet in each
        round. In principle if we know how much delay each packet will
        exprience, we might sometimes want to prioritize packets from slightly
        larger messages over packets from smaller message if we knew packets
        from smaller message will experience much higher delay. The ideal
        scheduler in fact overcome this problem by scheduling one packet from
        every message in the txQueue at each round and allows all messages to
        transmit one packet (rather than scheduling one packet only from the
        smallest message as simple scheduler does). Then as the packets are
        received in rxQueue, they will be prioritized based on the remaining
        message size. The scheduler then take the highest priority packet
        (packet from the message with smallest remaining size) and removes it
        from the queue.
        This scheduler completely hides the effect of random variation in delay
        and because of infinite priorities no HOL can happen in this ideal
        scheme. This scheme is the best we can do for the scheduler.
        
        @param  slot: The simulation round for which this scheduler is run.
        @type   slot: An integer.


        @retun  pktOutTimes: @see run() 
        @rtype  pktOutTimes: @see run()

        """

        pkt = self.rxQueue.dequeue()
        if pkt:
            #Update pktOutTimes
            msgId = pkt[0]
            msgSize = pkt[1]
            pktPrio = pkt[2]
            for pktInfo in pktOutTimes[msgId]:
                if (pktInfo[0] == msgSize):
                    pktInfo.append(slot)
                    break

            #Update activeMsgsPrio and update prio of the rest pkts belonging to
            #msgId in rxQueue
            if (pktPrio == 1): #this packet was the last pkt of msg
                activeMsgsPrio.pop(msgId)
            else:
                newPrio = pktPrio - 1
                activeMsgsPrio[msgId] = newPrio 
                self.rxQueue.updatePktsPrio(pktPrio, newPrio, msgId)

        #deplete pkts and update priorities
        pktsArrivedRxQueue = self.depleteDelayQueue(True, activeMsgsPrio) 
        for pkt in pktsArrivedRxQueue:
            msgId = pkt[0]
            msgSize = pkt[1]
            pktPrio = pkt[2]
            for pktInfo in pktOutTimes[msgId]:
                if (pktInfo[0] == msgSize):
                    pktInfo.append(slot)
                    break

        for msg in self.txQueue[:]:
            # Set the priority same as the size
            msgId = msg[0]
            msgSize = msg[1]
            prio = 0

            if (msgId in activeMsgsPrio):
                prio = activeMsgsPrio[msgId]
            else:
                prio = msgDict[msgId][0] # priority equal the original size of
                                         # message
                activeMsgsPrio[msgId] = prio 

            pkt = list([msgId, msgSize, prio])
            self.addPktToDelayQueue(pkt, avgDelay)

            if msgId in pktOutTimes:
                pktOutTimes[msgId].append([msgSize, prio, slot])
            else:
                pktOutTimes[msgId] = list()
                pktOutTimes[msgId].append([msgSize, prio, slot])

            if (msg[1] == 1):
                self.txQueue.remove(msg)
            else:
                msg[1] -= 1
            
        
def collectRxQueueStats(rxQueue, queueSizeDist, totalSizeDist): 
    """For the input rxQueue, this function gather the size distribution of the
    rxQueue and also size of distribution of priority queues within that queue.

    @param  prioList: A list containing the priorities values this scheduler
                      will ever assign to the packets. This is essentially a
                      list[smallest_msg_size, largest_msg_size] 
    @type   prioList: list[int, int, int, ...] 
    
    @return queueSizeDist: For every priority queue within the rxQueue this
                           varialbe will contain the distribution of the size of
                           that queue in the simulation. That is the
                           number(counts) of rounds of simulation that a
                           priority queue has had that size.  Must have been
                           initialized to dict{x:{} for x in prioList}. 
    @rtype  queueSizeDist: dict{ key(priority) : 
                                 value(dict{ key(size) : 
                                             value(counts) }) }

    @return totalSizeDist: This will contain the size distribution of rxQueue.
                           That is the number of (counts) of rounds that rxQueue
                           had that size. 
    @rtype  totalSizeDist: dict{key(size) : value(counts) }

    """

    # For every priority queue in rxQueue find the queue size and update the
    # distribution for that priority queue
    for prio, prioQueueDist in queueSizeDist.iteritems():
        prioQueueSize = rxQueue.getPrioQueueSize(prio)
        if prioQueueSize in prioQueueDist:
            prioQueueDist[prioQueueSize] += 1
        else:
            prioQueueDist[prioQueueSize] = 1
    
    # Update the distribution for total queue size
    rxQueueSize = rxQueue.getTotalSize() 
    if rxQueueSize in totalSizeDist:
        totalSizeDist[rxQueueSize] += 1
    else:
        totalSizeDist[rxQueueSize] = 1

def run(steps, rho, distMatrix, msgDict, pktOutTimes,
        prioQueueSizeDist, rxQueueSizeDist):

    """Starts the simulations for different schedulers and returns packet level
    inofrmation after simulation is over. This function works like a wrapper
    around the simulation scenario so if a new scheduler is implemented in the
    Scheduler class, this function is the right place to run the experiment for
    that scheduler.
    
    @param  steps: Number of rounds for which the simulation will run.
    @type   steps: An integer.

    @param  rho: Defines the average input rate at which packets will be
                 generated in this simulation. 
    @type   rho: A float number between 0 and 1.

    @param  distMatrix: A list (matrix) that defined the cumulative probability
                        distribution of message sizes. From the lowest size to
                        the highest size, for each size there is a row in this
                        matrix as row=[cumulative_probability, size]. So this
                        matrix is formed as a list of [row1, row2, ...]
    @type   distMatrix: list[[float, int], [float, int], ...]


    @return msgDict: A dict of all messages that will be generated in this
                     simulation. Each message in this dictionary will be
                     identified by a unique that is the message_id mapped to a
                     value that is a list of  message size and the time slots 
                     at which that message is genereated.
                     So the msgDict is a dict{key(messageId):value([messageSize,
                     time_slot)]
    @rtype  msgDict: dict{key(int) : value(list[int, int])}

    @return pktOutTimes: Record the out timeslot for every individual packet
                         within for each message for each scheduler type. This
                         is a dictionary from key:scheduler_type to a
                         value:dict(). Each value itself is a dictionary from
                         key:message_id to a value:list[pkt_info]. Each pkt_info
                         in this list is some information for one packet that
                         belongs to that message identified by message_id. Each
                         pkt_info is a list of five elements as
                         pkt_info[msgSize, priority, scheduling_slot,
                         rxQueue_arrival_slot, destination_arrival_slot] that is
                         the remaining size of the message when that packet was
                         scheduled, the priority that was assigned to that
                         packet by the scheduler, the time slot at which the
                         packet has left the txQueue, the time slot at which the
                         packet left delayQueue and arrived at rxQueue, and the
                         time slot at which the packet left rxQueue (arrived at
                         destination and completed the simulation cycle). 
    @rtype  pktOutTimes: dict{ key(scheduler) :
                               value( dict{ key(messge_id) : 
                                            value([pkt_info1, pkt_info2,..]) } }

    @return prioQueueSizeDist: For every priority queue within the RxQueue in
                               the simulation, this varialbe will contain the
                               distribution of the size of that queue in the
                               simulation. That is the number(counts) of rounds
                               of simulation that a priority queue has had that
                               size. This variable will contain this information
                               for every single scheduler in this simulation.
    @rtype  prioQueueSizeDist: dict{key(scheduler_type) : 
                                    value(dict{ key(priority) : 
                                                value(dict{ key(size) : 
                                                            value(counts) }) })}

    @return rxQueueSizeDist: This will contain the size distribution of RxQueue
                             for every scheduler type this simulation runs. That
                             is the number (counts) of rounds that RxQueue had
                             that size.  
    @rtype  rxQueueSizeDist: dict{key(scheduler_type) :
                                  value(dict{key(size) : value(counts) })} 

    """
    sizeAvg = 0
    prevProb = 0

    # Find the average size of messages from the distribution list.
    for row in distMatrix: 
        sizeAvg += row[1] * (row[0] - prevProb)
        prevProb = row[0]

    probPerSlot = rho / sizeAvg # The prbability by which new messages will be
                                # added to the txQueue.
    txQueue = list()
    rxQueue = RxQueue() 
    delayQueue = list()
    scheduler = Scheduler(txQueue, rxQueue, delayQueue)
    msgId = 0
    totalPkts = 0
    # Generate a new message in each round of simulation based on the given
    # distribution of sizes and given input rate of packets. Run this simulation
    # for total of \p steps rounds and then continue runnig the simulation until
    # all packets in the system are arrived at the destination. At the end
    # return the result in \p pktOutTimes.
    # Initialize the stats collecting datastructs
    prioList = [0]
    prioQueueSizeDist['simple'] = dict((x, dict()) for x in prioList)
    queueSizeDist = prioQueueSizeDist['simple']
    totalSizeDist = rxQueueSizeDist['simple']
    for slot in range(steps):
        if (random.uniform(0, 1) < probPerSlot):
            newMsg = [msgId, generateMsgSize(distMatrix)]
            txQueue.append(newMsg[:]) 
            txQueue.sort(cmp=lambda x, y: cmp(x[1], y[1]))
            msgDict[msgId] = [newMsg[1], slot] 
            msgId += 1
            totalPkts += newMsg[1]

        scheduler.simpleScheduler(slot, pktOutTimes['simple'])

        # Collect the stats after running scheduler
        collectRxQueueStats(rxQueue, queueSizeDist, totalSizeDist)

    while (rxQueue.getTotalSize() > 0 or len(txQueue) > 0 or
            len(delayQueue) > 0):

        slot += 1
        scheduler.simpleScheduler(slot, pktOutTimes['simple'])   

        # Collect the stats after running scheduler
        collectRxQueueStats(rxQueue, queueSizeDist, totalSizeDist)

    print ("Finished running simple scheduler for rho={0} in {1}"+
            " slots").format(rho, slot+1)

    # Run ideal scheduler for the exact same message distribution as the one we
    # ran simple scheduler for in previous loop.
    txQueue = list()
    rxQueue = RxQueue() 
    delayQueue = list()
    scheduler = Scheduler(txQueue, rxQueue, delayQueue)
    msgId = 0

    # Initialize the stats collecting datastructs
    # list of all possible msg sizes
    prioList = [int(x[1]) for x in distMatrix[1:]]
    prioQueueSizeDist['ideal'] = dict((x,dict()) for x in prioList)
    queueSizeDist = prioQueueSizeDist['ideal']
    totalSizeDist = rxQueueSizeDist['ideal']

    activeMsgsPrio = dict()
    for slot in range(steps):
        if (msgId in msgDict and msgDict[msgId][1] == slot):
            newMsg = [msgId, msgDict[msgId][0]]
            txQueue.append(newMsg[:]) 
            txQueue.sort(cmp=lambda x, y: cmp(x[1], y[1]))
            msgId += 1

        scheduler.idealScheduler(slot, pktOutTimes['ideal'], msgDict,
                activeMsgsPrio)   
        
        # Collect the stats after running scheduler
        collectRxQueueStats(rxQueue, queueSizeDist, totalSizeDist)

    while (rxQueue.getTotalSize() > 0 or len(txQueue) > 0 or
            len(delayQueue) > 0):
        slot += 1
        scheduler.idealScheduler(slot, pktOutTimes['ideal'], msgDict,
                activeMsgsPrio)   

        # Collect the stats after running scheduler
        collectRxQueueStats(rxQueue, queueSizeDist, totalSizeDist)

    print ("Finished running ideal scheduler for rho={0} in {1}"+
            " slots").format(rho, slot+1)
    print (("Finished simulation for rho={0}, for total of {1} messages" +
            " comprised of total of {2} packets.").format(rho, msgId, totalPkts))

def writeToFile(fd, rowVector, width = 12):
    """prints the elements in rowVector to a line in the file with the
    descriptor fd and adjust the the space between all elements in that line to
    be equal to width. Note that fd must be already open and writable before it
    is given to this function otherwise this function will throw exception.

    @param  fd: file descriptor for the file that rowVector data will be written
                to.
    @type   fd: file decriptor (integer)

    @param  rowVector: A list containing the elements that shoudl be written
                       into a line of the the file.
    @type   rowVector: list[elem1, elem2, ..] and each elem is either an
                       integer, float or string of characters.

    @param  width: The space in terms of characters taht each element will take
                   on each line of the file.
    @type   width: an integer.
    """
    fd.write(''.join(str(element).rjust(width) for element in rowVector) + '\n')

if __name__ == '__main__':
    parser = OptionParser(description='Runs a simplified simulations for'
            ' credit based transport schemes.')
    parser.add_option('--inputDir', metavar='DIR', default='input',
            dest='inputDir',
            help='Directory containing input files for this simulation.')
    parser.add_option('--outputDir', metavar='DIR', default='output',
            dest='outputDir',
            help='Target directory that will coantain output files after'
            ' simulation.')
    parser.add_option('--slots', type=int, metavar='N', default=10000,
            dest='steps',
            help='Total number of steps that this simulation should run.')
    parser.add_option('--inputRate', type=float, metavar='RATE', default=0.5,
            dest='rho',
            help='Average Ratio input packet rate to output packet rate')
    parser.add_option('--sizeDist', metavar='FILE_NAME',
            default='SizeDistribution', dest='distFileName',
            help='Name of the input file containing the cumulative distribution'
            ' of message sizes.')
    (options, args) = parser.parse_args()

    inputDir = options.inputDir
    outputDir = options.outputDir
    steps = options.steps
    rho = options.rho
    distFileName = options.distFileName 
    distMatrix = []
    f = open(inputDir + '/' + distFileName, 'r')
    for line in f:
        if (line[0] != '#' and line[0] != '\n'):
            distMatrix.append(map(float, line.split( )))
    f.close()
    print "Comulative Message Size Distribution:\n"+str(distMatrix)
    
    f = open(outputDir + '/' + 'ideal_vs_simple_penalty', 'w')
    writeToFile(f, ["penalty", "rho", "avg_delay"], 12)

    fd = open(outputDir + '/' + 'ideal_vs_simple_rho_fixed', 'w')
    writeToFile(fd, ["penalty", "size", "rho"])

    fd1 = open(outputDir + '/' + 'completion_time_distribution', 'w')
    writeToFile(fd1, ['rho', 'scheduler', 'size', 'comp_time', 'count'], 12)

    fd2 = open(outputDir + '/' + 'prio_queue_size_dist', 'w')
    writeToFile(fd2, ['rho', 'scheduler', 'prio_queue', 'size', 'count'], 12)

    fd3 = open(outputDir + '/' + 'rx_queue_total_size_dist', 'w')
    writeToFile(fd3, ['rho', 'scheduler', 'queue_size', 'count'])

    fd4 = open(outputDir + '/' + 'output_msg_size_dist', 'w')
    writeToFile(fd4, ['prob', 'msg_size'])

    fd5 = open(outputDir + '/' + 'delays_break_down', 'w')
    writeToFile(fd5, ['rho', 'scheduler', 'size', 'total_delay',
            'serialization_delay', 'scheduling_delay', 'network_delay',
            'rxQueue_delay'], 20)

    msgDict = dict()

    # Change rho, and calculate penalty of simple scheduler vs ideal
    # scheduler.
    for n, rho in enumerate([x/100.0 for x in range(15, 105, 20)]):
        pktOutTimes = dict() 
        pktOutTimes['simple'] = dict()
        pktOutTimes['ideal'] = dict()
        rxQueueSizeDist = dict()
        rxQueueSizeDist['simple'] = dict()
        rxQueueSizeDist['ideal'] = dict()
        prioQueueSizeDist = dict()
        prioQueueSizeDist['simple'] = dict()
        prioQueueSizeDist['ideal'] = dict()

        msgDict = dict()
        run(steps, rho, distMatrix, msgDict, pktOutTimes, prioQueueSizeDist,
                rxQueueSizeDist)

        penalty = 0.0
        pktOutTimesSimple = pktOutTimes['simple']
        pktOutTimesIdeal = pktOutTimes['ideal']
        numMsg = 0

        # type of penaltyPerSize: 
        # Dict{ key(msgSize): value([totalPenalty, msgCount, rho]) }
        penaltyPerSize = dict()
        for key in pktOutTimesSimple:
            if key in pktOutTimesIdeal:

                # a list of completion times for the packets of this msg 
                pktTimesSimple = [pktInf[-1] for pktInf in
                    pktOutTimesSimple[key] if len(pktInf) == 5]
                pktTimesIdeal = [pktInf[-1] for pktInf in
                    pktOutTimesIdeal[key] if len(pktInf) == 5]
                msgSize = msgDict[key][0]
                msgBeginSlot = msgDict[key][1]

                #only calculate the penalty for this message if all packets from
                #this message has completed the simulation in both simple and
                #ideal scheduler simulations.
                if (msgSize == len(pktTimesSimple) and
                        msgSize == len(pktTimesIdeal)):
                    msgCompletionTimeSimple = (max(pktTimesSimple) -
                            msgDict[key][1])

                    msgCompletionTimeIdeal = (max(pktTimesIdeal) -
                            msgDict[key][1])

                    numMsg += 1

                    # Total penalty
                    penalty += (msgCompletionTimeSimple -
                        msgCompletionTimeIdeal) * 1.0 / msgCompletionTimeIdeal

                    # Record penalty imposed on the current msgSize
                    if (msgSize in penaltyPerSize):
                        penaltyPerSize[msgSize][0] += ((msgCompletionTimeSimple
                                - msgCompletionTimeIdeal) * 1.0 /
                                msgCompletionTimeIdeal)

                        penaltyPerSize[msgSize][1] += 1

                    else:
                        penaltyPerSize[msgSize] = [0.0, 0, rho]
                        penaltyPerSize[msgSize][0] += ((msgCompletionTimeSimple
                                - msgCompletionTimeIdeal) * 1.0 /
                                msgCompletionTimeIdeal)

                        penaltyPerSize[msgSize][1] += 1

                    # Record delay break downs
                    serialDelaySimple = msgSize
                    schedulingSlotSimple = [pktInf[2] for pktInf in
                            pktOutTimesSimple[key]]
                    scheduleDelaySimple = max(map(sub, schedulingSlotSimple, 
                            range(msgBeginSlot, msgBeginSlot +
                            int(msgSize))))

                    rxQueueArrivalSlots = [pktInf[3] for pktInf in
                            pktOutTimesSimple[key]]
                    destArrivalSlots = [pktInf[4] for pktInf in
                            pktOutTimesSimple[key]]
                    rxQueueDelaySimple = (max(destArrivalSlots) -
                            max(rxQueueArrivalSlots) - 1)
                    networkDelaySimple = (msgCompletionTimeSimple -
                            serialDelaySimple - scheduleDelaySimple -
                            rxQueueDelaySimple)
                    writeToFile(fd5, [rho, 'simple', msgSize,
                            msgCompletionTimeSimple, serialDelaySimple,
                            scheduleDelaySimple, networkDelaySimple,
                            rxQueueDelaySimple], 20)
                    
                    serialDelayIdeal = msgSize
                    schedulingSlotIdeal = [pktInf[2] for pktInf in
                            pktOutTimesIdeal[key]]
                    scheduleDelayIdeal = max(map(sub, schedulingSlotIdeal, 
                            range(msgBeginSlot, msgBeginSlot +
                            int(msgSize))))

                    rxQueueArrivalSlots = [pktInf[3] for pktInf in
                            pktOutTimesIdeal[key]]
                    destArrivalSlots = [pktInf[4] for pktInf in
                            pktOutTimesIdeal[key]]
                    rxQueueDelayIdeal = (max(destArrivalSlots) -
                            max(rxQueueArrivalSlots) - 1)
                    networkDelayIdeal = (msgCompletionTimeIdeal -
                            serialDelayIdeal - scheduleDelayIdeal -
                            rxQueueDelayIdeal)
                    writeToFile(fd5, [rho, 'ideal', msgSize,
                            msgCompletionTimeIdeal, serialDelayIdeal,
                            scheduleDelayIdeal, networkDelayIdeal,
                            rxQueueDelayIdeal], 20)


        writeToFile(f, ['{0:6f}'.format(penalty/numMsg), rho, avgDelay+fixedDelay], 12)

        for key in penaltyPerSize:
            writeToFile(fd, ['{0:6f}'.format(penaltyPerSize[key][0]/penaltyPerSize[key][1] if
                    penaltyPerSize[key][1] != 0 else 0.0), key, rho])

        # compTimeDist is a dictionary variable that for every rho value,
        # records the distribution of completion times for every message size
        # that is generated in this simulation.
        # dict{key(rho) : 
        #      value(dict{ key(msgSize) :
        #                  value(dict{ key(completion_time) : 
        #                              value(count) }) })}
        compTimeDist = dict()

        # Now find the rxQueue statistical size distribution for both ideal and
        # simple scheduler.
        for key in pktOutTimesSimple:
            pktTimesSimple = [pktInf[-1] for pktInf in
                pktOutTimesSimple[key] if len(pktInf) == 5]
            msgSize = msgDict[key][0]

            #only calculate the completion time for this message if all packets
            #from this message have been received in the destination.
            if (msgSize == len(pktTimesSimple)):
                completionTime = max(pktTimesSimple) - msgDict[key][1]
                if msgSize not in compTimeDist:
                    compTimeDist[msgSize] = {}
                    compTimeDist[msgSize][completionTime] = 1

                elif completionTime not in compTimeDist[msgSize]:
                    compTimeDist[msgSize][completionTime] = 1

                else:
                    compTimeDist[msgSize][completionTime] += 1

        
        for msgSize, val in compTimeDist.iteritems():
            for compTime, count in val.iteritems():
                writeToFile(fd1, [rho, 'simple', msgSize, compTime, count], 12)
                
        compTimeDist = dict()
        for key in pktOutTimesIdeal:
            pktTimesIdeal = [pktInf[-1] for pktInf in
                pktOutTimesIdeal[key] if len(pktInf) == 5]
            msgSize = msgDict[key][0]

            #only calculate the completion time for this message if all packets
            #from this message have been received in the destination.
            if (msgSize == len(pktTimesIdeal)):
                completionTime = max(pktTimesIdeal) - msgDict[key][1]
                if msgSize not in compTimeDist:
                    compTimeDist[msgSize] = {}
                    compTimeDist[msgSize][completionTime] = 1

                elif completionTime not in compTimeDist[msgSize]:
                    compTimeDist[msgSize][completionTime] = 1

                else:
                    compTimeDist[msgSize][completionTime] += 1
        
        for msgSize, val in compTimeDist.iteritems():
            for compTime, count in val.iteritems():
                writeToFile(fd1, [rho, 'ideal', msgSize, compTime, count], 12)
        
        # Now record queue size distributions in the file
        for schdlr, prioQuDist in prioQueueSizeDist.iteritems():
            for prioQ, sizeDist in prioQuDist.iteritems():
                for size, count in sizeDist.iteritems():
                    writeToFile(fd2, [rho, schdlr, prioQ, size, count], 12 )

        # Now record total rx queue size distribution
        for schdlr, sizeDist in rxQueueSizeDist.iteritems():
            for size, count in sizeDist.iteritems():
                writeToFile(fd3, [rho, schdlr, size, count])

    # Record distribution of generated message sizes only once for the 
    # last simulation
    msgSizeDist = {}
    totalMsgs = 0
    for msgId, msgInfo in msgDict.iteritems():
        totalMsgs += 1
        if msgInfo[0] in msgSizeDist:
            msgSizeDist[msgInfo[0]] += 1
        else:
            msgSizeDist[msgInfo[0]] = 1
    prevProb = 0
    writeToFile(fd4, [0.0, 0])
    for msgSize, count in msgSizeDist.iteritems():
        currentProb = count*1.0/totalMsgs
        writeToFile(fd4, ['{0:6f}'.format(prevProb + currentProb), msgSize])
        prevProb += currentProb 

    fd.close() 
    f.close()
    fd1.close()
    fd2.close()
    fd3.close()
    fd4.close()
    fd5.close()
