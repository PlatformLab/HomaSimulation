#!/usr/bin/Rscript
library(reshape2)
library(ggplot2)
library(grid)
library(gridExtra)
library(plyr)

queueLen <- read.table("queueLenStats.txt", na.strings = "NA",
    col.names=c('TransportType', 'LoadFactor', 'WorkLoad',
    'UnschedBytes', 'PrioLevels', 'SchedPrios', 'RedundancyFactor',
    'QueueLocation', 'Metric', 'QueueLen'), header=TRUE)

allW <- c('W1', 'W2', 'W3', 'W4', 'W5')
allWorkloads <- c('FACEBOOK_KEY_VALUE', 'GOOGLE_SEARCH_RPC',
    'FABRICATED_HEAVY_MIDDLE', 'GOOGLE_ALL_RPC','FACEBOOK_HADOOP_ALL')
workloads <- allWorkloads[match(unique(queueLen$WorkLoad), allWorkloads)]
levels(queueLen$WorkLoad) <- allW[match(levels(queueLen$WorkLoad), allWorkloads)]
queueLen$WorkLoad <- as.character(queueLen$WorkLoad)
queueLen <- queueLen[with(queueLen, order(TransportType, WorkLoad)),]
queueLen$WorkLoad <- as.factor(queueLen$WorkLoad)
Ws <- levels(queueLen$WorkLoad)

queueLen$LoadFactor <- factor(queueLen$LoadFactor)
queueLen$TransportType <- factor(queueLen$TransportType)

textSize <- 20
titleSize <- 20
#metricsToPlot = c('maxBytes', 'maxCnt', 'meanBytes', 'meanCnt',
#   'minBytes', 'minCnt', 'empty', 'onePkt')
metricsToPlot = c('maxCnt', 'meanCnt')
qLocations = c('TorDown')

for (rho in unique(queueLen$LoadFactor)) {
    i <- 0
    queuePlot = list()
    for (workload in Ws) {
        for (metric in metricsToPlot) {
            tmp <- subset(queueLen, WorkLoad==workload & LoadFactor==rho & 
                Metric %in% metricsToPlot & QueueLocation %in% qLocations &
                Metric == metric,
                select=c('LoadFactor', 'WorkLoad', 'UnschedBytes',
                'QueueLocation', 'Metric', 'QueueLen'))
            if (nrow(tmp) == 0) {
                next
            }

            i <- i+1
            plotTitle = sprintf("Workload: %s, Metric: %s", workload, metric)

            yLab = 'Queue Length (Packets)\n'
            xLab <- 'Unscheduled Bytes'

            queuePlot[[i]] <- ggplot() + geom_line(
                data=tmp, aes(x=UnschedBytes, y=QueueLen), size=2)

            queuePlot[[i]] <- queuePlot[[i]] +
                theme(text = element_text(size=1.5*textSize),
                    axis.text = element_text(size=1.3*textSize),
                    axis.text.x = element_text(angle=75, vjust=0.5),
                    strip.text = element_text(size = textSize),
                    plot.title = element_text(size = 1.2*titleSize,
                        color='darkblue'),
                    plot.margin=unit(c(2,2,2.5,2.2),"cm"),
                    legend.position = c(0.1, 0.85),
                    legend.text = element_text(size=textSize)) +
                guides(colour = guide_legend(override.aes = 
                    list(size=textSize/4)))+
                coord_cartesian(ylim=c(0, max(tmp$QueueLen)+2)) +
                labs(title = plotTitle, y = yLab, x = xLab)
        }

    }
    pdf(sprintf("plots/changeUnschedBytes/queueLength_rho%s.pdf",
        rho), width=15, height=10*length(Ws))
    args.list <- c(queuePlot, list(ncol=length(metricsToPlot)))
    do.call(grid.arrange, args.list)
    dev.off()
}

# create a table
queueLen.filtered <- subset(queueLen, select=c('WorkLoad', 'QueueLocation', 'Metric', 'QueueLen'))
queueLen.filtered <- queueLen.filtered[with(queueLen.filtered, order(WorkLoad, QueueLocation, Metric)),]
queueLen.filtered <- subset(queueLen.filtered, Metric %in% c('maxBytes', 'maxCnt', 'meanBytes', 'meanCnt'))


