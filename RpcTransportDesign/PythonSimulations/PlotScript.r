#!/usr/bin/Rscript

library(reshape2)
library(ggplot2)
penaltyPerRho <- read.table("output/ideal_vs_simple_penalty")
penaltyPerRho$avg_delay <- factor(penaltyPerRho$avg_delay)
plotPenaltyPerRho <- ggplot(penaltyPerRho, aes(x = rho, y = penalty))
pdf('plots/PenaltyPerRho.pdf')
plotPenaltyPerRho + geom_line(color = "red", size = 3, alpha = 1/2) + labs(title = expression(frac(1,N)*Sigma*frac(CompletionTime[simple]-CompletionTime[ideal], CompletionTime[ideal]) * "   VS.   Average Load"))
dev.off()

cumDist <- read.table("input/SizeDistribution", col.names=c('CDF','message_size'))
plotDist <- ggplot(cumDist, aes(x = message_size, y = CDF))
pdf('plots/DistributionPlot.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Cumulative Distribution of Message Size")
dev.off()

penaltyPerSize <- read.table("output/ideal_vs_simple_rho_fixed", header=TRUE)
penaltyPerSize$rho = factor(penaltyPerSize$rho)
plotPenaltyPerSize <- ggplot(penaltyPerSize, aes(x = size, y = penalty))
pdf("plots/PenaltyPerSize.pdf")
plotPenaltyPerSize + geom_line(aes(color = rho), size = 3, alpha = 1/2) + labs(title = expression(frac(1,N)*Sigma*frac(CompletionTime[simple]-CompletionTime[ideal], CompletionTime[ideal]) * "   VS.  Message Sizes"))
dev.off()

compTimeDist <- read.table("output/completion_time_distribution", col.names=c('rho','scheduler','msgSize','compTime','count'))
compTimeDist$rho <- factor(compTimeDist$rho)
compTimeDist$scheduler <- factor(compTimeDist$scheduler)
compTimeDist$msgSize <- factor(compTimeDist$msgSize)
summary(compTimeDist)

compDist <- read.table("output/compTime_rho15_simple_size1", col.names=c('compTime','count'), header=TRUE)
compDist$count <- (compDist$count)/sum(compDist$count)
compDist <- compDist[with(compDist,order(compTime)),]
plotDist <- ggplot(compDist, aes(x = compTime, y = cumsum(count)))
pdf('plots/compTime_rho15_simple_size1.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of completion time, rho=0.15, simple scheduler, size=1pkt")
dev.off()

compDist <- read.table("output/compTime_rho15_ideal_size1", col.names=c('compTime','count'), header=TRUE)
compDist$count <- (compDist$count)/sum(compDist$count)
compDist <- compDist[with(compDist,order(compTime)),]
plotDist <- ggplot(compDist, aes(x = compTime, y=cumsum(count)))
pdf('plots/compTime_rho15_ideal_size1.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of completion time, rho=0.15, ideal scheduler, size=1pkt")
dev.off()

compDist <- read.table("output/compTime_rho95_simple_size1", col.names=c('compTime','count'), header=TRUE)
compDist$count <- (compDist$count)/sum(compDist$count)
compDist <- compDist[with(compDist,order(compTime)),]
plotDist <- ggplot(compDist, aes(x = compTime, y=cumsum(count)))
pdf('plots/compTime_rho95_simple_size1.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of completion time, rho=0.95, simple scheduler, size=1pkt")
dev.off()

compDist <- read.table("output/compTime_rho95_ideal_size1", col.names=c('compTime','count'), header=TRUE)
compDist$count <- (compDist$count)/sum(compDist$count)
compDist <- compDist[with(compDist,order(compTime)),]
plotDist <- ggplot(compDist, aes(x = compTime, y=cumsum(count)))
pdf('plots/compTime_rho95_ideal_size1.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of completion time, rho=0.95, ideal scheduler, size=1pkt")
dev.off()

compDist <- read.table("output/compTime_rho15_simple_size8", col.names=c('compTime','count'), header=TRUE)
compDist$count <- (compDist$count)/sum(compDist$count)
compDist <- compDist[with(compDist,order(compTime)),]
plotDist <- ggplot(compDist, aes(x = compTime, y=cumsum(count)))
pdf('plots/compTime_rho15_simple_size8.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of completion time, rho=0.15, simple scheduler, size=8pkt")
dev.off()

compDist <- read.table("output/compTime_rho15_ideal_size8", col.names=c('compTime','count'), header=TRUE)
compDist$count <- (compDist$count)/sum(compDist$count)
compDist <- compDist[with(compDist,order(compTime)),]
plotDist <- ggplot(compDist, aes(x = compTime, y=cumsum(count)))
pdf('plots/compTime_rho15_ideal_size8.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of completion time, rho=0.15, ideal scheduler, size=8pkt")
dev.off()

compDist <- read.table("output/compTime_rho95_simple_size8", col.names=c('compTime','count'), header=TRUE)
compDist$count <- (compDist$count)/sum(compDist$count)
compDist <- compDist[with(compDist,order(compTime)),]
plotDist <- ggplot(compDist, aes(x = compTime, y=cumsum(count)))
pdf('plots/compTime_rho95_simple_size8.pdf')
plotDist + geom_line(color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of completion time, rho=0.95, simple scheduler, size=8pkt")
dev.off()

compDist <- read.table("output/compTime_rho95_ideal_size8", col.names=c('compTime','count'), header=TRUE)
compDist$count <- (compDist$count)/sum(compDist$count)
compDist <- compDist[with(compDist,order(compTime)),]
plotDist <- ggplot(compDist, aes(x = compTime, y=cumsum(count)))
pdf('plots/compTime_rho95_ideal_size8.pdf')
plotDist + geom_line( color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of completion time, rho=0.95, ideal scheduler, size=8pkt")
dev.off()

rxQueueDist <- read.table("output/queueDist_rho35_simple", col.names=c('queueSize','count'), header=TRUE)
rxQueueDist$count <- (rxQueueDist$count)/sum(rxQueueDist$count)
rxQueueDist <- rxQueueDist[with(rxQueueDist,order(queueSize)),]
plotDist <- ggplot(rxQueueDist, aes(x = queueSize, y = cumsum(count)))
pdf('plots/queueDist_rho35_simple.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of rxQueue Size, rho=0.35, simple scheduler")
dev.off()

rxQueueDist <- read.table("output/queueDist_rho35_ideal", col.names=c('queueSize','count'), header=TRUE)
rxQueueDist$count <- (rxQueueDist$count)/sum(rxQueueDist$count)
rxQueueDist <- rxQueueDist[with(rxQueueDist,order(queueSize)),]
plotDist <- ggplot(rxQueueDist, aes(x = queueSize, y = cumsum(count)))
pdf('plots/queueDist_rho35_ideal.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of rxQueue Size, rho=0.35, ideal scheduler")
dev.off()

rxQueueDist <- read.table("output/queueDist_rho95_simple", col.names=c('queueSize','count'), header=TRUE)
rxQueueDist$count <- (rxQueueDist$count)/sum(rxQueueDist$count)
rxQueueDist <- rxQueueDist[with(rxQueueDist,order(queueSize)),]
plotDist <- ggplot(rxQueueDist, aes(x = queueSize, y = cumsum(count)))
pdf('plots/queueDist_rho95_simple.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of rxQueue Size, rho=0.95, simple scheduler")
dev.off()

rxQueueDist <- read.table("output/queueDist_rho95_ideal", col.names=c('queueSize','count'), header=TRUE)
rxQueueDist$count <- (rxQueueDist$count)/sum(rxQueueDist$count)
rxQueueDist <- rxQueueDist[with(rxQueueDist,order(queueSize)),]
plotDist <- ggplot(rxQueueDist, aes(x = queueSize, y = cumsum(count)))
pdf('plots/queueDist_rho95_ideal.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Distribution of rxQueue Size, rho=0.95, ideal scheduler")
dev.off()


delays <- read.table("output/delays_break_down", col.names=c('rho', 'scheduler', 'size', 'totalDelay','serializationDelay', 'schedulingDelay', 'networkDelay','rxQueueDelay'), header=TRUE)
delays$rho <- factor(delays$rho)
delays$scheduler <- factor(delays$scheduler)
rhos = c()
schedulers = c()
serialDel = c()
schedulingDel = c()  
networkDel = c()
rxqueueDel = c()
for (rho in levels(delays$rho)){
    for (scheduler in levels(delays$scheduler)){
        rhos <- c(rhos, rho)
        schedulers <- c(schedulers, scheduler)
        serialDel <- c(serialDel, ave(delays$serializationDelay[delays$rho == rho & delays$scheduler == scheduler])[1])
        schedulingDel <- c(schedulingDel, ave(delays$schedulingDelay[delays$rho == rho & delays$scheduler == scheduler])[1])
        networkDel <- c(networkDel, ave(delays$networkDelay[delays$rho == rho & delays$scheduler == scheduler])[1])
        rxqueueDel <- c(rxqueueDel, ave(delays$rxQueueDelay[delays$rho == rho & delays$scheduler == scheduler])[1])
    }
}
delaysAvg <- data.frame(rhos,schedulers, serialDel, schedulingDel, networkDel, rxqueueDel)
delaysAvg <- melt(delaysAvg, id.var=c("schedulers", "rhos"))
plotDelayAvg <- ggplot(delaysAvg, aes(x=schedulers, y=value, fill=variable))
pdf('plots/average_delay_breakdown.pdf')
plotDelayAvg + facet_wrap(~rhos) + geom_bar(stat="identity")
dev.off()

rho = levels(delays$rho)[ceiling(length(levels(delays$rho))/2)]
sizes = c()
schedulers = c()
serialDel = c()
schedulingDel = c()  
networkDel = c()
rxqueueDel = c()
for (size in c(1,2,5,8)){
    for (scheduler in levels(delays$scheduler)){
        sizes <- c(sizes, size)
        schedulers <- c(schedulers, scheduler)
        serialDel <- c(serialDel, ave(delays$serializationDelay[delays$rho == rho & delays$scheduler == scheduler & delays$size == size])[1])
        schedulingDel <- c(schedulingDel, ave(delays$schedulingDelay[delays$rho == rho & delays$scheduler == scheduler & delays$size == size])[1])
        networkDel <- c(networkDel, ave(delays$networkDelay[delays$rho == rho & delays$scheduler == scheduler & delays$size == size])[1])
        rxqueueDel <- c(rxqueueDel, ave(delays$rxQueueDelay[delays$rho == rho & delays$scheduler == scheduler & delays$size == size])[1])
    }
}
delaysAvg <- data.frame(sizes, schedulers, serialDel, schedulingDel, networkDel, rxqueueDel)
delaysAvg <- melt(delaysAvg, id.var=c("schedulers", "sizes"))
plotDelayAvg <- ggplot(delaysAvg, aes(x=schedulers, y=value, fill=variable))
pdf('plots/average_delay_breakdown_rho_fixed.pdf')
plotDelayAvg + facet_wrap(~sizes) + geom_bar(stat="identity")
dev.off()
