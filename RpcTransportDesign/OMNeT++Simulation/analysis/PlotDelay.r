#!/usr/bin/Rscript

library(reshape2)
library(ggplot2)
library(gridExtra)

pktDelays = c()
delayCDF = c()
loadFactor = c()

pktDelaysRawData <- read.table("DcnTopo-0.25.vec", col.names=c('vecId','rowInd','simTime','delay'))
delayRange <- seq(min(pktDelaysRawData$delay), max(pktDelaysRawData$delay), (max(pktDelaysRawData$delay)-min(pktDelaysRawData$delay))/20000 )
pktDelays <- c(pktDelays, delayRange)
loadFactor <- c(loadFactor, rep(0.25, length(delayRange)))
P <- ecdf(pktDelaysRawData$delay)
delayCDF <- c(delayCDF, P(delayRange))

pktDelaysRawData <- read.table("DcnTopo-0.5.vec", col.names=c('vecId','rowInd','simTime','delay'))
pktDelays <- c(pktDelays, delayRange)
loadFactor <- c(loadFactor, rep(0.5, length(delayRange)))
P <- ecdf(pktDelaysRawData$delay)
delayCDF <- c(delayCDF, P(delayRange))

pktDelaysRawData <- read.table("DcnTopo-0.8.vec", col.names=c('vecId','rowInd','simTime','delay'))
pktDelays <- c(pktDelays, delayRange)
loadFactor <- c(loadFactor, rep(0.8, length(delayRange)))
P <- ecdf(pktDelaysRawData$delay)
delayCDF <- c(delayCDF, P(delayRange))

pktDelaysRawData <- read.table("DcnTopo-0.95.vec", col.names=c('vecId','rowInd','simTime','delay'))
pktDelays <- c(pktDelays, delayRange)
loadFactor <- c(loadFactor, rep(0.95, length(delayRange)))
P <- ecdf(pktDelaysRawData$delay)
delayCDF <- c(delayCDF, P(delayRange))


pktDelaysCDF <- data.frame(load_factor=loadFactor, delays=pktDelays, CDF=delayCDF)
pktDelaysCDF$load_factor <- factor(pktDelaysCDF$load_factor)
CDFDelayPlot <- ggplot(pktDelaysCDF, aes(x=delays, y=1-CDF))
pdf("PacketDelayDist.pdf")
CDFDelayPlot + facet_wrap(~load_factor)+ geom_line(size = 2, alpha = 1/2)
dev.off()

