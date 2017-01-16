#!/usr/bin/Rscript
library(reshape2)
library(ggplot2)
library(gridExtra)
library(plyr)

data <- read.table("prioUsage.txt", na.strings = "NA",
    col.names=c('prioLevels', 'adaptivePrioLevels', 'workload', 'redundancyFactor',
    'loadFactor', 'nominalLoadFactor', 'priority', 'prioUsagePct_activelyRecv',
    'prioUsagePct_activelyRecvSched'), header=TRUE)
data$redundancyFactor <- factor(data$redundancyFactor)
nominalLoads <- c(50, 60, 70, 80, 90)
workloads <- c('GOOGLE_SEARCH_RPC', 'FABRICATED_HEAVY_MIDDLE', 'GOOGLE_ALL_RPC','FACEBOOK_HADOOP_ALL')
prioUsagePct <- subset(data, nominalLoadFactor %in% nominalLoads)
i <- 0
plotList <- list()
textSize = 10 
yLab = 'Scheduled Priority Time Usage %'
xLab = 'Priority Level'
for (wl in workloads) {
    for (nominalLoad in sort(unique(prioUsagePct$nominalLoadFactor))) {
        i <- i+1
        plotTitle = sprintf("Workload: %s, LoadFactor= %f", wl, nominalLoad)
        tmp = subset(prioUsagePct, workload==wl & nominalLoadFactor==nominalLoad)
        plotList[[i]] <- ggplot(tmp, aes(x=priority, y=prioUsagePct_activelyRecv)) +
            geom_bar(stat="identity") +
            theme(text = element_text(size=textSize, face="bold"), legend.position="top",
                strip.text.x = element_text(size = textSize), strip.text.y = element_text(size = textSize),
                plot.title = element_text(size = textSize)) +
            coord_cartesian(ylim=c(0,100)) +
            labs(title = plotTitle, x = xLab, y = yLab)
    }
}

pdf("plots/prioUsage.pdf", width=23, height=15)
args.list <- c(plotList, list(ncol=length(nominalLoads)))
do.call(grid.arrange, args.list)
dev.off()
