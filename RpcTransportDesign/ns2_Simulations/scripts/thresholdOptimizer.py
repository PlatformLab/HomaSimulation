import bisect
import sys
from numpy import *

#cdfFile = 'originalPias/CDF_dctcp.tcl'
cdfFile = 'CDF_search.tcl'
#avgFlowSize = 1744.70*1442.0 #in bytes
#rate = 1e10/8.0/avgFlowSize # in flows per seconds
rate = 1e10/8.0 # in bytes per seconds
lf = 0.5
lambdaIn = lf*rate
mu = rate
K = 8
learnRate = 0.01
thetas_init = [0.8]
for i in range(K-1):
    thetas_init.append((1-sum(thetas_init))/3)

thetas_init[-1] = 1-sum(thetas_init[0:-1])
#thetas_init = [1.0/8.0]*8

alphasOpt = [1059*1460, 1412*1460, 1643*1460, 1869*1460,\
    2008*1460, 2115*1460, 2184*1460]

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

weightedBytes = 1.0
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

def lambdas(thetas, cdfInvSumThetas):
    global weightedBytes
    allLambdas = []
    prevInv = 0.0
    for i, theta in enumerate(thetas):
        inv = cdfInvSumThetas[i]
        lamda = theta * (inv - prevInv)
        prevInv = inv
        allLambdas.append(lamda)
    weightedBytes = sum(allLambdas)
    #print weightedBytes
    allLambdas = [lamda*lambdaIn/weightedBytes for lamda in allLambdas]
    return allLambdas

def lambdaPrime(m, n, thetas, cdfInvSumThetas):
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
        ret = (cdfInv[m] - cdfInv[m-1]) + divide(t[m], getPdf(cdfInv[m]))
        return ret * lambdaIn / weightedBytes
    else:
        ret = t[m] * divide(1, getPdf(cdfInv[m])) - \
            t[m] * divide(1, getPdf(cdfInv[m-1]))
        return ret * lambdaIn / weightedBytes

def gradient(n, thetas, cdfInvSumThetas, allLambdas):
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
                    thetas, cdfInvSumThetas)
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

def main():
    iters = 1000
    thetas = thetas_init

    alphas = cdfInvOfSumThetas(thetas)
    print alphas
    print [alpha/1442 for alpha in alphas]
    print"\n\n\n"

    for it in range(iters):
        cdfInvSumThetas = cdfInvOfSumThetas(thetas)
        #print cdfInvSumThetas

        allLambdas = lambdas(thetas, cdfInvSumThetas)
        #print allLambdas
        #print tau(thetas, allLambdas)

        gradients = []
        for n in range(1,K+1):
            gradients.append(gradient(n, thetas, cdfInvSumThetas,
                allLambdas))
        #print gradients

        thetas = [thetas[i] - learnRate * gradients[i] for i in range(K)]
        for i in range(K):
            thetas[i] = max(thetas[i], 0)
        thetas[-1] = max(1 - sum(thetas[0:-1]), 0)
        #thetas = [theta/sum(thetas) for theta in thetas]


    print thetas
    alphas = cdfInvOfSumThetas(thetas)
    print alphas
    print [alpha/1442 for alpha in alphas]

    thetasOpt = []
    prevAlphaCdf = 0.0
    for alpha in alphasOpt:
        alphaCdf = getCdf(alpha)
        thetasOpt.append(alphaCdf - prevAlphaCdf)
        prevAlphaCdf = alphaCdf
    thetasOpt.append(1-alphaCdf)
    print "optimum thetas:"
    print thetasOpt
    print "optimum tau:"
    print tau(thetasOpt, lambdas(thetasOpt, cdfInvOfSumThetas(thetasOpt)))

if __name__ == '__main__':
   sys.exit(main());
