import bisect
import sys
import os
from numpy import *

sys.path.insert(0, os.environ['HOME'] +\
    '/Research/RpcTransportDesign/OMNeT++Simulation/analysis')
from parseResultFiles import *

rate = 1e6/8.0 # in bytes per seconds
mu = rate
K = 8

conf = AttrDict()
conf.lf = [0.5, 0.8]
conf.lambdaIn = [lf*rate for lf in conf.lf]
conf.cdfFile = ['FacebookKeyValueMsgSizeDist.tcl','Google_AllRPC.tcl',
    'Google_SearchRPC.tcl', 'Facebook_HadoopDist_All.tcl',
    'CDF_search.tcl']
conf.learnRate = [.1,.1,.1,.1,.1] #one for each cdfFile
conf.thetas_init = [] # one for cdfFile

def geometricThetaInit(start, expStep):
    thetas_init = [start]
    for i in range(K-1):
        thetas_init.append((1-sum(thetas_init))/expStep)
    thetas_init[-1] = 1-sum(thetas_init[0:-1])
    return thetas_init

#thetas_init = [1.0/8.0]*8
conf.thetas_init.extend([geometricThetaInit(0.8, 4.0)]*5)

alphasOpt = [1059*1460, 1412*1460, 1643*1460, 1869*1460,\
    2008*1460, 2115*1460, 2184*1460]
#alphasOpt =[909*1460 ,1329*1460 ,1648*1460 ,1960*1460 ,\
#    2143*1460 ,2337*1460 ,2484*1460]


cdf = list()
pdf = list()
cdfInverse = list()
weightedBytes = 1.0

def readCdfFile(cdfFile):
    global cdf
    global pdf
    global cdfInverse
    global weightedBytes
    cdfFileFd = open(cdfFile)
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


def main(lambdaIn, cdfFile, learnRate, thetas_init):
    readCdfFile(cdfFile)
    iters = 100000
    thetas = thetas_init

    alphas = cdfInvOfSumThetas(thetas)
    print "initial thetas:"
    print thetas_init
    print "initial alphas:"
    print alphas
    print [alpha/1442 for alpha in alphas]
    print"\n"

    for it in range(iters):
        cdfInvSumThetas = cdfInvOfSumThetas(thetas)
        #print cdfInvSumThetas

        allLambdas = lambdas(thetas, cdfInvSumThetas, lambdaIn)
        #print allLambdas
        if it % 100 == 0:
            print 'tau: {0} at iter: {1}'.format(tau(thetas, allLambdas), it)

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
    print "-"*100
    print "computed thetas:"
    print thetas
    print "computed alphas:"
    alphas = cdfInvOfSumThetas(thetas)
    print alphas
    print [alpha/1442 for alpha in alphas]
    print "computed tau:"
    print tau(thetas, lambdas(thetas, cdfInvOfSumThetas(thetas), lambdaIn))

    thetasOpt = []
    prevAlphaCdf = 0.0
    for alpha in alphasOpt:
        alpha = alpha/1460*1442
        alphaCdf = getCdf(alpha)
        thetasOpt.append(alphaCdf - prevAlphaCdf)
        prevAlphaCdf = alphaCdf

    thetasOpt.append(1-alphaCdf)
    print "-"*100
    print "optimum thetas:"
    print thetasOpt
    print "optimum alphas:"
    alphas = cdfInvOfSumThetas(thetasOpt)
    print alphas
    print [alpha/1442 for alpha in alphas]
    print "optimum tau:"
    print tau(thetasOpt, lambdas(thetasOpt, cdfInvOfSumThetas(thetasOpt),
        lambdaIn))

if __name__ == '__main__':
   lambdaIn = conf.lambdaIn[0]
   cdfFile = conf.cdfFile[4]
   learnRate = conf.learnRate[4]
   thetas_init = conf.thetas_init[4]
   sys.exit(main(lambdaIn, cdfFile, learnRate, thetas_init))
