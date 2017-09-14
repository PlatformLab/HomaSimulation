import sys
from scipy.optimize import basinhopping
import numpy as np
from thresholdOptimizer import *


rho = 0.5
def tauZigon(thetas):
    sumTaus = 0.0
    for l in range(0, len(thetas)):
        sumThetas = 1-rho*sum(thetas[0:l])
        sumTaus += thetas[l]*1.0/sumThetas
    sumTaus += (1-sum(thetas))/(1-rho*sum(thetas))
    return -sumTaus

#minimizer_kwargs = {"method":"L-BFGS-B", "jac":True}
#minimizer_kwargs = {"method": "BFGS"}
minimizer_kwargs = {"method":"L-BFGS-B"}
K = 8
thetas_init = [0.8]
for i in range(K-2):
    thetas_init.append((1-sum(thetas_init))/3)

class MyBounds(object):
    def __init__(self, xmin=[0,0,0,0,0,0,0]):
        self.xmin = np.array(xmin)
    def __call__(self, **kwargs):
        x = kwargs["x_new"]
        tmin = bool(np.all(x >= self.xmin))
        return tmin and (sum(x) < 1)

mybounds = MyBounds()
ret = basinhopping(tauZigon, thetas_init,
    minimizer_kwargs=minimizer_kwargs, niter=200,
    accept_test=mybounds)

print ret.x
print sum(ret.x)

thetas = list(ret.x)
thetas.append(1-sum(thetas))
alphas = cdfInvOfSumThetas(thetas)
print alphas
print [alpha/1442 for alpha in alphas]


minimizer_kwargs = {"method":"L-BFGS-B"}
K = 8
thetas_init = [0.5]
for i in range(K-1):
    thetas_init.append((1-sum(thetas_init))/2)

thetas_init[-1] = 1-sum(thetas_init[0:-1])

def tau_2(thetas):
    allLambdas = lambdas(thetas, cdfInvOfSumThetas(thetas)) 
    sumT = 0.0
    for l in range(K):
        t = 1.0 / (mu - sum(allLambdas[0:l]))
        t *= sum(thetas[l-1:K])
        sumT += t
    return sumT

class HisBounds(object):
    def __init__(self, xmin=[0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0]):
        self.xmin = np.array(xmin)
    def __call__(self, **kwargs):
        x = kwargs["x_new"]
        tmin = bool(np.all(x >= self.xmin))
        return tmin and (sum(x) == 1.0)

hisbounds = HisBounds()
ret = basinhopping(tau_2, thetas_init,
    minimizer_kwargs=minimizer_kwargs, niter=200000,
    accept_test=hisbounds)

print ret.x
print sum(ret.x)

thetas = list(ret.x)
thetas.append(1-sum(thetas))
alphas = cdfInvOfSumThetas(thetas)
print alphas
print [alpha/1442 for alpha in alphas]

