import bisect
import sys
import os
from numpy import *
import time
from multiprocessing import Process, Queue

sys.path.insert(0, os.environ['HOME'] +\
    '/Research/RpcTransportDesign/OMNeT++Simulation/analysis')
from parseResultFiles import *

rate = 1e5/8.0 # in bytes per seconds
mu = rate
K = 8

conf = AttrDict()
conf.lf = [0.5, 0.8]
conf.lambdaIn = [lf*rate for lf in conf.lf]
conf.cdfFile = ['FacebookKeyValueMsgSizeDist.tcl', 'Google_SearchRPC.tcl',
    'Google_AllRPC.tcl', 'Facebook_HadoopDist_All.tcl', 'CDF_search.tcl']
conf.learnRate = [.01,.001,.01,.1,.1] #one for each cdfFile
conf.thetas_init = [] # one for cdfFile

def geometricThetaInit(start, expStep):
    thetas_init = [start]
    for i in range(K-1):
        thetas_init.append((1-sum(thetas_init))/expStep)
    thetas_init[-1] = 1-sum(thetas_init[0:-1])
    return thetas_init

#thetas_init = [1.0/8.0]*8
conf.thetas_init.extend([geometricThetaInit(0.75, 2.0)]*5)

# values for CDF_search.tcl workload from pias github repository
alphasOpt_50 = [1059*1460, 1412*1460, 1643*1460, 1869*1460,\
    2008*1460, 2115*1460, 2184*1460]
alphasOpt_80 =[909*1460 ,1329*1460 ,1648*1460 ,1960*1460 ,\
    2143*1460 ,2337*1460 ,2484*1460]


cdf = list()
pdf = list()
cdfInverse = list()
weightedBytes = 1.0

def readCdfFile(cdfFile):
    global cdf
    global pdf
    global cdfInverse
    global weightedBytes
    cdfFileFd = open(cdfFile, 'r')
    cdf = [[0.0], [0.0]]
    pdf = [[0.0], [0.0]] # = derivative of cdf = dF/d(size)
    cdfInverse = [[0.0], [0.0]]
    prevProb = 0.0
    prevSize = 0.0
    for line in cdfFileFd:
        elem = line.split()
        if elem == []:
            continue
        size = float(elem[0])
        #size /= 1442.0
        #size *= 1460.0
        prob = float(elem[2])
        if prob == prevProb:
            continue
        cdf[0].append(size)
        cdf[1].append(prob)
        cdfInverse[0].append(prob)
        cdfInverse[1].append(size)
        pdf[0].append(size)
        pdf[1].append((prob-prevProb) / (size-prevSize))
        prevProb = prob
        prevSize = size

    cdfFileFd.close()

    #lastSize = pdf[0][0]
    #for (size,prob) in zip(pdf[0][1:], pdf[1][1:]):
    #    weightedBytes += 0.5 * (size**2 - lastSize**2) * prob
    #    lastSize = size

def getCdf(size):
    sizes = cdf[0]
    probs = cdf[1]
    index = bisect.bisect_left(sizes, size)
    if index==len(sizes):
        return probs[index-1]
    if index == 0:
        return probs[0]
    return probs[index-1] +  (size - sizes[index-1]) * (probs[index] -\
        probs[index-1]) / (sizes[index] - sizes[index-1])

def getCdfInv(prob):
    probs = cdfInverse[0]
    sizes = cdfInverse[1]
    index = bisect.bisect_left(probs, prob)
    if index==len(probs):
        return sizes[index-1]
    if index == 0:
        return sizes[0]
    return sizes[index-1] +  (prob - probs[index-1]) * (sizes[index] -\
        sizes[index-1]) / (probs[index] - probs[index-1])

def getCdfPrime(size):
    """
    linearly interpolates the derivative of cdf and returns the interpolated
    value for the derivative of cdf. This method can be used instead of getPdf
    """
    sizes = pdf[0]
    probs = pdf[1]
    index = bisect.bisect_left(sizes, size)
    if index==len(sizes):
        return probs[index-1]
    if index == 0:
        return probs[0]
    return probs[index-1] +  (size - sizes[index-1]) * (probs[index] -\
        probs[index-1]) / (sizes[index] - sizes[index-1])

def getPdf(size):
    sizes = pdf[0]
    probs = pdf[1]
    index = bisect.bisect_left(sizes, size)
    if index == len(sizes):
        index -= 1
    return probs[index]

def cdfInvOfSumThetas(thetas):
    cdfInvSumThetas = []
    sumThetas = 0.0
    for theta in thetas:
        sumThetas += theta
        inv = getCdfInv(sumThetas)
        cdfInvSumThetas.append(inv)
    return cdfInvSumThetas

def lambdas(thetas, cdfInvSumThetas, lambdaIn):
    global weightedBytes
    allLambdas = []
    prevInv = 0.0
    sumThetas = 0.0
    for i, theta in enumerate(thetas):
        inv = cdfInvSumThetas[i]
        lamda = (1 - sumThetas) * (inv - prevInv)
        sumThetas += theta
        prevInv = inv
        allLambdas.append(lamda)
    weightedBytes = sum(allLambdas)
    #print weightedBytes
    allLambdas = [lamda*lambdaIn/weightedBytes for lamda in allLambdas]
    return allLambdas

def lambdaPrime(m, n, thetas, cdfInvSumThetas, lambdaIn):
    """
    find the d(lambda_m)/d(theta_n)
    """
    #seterr(divide='ignore')
    #divideZero = lambda a, b: a * 1.0 / b if b!=0 else 0.0
    if m < n:
        return 0.0
    t = [0.0] + thetas
    cdfInv = [0.0] + cdfInvSumThetas
    if m == n:
        ret = (1 - sum(t[0:m])) * divide(1, getPdf(cdfInv[m]))
    else:
        ret = (1 - sum(t[0:m])) * (divide(1, getPdf(cdfInv[m])) -\
            divide(1, getPdf(cdfInv[m-1]))) - (cdfInv[m] - cdfInv[m-1])
    return ret * lambdaIn / weightedBytes

def gradient(n, thetas, cdfInvSumThetas, allLambdas, lambdaIn):
    """
    compute the derivative of the T with respect to the theta_n
    """
    gradient = 0.0
    for m in range(1, n+1):
        sumLambdas = sum(allLambdas[0:m])
        gradient += 1/(mu - sumLambdas)

    for l in range(1, K+1):
        subGradient = 0.0
        for m in range(n, l+1):
            denom = (mu - sum(allLambdas[0:m])) ** 2
            sumLambdaPrimes = 0.0
            for j in range(n, m+1):
                sumLambdaPrimes += lambdaPrime(j, n,
                    thetas, cdfInvSumThetas, lambdaIn)
            sumLambdaPrimes /= denom
            subGradient += sumLambdaPrimes
        gradient += thetas[l-1] * subGradient

    return gradient

def tau(thetas, allLambdas):
    """
    """
    tau_ = 0.0
    for l in range(1, K+1):
        subTau = 0.0
        for m in range(1, l+1):
            denom = mu - sum(allLambdas[0:m])
            subTau += 1.0/denom
        tau_ += subTau * thetas[l-1]
    return tau_

def tau_2ndForm(thetas, allLambdas):
    sumT = 0.0
    for l in range(1, K+1):
        t = 1.0 / (mu - sum(allLambdas[0:l]))
        t *= (1 - sum(thetas[0:l-1]))
        sumT += t
    return sumT

def test(thetas):
    """
    this test enforces the constraint of sum(thetas) == 1 using the algorithm
    given in paper: "Projection onto the probability simplex"
    """
    #thetas[-1] = max(1 - sum(thetas[0:-1]), 0)
    #thetas = [theta/sum(thetas) for theta in thetas]
    U = sorted(thetas)
    rho = max([j+1 for j,u in enumerate(U) if
        (u + (1-sum(U[0:j+1]))/(j+1)) > 0])
    lamda = (1 - sum(U[0:rho]))/rho
    thetas = [max(theta+lamda, 0) for theta in thetas]
    return thetas


def main(lambdaIn, cdfFile, learnRate, thetas_init, outQueue=''):
    readCdfFile(cdfFile)
    iters = 20000
    thetas = thetas_init

    for it in range(iters):
        cdfInvSumThetas = cdfInvOfSumThetas(thetas)
        #print cdfInvSumThetas

        allLambdas = lambdas(thetas, cdfInvSumThetas, lambdaIn)
        #print allLambdas
        #if it % 100 == 0:
        #    print 'tau: {0} at iter: {1}, procId: {2}'.format(tau(thetas,
        #        allLambdas), it, os.getpid())

        gradients = []
        for n in range(1,K+1):
            gradients.append(gradient(n, thetas, cdfInvSumThetas,
                allLambdas, lambdaIn))
        #print gradients

        thetas = [thetas[i] - learnRate * gradients[i] for i in range(K)]
        for i in range(K):
            thetas[i] = max(thetas[i], 0)

        thetas = test(thetas)

    #allLambdas = lambdas(thetas, cdfInvOfSumThetas(thetas), lambdaIn)
    #print tau_2ndForm(thetas)
    tau_ = tau(thetas, allLambdas)

    if lambdaIn <= rate*.65:
        alphasOpt = alphasOpt_50
    else:
        alphasOpt = alphasOpt_80

    thetasOpt = []
    prevAlphaCdf = 0.0
    readCdfFile('CDF_search.tcl')
    for alpha in alphasOpt:
        alpha = alpha/1460*1442
        alphaCdf = getCdf(alpha)
        thetasOpt.append(alphaCdf - prevAlphaCdf)
        prevAlphaCdf = alphaCdf
    thetasOpt.append(1-alphaCdf)
    readCdfFile(cdfFile)
    tauOpt_ = tau(thetasOpt, lambdas(thetasOpt, cdfInvOfSumThetas(thetasOpt),
            lambdaIn))
 
    if outQueue == '':
        print "-"*100
        alphas = cdfInvOfSumThetas(thetas_init)
        print "initial thetas:"
        print thetas_init
        print "initial alphas:"
        print alphas
        print [alpha/1442 for alpha in alphas]

        print "-"*100
        print "computed thetas:"
        print thetas
        print "computed alphas:"
        alphas = cdfInvOfSumThetas(thetas)
        print alphas
        print [alpha/1442 for alpha in alphas]
        print "computed tau:"
        print tau(thetas, lambdas(thetas, cdfInvOfSumThetas(thetas), lambdaIn))

        print "-"*100
        print "optimum thetas:"
        print thetasOpt
        print "optimum alphas:"
        alphasOpt = cdfInvOfSumThetas(thetasOpt)
        print alphasOpt
        print [alpha/1442 for alpha in alphasOpt]
        print "optimum tau:"
        print tau(thetasOpt, lambdas(thetasOpt, cdfInvOfSumThetas(thetasOpt),
            lambdaIn))
    else:
        #assert(isinstance(outQueue, Queue))
        alphas = cdfInvOfSumThetas(thetas)
        outQueue.put((cdfFile, lambdaIn/rate, alphas, tau_, tauOpt_))

def worker(qMainArgs, qOut):
    while not(qMainArgs.empty()):
        mainArgs = qMainArgs.get(block = 0)
        lambdaIn = mainArgs[0]
        cdfFile = mainArgs[1]
        learnRate = mainArgs[2]
        thetas_init = mainArgs[3]
        main(lambdaIn, cdfFile, learnRate, thetas_init, qOut)

    return

if __name__ == '__main__':
    parser = OptionParser(description='This script runs gradient descent'
        'optimizer to compute pias optimal thresholds.')
    parser.add_option('--mode', metavar='master/slave',
        default = '', dest='mode', type='string',
        help='master modes one specific run, slave mode spawns many threads'
        ' and run many simulations at onces')
    parser.add_option('--lf', metavar= '0.5', default = 0.5, dest='lf',
        type='float', help='The average load factor of the network.')
    parser.add_option('--cdfFile', metavar= "FILENAME", dest='cdfFile',
        default = "CDF_search.tcl", type='string',
        help='Name of the file containing distribution of mesg/flow size'
        ' for which we compute pias thresholds.')
    parser.add_option('--lr', metavar= '0.1', default = 0.1, dest='lr',
        type='float', help='The learning rate in gradient-descnet methods.')
    parser.add_option('--thetas_init', metavar= '0.1,0.2,..,0.01', default =
        '1,1,1,1,1,1,1,1', dest='thetas_init', type='string',
        help='a string of 8 comma separated numbers to be used for initial '
        'point of thetas in gradient descent. If sum of the numbers is not '
        'one, the number will be normalized to their sum.')
    options, args = parser.parse_args()

    if options.mode == 'master':
        #will contain output of python processes that run main()
        qArgs = Queue()
        for lambdaIn in conf.lambdaIn:
            for i,cdfFile in enumerate(conf.cdfFile):
                qArgs.put((lambdaIn, cdfFile, conf.learnRate[i],
                    conf.thetas_init[i]))

        #Create all worker processes
        procs = []
        number_worker_procs = 4

        #Start to process jobs
        qOut = Queue()
        for i in range(number_worker_procs):
            p = Process(target = worker, args=(qArgs,qOut,))
            procs.append(p)
            p.start()
            time.sleep(1)

        #Join all completed processes
        for p in procs:
            p.join()

        while not(qOut.empty()):
            out = qOut.get()
            print 'cdf: {0}, load: {1}'.format(out[0], out[1])
            print '\talphas:' + str(out[2])
            print '\ttau:' + str(out[3])
            print '\ttauOpt:' + str(out[4])
            print "-"*100
        sys.exit()

    if options.mode == 'slave':
        lambdaIn = options.lf * rate
        cdfFile = options.cdfFile
        learnRate = options.lr
        thetas_init = [float(theta) for theta in options.thetas_init.split(',')]
        thetas_init = [theta/sum(thetas_init) for theta in thetas_init]
        sys.exit(main(lambdaIn, cdfFile, learnRate, thetas_init))

    else:
        lambdaIn = conf.lambdaIn[1]
        i=4
        cdfFile = conf.cdfFile[i]
        learnRate = conf.learnRate[i]
        thetas_init = conf.thetas_init[i]
        sys.exit(main(lambdaIn, cdfFile, learnRate, thetas_init))
