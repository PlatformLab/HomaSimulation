#!/usr/bin/Rscript

library(reshape2)
library(ggplot2)
library(gridExtra)
library(grid)
library(plyr)

wastedBw <- read.table("wastedBw.txt", na.strings = "NA",
    col.names=c('transport', 'workload', 'prioLevels', 'redundancyFactor',
    'loadFactor', 'nominalLoadFactor', 'sxWastedBw', 'sxActiveWastedBw',
    'wastedBw', 'activeWastedBw', 'totalWastedOversubBw',
    'oversubWastedOversubBw'), header=TRUE)

wastedBw$redundancyFactor <- factor(wastedBw$redundancyFactor)
wastedBw$prioLevels <- factor(wastedBw$prioLevels)

allW <- c('W1', 'W2', 'W3', 'W4', 'W5')
allWorkloads <- c('FACEBOOK_KEY_VALUE', 'GOOGLE_SEARCH_RPC',
    'FABRICATED_HEAVY_MIDDLE', 'GOOGLE_ALL_RPC','FACEBOOK_HADOOP_ALL')
workloads <- allWorkloads[allWorkloads %in% unique(wastedBw$workload)]

levels(wastedBw$workload) <- allW[allWorkloads %in% unique(wastedBw$workload)]
wastedBw$workload <- as.character(wastedBw$workload)
wastedBw <- wastedBw[with(wastedBw, order(transport, workload)),]
wastedBw$workload <- as.factor(wastedBw$workload)

textSize = 10
yLab = 'Receiver\'s Wasted Bandwidth(%)'
xLab = 'Network Load (%)'
#yLimit = ceiling(max(wastedBw$wastedBw) / 10)*10
yLimit = 75

i <- 0
plotList <- list()
plotTitle = "Homa's Wasted BW vs. BW Utilization"
for (workload in unique(wastedBw$workload)) {
    i <- i+1
    plotTitle = sprintf("Workload: %s", workload)
    plotList[[i]] <- ggplot(wastedBw[wastedBw$workload==workload,]) +
        geom_line(aes(x=nominalLoadFactor, y=wastedBw, colour=prioLevels),
            size = 1, alpha = 2/3) +
        geom_line(aes(x=nominalLoadFactor, y=totalWastedOversubBw,
            colour=prioLevels), size = 1, alpha = 2/3) +
        geom_line(data=data.frame(nominalLoadFactor=0:100, wastedBw=100:0),
            aes(x=nominalLoadFactor, y=wastedBw), color='magenta',
            size = 1, alpha = 2/3) +
        theme(text = element_text(size=textSize, face="bold", color='black'),
            axis.text = element_text(size=1.3*textSize, color='black'),
            panel.background = element_rect(fill = "white"),
            legend.position = c(0.9, 0.7),
            plot.margin=unit(c(0,0.3,0,0),"cm"),
            strip.text.x = element_text(size = 2*textSize),
            strip.text.y = element_text(size = 2*textSize),
            plot.title = element_text(size = textSize)) +
        coord_cartesian(ylim=c(0,yLimit), xlim=c(0,100)) +
        labs(title = plotTitle, x = xLab, y = yLab)
}

pdf("plots/wastedBw/wastedBwVsPrioLevels.pdf", width=3, height=3)
args.list <- c(plotList, list(ncol=length(workloads)))
do.call(grid.arrange, args.list)
dev.off()


textSize = 10
yLab = 'Sender\'s Wasted Bandwidth(%)'
xLab = 'Network Load (%)'
yLimit = 75

i <- 0
plotList <- list()
plotTitle = "Homa's Wasted BW vs. BW Utilization"
for (workload in unique(wastedBw$workload)) {
    i <- i+1
    plotTitle = sprintf("Workload: %s", workload)
    plotList[[i]] <- ggplot(wastedBw[wastedBw$workload==workload,]) +
        geom_line(aes(x=loadFactor, y=sxWastedBw, colour=prioLevels),
            size = 1, alpha = 2/3) +
        geom_line(aes(x=loadFactor, y=totalWastedOversubBw,
            colour=prioLevels), size = 1, alpha = 2/3) +
        geom_line(data=data.frame(loadFactor=0:100, sxWastedBw=100:0),
            aes(x=loadFactor, y=sxWastedBw), color='magenta',
            size = 1, alpha = 2/3) +
        theme(text = element_text(size=textSize, face="bold", color='black'),
            axis.text = element_text(size=1.3*textSize, color='black'),
            panel.background = element_rect(fill = "white"),
            legend.position = c(0.9, 0.7),
            plot.margin=unit(c(0,0.3,0,0),"cm"),
            strip.text.x = element_text(size = 2*textSize),
            strip.text.y = element_text(size = 2*textSize),
            plot.title = element_text(size = textSize)) +
        coord_cartesian(ylim=c(0,yLimit), xlim=c(0,100)) +
        labs(title = plotTitle, x = xLab, y = yLab)
}

pdf("plots/wastedBw/wastedBwSenders.pdf", width=3, height=3)
args.list <- c(plotList, list(ncol=length(workloads)))
do.call(grid.arrange, args.list)
dev.off()


yLimit = 100
textSize = 15
lineTypes = c("solid", "twodash", "longdash", "dashed", "dotdash", "dotted")
lineColors = c('#e41a1c', '#377eb8', '#4daf4a', '#984ea3', '#ff7f00',
    '#f781bf', '#a65628')
lineSizes = c(1.5, 1.4, 1.3, 1.2, 1.1, 1, 0.8)

tmp <- subset(wastedBw, workload=='W4', select=c('nominalLoadFactor',
    'totalWastedOversubBw', 'redundancyFactor'))
levels(tmp$redundancyFactor) <-
    paste(levels(tmp$redundancyFactor), "Sched. Prio.")
tmp <- rbind(tmp, data=data.frame(nominalLoadFactor=0:100, totalWastedOversubBw=100:0,
    redundancyFactor=rep('Surplus BW.')) )

i <- 1
plotTitle = ''
plotList <- list()
plotList[[i]] <- ggplot(tmp) +
    geom_line(aes(x=nominalLoadFactor, y=totalWastedOversubBw,
        color=redundancyFactor, size = redundancyFactor), alpha = 2/3) +

    theme_classic() +
    theme(text = element_text(size=textSize, color='black'),
        axis.text = element_text(size=1.1*textSize, color='black'),
        axis.title.y = element_text(size=1.2*textSize, hjust=1),
        axis.title.x = element_text(size=1.2*textSize),
        panel.background = element_rect(fill = "white"),
        panel.grid.major = element_line(linetype = 'dotted',
            colour = "black"),
        legend.position = c(0.8, 0.75),
        legend.text = element_text(size =1.2*textSize),
        plot.margin=unit(c(-.4,0.6,0,0),"cm"),
        strip.text.x = element_text(size = 2*textSize),
        strip.text.y = element_text(size = 2*textSize),
        plot.title = element_text(size = textSize),
        #legend.background = element_rect(fill = "transparent",
        #    colour = "transparent"),
        legend.title = element_blank()) +

    coord_cartesian(ylim=c(0,yLimit), xlim=c(0,100)) +
    labs(title = plotTitle, x = xLab, y = yLab) +
    scale_linetype_manual(values = lineTypes) +
    scale_color_manual(values = lineColors) +
    scale_size_manual(values = lineSizes)

pdf("plots/wastedBw/oversubWastedBw.pdf", width=5, height=4)
args.list <- c(plotList, list(ncol=1))
do.call(grid.arrange, args.list)
dev.off()
