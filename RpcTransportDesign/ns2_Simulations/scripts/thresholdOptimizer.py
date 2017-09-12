import bisect
import sys

rate = 1e10/8 # in bytes per seconds
lf = 0.5
lambdaIn = lf*rate
mu = rate
cdfFile = 'CDF_search.tcl'
cdfFileFd = open(cdfFile)

cdf = list()
pdf = list() # = derivative of cdf = dF/d(size)
cdfInverse = list()
cdf.extend([[0.0], [0.0]])
pdf.extend([[0.0], [0.0]])
cdfInverse.extend([[0.0], [0.0]])
prevProb = 0.0
prevSize = 0.0
thetas_init = [0.5, 0.5/2, 0.5/4, 0.5/8, 0.5/16, 0.5/32, 0.5/32]
K = 8
alpha = .001

for line in cdfFileFd:
    elem = line.split()
    if elem == []:
        continue
    size = float(elem[0])
    prob = float(elem[2])
    if prob == prevProb:
        continue
    cdf[0].append(size)
    cdf[1].append(prob)
    cdfInverse[0].append(prob)
    cdfInverse[1].append(size)
    pdf[0].append(size)
    pdf[1].append((prob-prevProb)/(size-prevSize))
    prevProb = prob
    prevSize = size

cdfFileFd.close()

weightedBytes = 0.0
lastSize = pdf[0][0]
for (size,prob) in zip(pdf[0][1:], pdf[1][1:]):
    weightedBytes += 0.5 * (size**2 - lastSize**2) * prob
    lastSize = size

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

#def getCdfPrime(size):
#    """
#    linearly interpolates the derivative of cdf and returns the interpolated
#    value for the derivative of cdf 
#    """
#    sizes = pdf[0]
#    probs = pdf[1]
#    index = bisect.bisect_left(sizes, size)
#    if index==len(sizes):
#        index = -1 
#    if index == 0:
#        index = 1 
#    return probs[index-1] +  (size - sizes[index-1]) * (probs[index] -\
#        probs[index-1]) / (sizes[index] - sizes[index-1])
#
def getPdf(size):
    sizes = pdf[0]
    probs = pdf[1]
    index = bisect.bisect_left(sizes, size)
    if index == len(sizes):
        index -= 1
    return probs[index]

def cdfInvOfSumThetas(thetas):
    cdfInvSumThetas = []
    prevSum = 0.0
    prevInv = 0.0
    for theta in thetas:
        inv = getCdfInv(theta+prevSum)
        cdfInvSumThetas.append(inv)
        prevInv = inv
        prevSum = theta + prevSum
    return cdfInvSumThetas

def lambdas(thetas, cdfInvSumThetas):
    allLambdas = []
    prevInv = 0.0
    for i, theta in enumerate(thetas):
        inv = cdfInvSumThetas[i]
        lamda = lambdaIn * theta * (inv - prevInv) / weightedBytes
        allLambdas.append(lamda)
        prevInv = inv
    return allLambdas

def lambdaPrime(m, n, thetas, cdfInvSumThetas):
    """
    find the d(lambda_m)/d(theta_n)
    """
    t = [0.0] + thetas
    cdfInv = [0.0] + cdfInvSumThetas
    if m < n:
        return 0.0
    if m == n:
        ret = (cdfInv[m] - cdfInv[m-1]) + t[m] / getPdf(cdfInv[m])
        return ret * lambdaIn / weightedBytes
    else:
        ret = t[m] / (getPdf(cdfInv[m]) - getPdf(cdfInv[m-1]))
        return ret * lambdaIn / weightedBytes

def gradient(n, thetas, cdfInvSumThetas, allLambdas):
    """
    compute the derivative of the T with respect to the theta_n
    """
    gradient = 0.0
    for m in range(n):
        sumLambdas = sum(allLambdas[0:m+1])
        gradient += 1/(mu - sumLambdas)

    for l in range(1, K+1):
        subGradient = 0.0
        for m in range(n, l+1):
            denom = (1 - sum(allLambdas[0:m])) ** 2
            sumLambdaPrimes = 0.0
            for j in range(n, m+1):
                sumLambdaPrimes += lambdaPrime(j, n,
                    thetas, cdfInvSumThetas)
            sumLambdaPrimes /= denom
            subGradient += sumLambdaPrimes
        gradient += thetas[l-1] * subGradient

    return gradient

def main():
    iters = 1000
    thetas = thetas_init
    for it in range(iters):
        cdfInvSumThetas = cdfInvOfSumThetas(thetas)
        allLambdas = lambdas(thetas, cdfInvSumThetas)
        gradients = []
        for n in range(1,K+1):
            gradients.append(gradient(n, thetas, cdfInvSumThetas,
                allLambdas))

        thetas = [thetas[i]-alpha*gradients[i]*thetas[i] for i in range(K)]
        print thetas
        for i in range(len(thetas)):
            thetas[i] = max(thetas[i], 0)
        thetas[-1] = max(1 - sum(thetas[0:-1]), 0)
        print sum(thetas)


    aplphas = cdfInvOfSumThetas(thetas)
    print alphas
    print [alpha/1460 for alpha in alphas]

if __name__ == '__main__':
   sys.exit(main());
