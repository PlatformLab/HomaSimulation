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

compTimeDist <- read.table("output/completion_time_distribution", col.names=c('rho','scheduler','msgSize','compTime','count'), header=TRUE)
compTimeDist$rho <- factor(compTimeDist$rho)
compTimeDist$scheduler <- factor(compTimeDist$scheduler)
compTimeDist$msgSize <- factor(compTimeDist$msgSize)

compTimeDist.filtered = list()
rhos = c(0.15, 0.55, .95)
msgSize = c(1, 2, 5, 9, 10)
i = 1
for (rho in rhos){
    scheduler <- c()
    compTime <- c()
    count <- c()
    messageSize <- c()
    for (sizeMsg in msgSize){
        scheduler<-c(scheduler, as.vector(compTimeDist$scheduler[compTimeDist$rho == rho & compTimeDist$msgSize == sizeMsg]))
        compTime<-c(compTime, compTimeDist$compTime[compTimeDist$rho == rho & compTimeDist$msgSize == sizeMsg])
        count<-c(count, compTimeDist$count[compTimeDist$rho == rho & compTimeDist$msgSize == sizeMsg])
        messageSize<-c(messageSize, compTimeDist$msgSize[compTimeDist$rho == rho & compTimeDist$msgSize == sizeMsg])
    }
    compTimeDist.filtered[[i]] <- data.frame(scheduler, messageSize, compTime, count)
    compTimeDist.filtered[[i]]$scheduler <- factor(compTimeDist.filtered[[i]]$scheduler)
    compTimeDist.filtered[[i]]$messageSize <- factor(compTimeDist.filtered[[i]]$messageSize)

    #order the dataframe by the scheduler and compTime 
    compTimeDist.filtered[[i]] <- compTimeDist.filtered[[i]][with(compTimeDist.filtered[[i]], order(scheduler, messageSize, compTime)),]
    
    #calculate the cumulative distribution of the completion times and add it to
    #as a new column to the compTimeDist.filtered dataframe
    compTime.distribution <- tapply(compTimeDist.filtered[[i]]$count, list(compTimeDist.filtered[[i]]$messageSize, compTimeDist.filtered[[i]]$scheduler), function(x) cumsum(x)/sum(x))
    tmp <- c()
    for (cumDistVec in compTime.distribution) {
        tmp <- c(tmp, cumDistVec)
    }
    compTimeDist.filtered[[i]]$compTime.distribution <- tmp
    
    #plot the completion time distribution
    plotDist <- ggplot(compTimeDist.filtered[[i]], aes(x=compTime, y=compTime.distribution))
    pdf(sprintf("plots/compTimeDistribution_rho%d.pdf", floor(rho*100)), width=10, height=20 )
    print(plotDist + geom_step(aes(color=scheduler), size = 1, alpha = 1/2) + labs(title = sprintf("Distribution of completion time, rho=%f",rho)) + facet_grid(messageSize~scheduler, scales="free_x"))
    dev.off()

    i = i+1
}

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
