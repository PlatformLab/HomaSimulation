#!/usr/bin/Rscript

library(reshape2)
library(ggplot2)
library(gridExtra)
penaltyPerRho <- read.table("output/ideal_vs_simple_penalty", col.names=c('penalty', 'rho', 'avg_delay'), header=TRUE)
penaltyPerRho$avg_delay <- factor(penaltyPerRho$avg_delay)
plotPenaltyPerRho <- ggplot(penaltyPerRho, aes(x = rho, y = penalty))
pdf('plots/PenaltyPerRho.pdf')
plotPenaltyPerRho + geom_line(color = "red", size = 3, alpha = 1/2) + 
    labs(title = expression(frac(1,N)*Sigma*frac(CompletionTime[simple]-CompletionTime[ideal], CompletionTime[ideal]) * "   VS.   Average Load"))
dev.off()

cumDist <- read.table("input/SizeDistribution", col.names=c('CDF','message_size'))
plotDist <- ggplot(cumDist, aes(x = message_size, y = CDF))
pdf('plots/DistributionPlot.pdf')
plotDist + geom_bar(stat = "identity", color = "red", size = 1, alpha = 1/2) + labs(title = "Cumulative Distribution of Message Size")
dev.off()

penaltyPerSize <- read.table("output/ideal_vs_simple_rho_fixed", col.names=c('penalty', 'size', 'rho'), header=TRUE)
penaltyPerSize$rho = factor(penaltyPerSize$rho)
plotPenaltyPerSize <- ggplot(penaltyPerSize, aes(x = size, y = penalty))
pdf("plots/PenaltyPerSize.pdf")
plotPenaltyPerSize + geom_line(aes(color = rho), size = 3, alpha = 1/2) + 
        labs(title = expression(frac(1,N)*Sigma*frac(CompletionTime[simple]-CompletionTime[ideal], CompletionTime[ideal]) * "   VS.  Message Sizes"))
dev.off()

#plot dupplicate of penaltyPerRho and penaltyPerSize graphs based on the data
#from delay_break_down output files. This is just a sanity check that verifies
#we generate same plot from two different data
delays <- read.table("output/delays_break_down", col.names=c('rho', 'scheduler', 'size', 'totalDelay','serializationDelay', 'schedulingDelay', 'networkDelay','rxQueueDelay'), header=TRUE)
delaySubset <- subset(delays, TRUE, c('rho', 'scheduler', 'size', 'totalDelay'))
delaySubset$rho <- factor(delaySubset$rho)
delaySubset$scheduler <- factor(delaySubset$scheduler)

totalAvgPenalty <- c() 
rhos <- c()
msgSize <- c()
penaltyPerSize <- c()
for (rho_ in levels(delaySubset$rho)) {
    delayList <- list()
    for (scheduler_ in levels(delaySubset$scheduler)){
        delayList[[scheduler_]] <- subset(delaySubset, rho == rho_ & scheduler == scheduler_, c('size', 'totalDelay'))
    }
    delayPenalty <- c()
    delayPenalty <- ((delayList$simple$totalDelay - delayList$ideal$totalDelay) / delayList$ideal$totalDelay)
    totalAvgPenalty <- c(totalAvgPenalty, sum(delayPenalty)/length(delayPenalty))
    sizes <- delayList$ideal$size
    delayPenalty <- data.frame(sizes, delayPenalty)
    delayPenalty$sizes <- factor(delayPenalty$sizes)
    for (size in levels(delayPenalty$sizes)) {
        rhos <- c(rhos, rho_) 
        msgSize <- c(msgSize, size)
        delayPenaltySub <- subset(delayPenalty, sizes==size, c('delayPenalty'))$delayPenalty
        penaltyPerSize <- c(penaltyPerSize, sum(delayPenaltySub)/length(delayPenaltySub))
    }
}
penaltyPerSize <- data.frame(penalty=penaltyPerSize, size=as.numeric(msgSize), rho=rhos)
penaltyPerSize$rho <- factor(penaltyPerSize$rho)
penaltyPerRho <- data.frame(rho=as.numeric(levels(delaySubset$rho)), penalty=totalAvgPenalty)
plotPenaltyPerRho <- ggplot(penaltyPerRho, aes(x = rho, y = penalty))

pdf('plots/PenaltyPerRho_Dupplicate.pdf')
plotPenaltyPerRho + geom_line(color = "red", size = 3, alpha = 1/2) + 
    labs(title = expression(frac(1,N)*Sigma*frac(CompletionTime[simple]-CompletionTime[ideal], CompletionTime[ideal]) * "   VS.   Average Load (Dupplicate Plot)"))
dev.off()

plotPenaltyPerSize <- ggplot(penaltyPerSize, aes(x = size, y = penalty))
pdf("plots/PenaltyPerSize_Dupplicate.pdf")
plotPenaltyPerSize + geom_line(aes(color = rho), size = 3, alpha = 1/2) + 
        labs(title = expression(frac(1,N)*Sigma*frac(CompletionTime[simple]-CompletionTime[ideal], CompletionTime[ideal]) * "   VS.  Message Sizes (Dupplicate Plot)"))
dev.off()

#plot delay breakdown distribution
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
plotDelayAvg + facet_wrap(~rhos) + geom_bar(stat="identity") +
        labs(title = "Average Delay Break Down Over All Message Sizes For Various rho Values")
dev.off()

##########################
#rho = levels(delays$rho)[ceiling(length(levels(delays$rho))/2)] #for the center value of rho
rho = levels(delays$rho)[length(levels(delays$rho))] #for the max value of rho
sizes = c()
schedulers = c()
serialDel = c()
schedulingDel = c()  
networkDel = c()
rxqueueDel = c()
for (size in c(1,2,3,4)){
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
pdf('plots/average_delay_breakdown_rho_fixed_small_sizes.pdf')
plotDelayAvg + facet_wrap(~sizes) + geom_bar(stat="identity") + 
        labs(title = sprintf("Average Delay Break For rho=%s And Various Message Sizes",rho))
dev.off()

##########################
sizes = c()
schedulers = c()
serialDel = c()
schedulingDel = c()  
networkDel = c()
rxqueueDel = c()
for (size in c(7,8,9,10)){
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
pdf('plots/average_delay_breakdown_rho_fixed_large_sizes.pdf')
plotDelayAvg + facet_wrap(~sizes) + geom_bar(stat="identity") + 
        labs(title = sprintf("Average Delay Break For rho=%s And Various Message Sizes",rho))
dev.off()

#plot completion time distribution for different message sizes and rho values
compTimeDist <- read.table("output/completion_time_distribution", col.names=c('rho','scheduler','msgSize','compTime','count'), header=TRUE)
compTimeDist$rho <- factor(compTimeDist$rho)
compTimeDist$scheduler <- factor(compTimeDist$scheduler)
compTimeDist$msgSize <- factor(compTimeDist$msgSize)

compTimeDist.filtered <- list()
plotDist <- list()
rhos <- c(0.15, 0.55, .95)
msgSize <- c(1, 2, 5, 9, 10)
i <- 0 
for (rho in rhos){
    scheduler <- c()
    compTime <- c()
    count <- c()
    messageSize <- c()
    plotList <- list()
    for (sizeMsg in msgSize) {
        i <- i+1
        scheduler<-c(scheduler, as.vector(compTimeDist$scheduler[compTimeDist$rho == rho & compTimeDist$msgSize == sizeMsg]))
        compTime<-c(compTime, compTimeDist$compTime[compTimeDist$rho == rho & compTimeDist$msgSize == sizeMsg])
        count<-c(count, compTimeDist$count[compTimeDist$rho == rho & compTimeDist$msgSize == sizeMsg])
#        messageSize<-c(messageSize, compTimeDist$msgSize[compTimeDist$rho == rho & compTimeDist$msgSize == sizeMsg])

        compTimeDist.filtered[[i]] <- data.frame(scheduler, compTime, count)
        compTimeDist.filtered[[i]]$scheduler <- factor(compTimeDist.filtered[[i]]$scheduler)

        #order the dataframe by the scheduler and compTime 
        compTimeDist.filtered[[i]] <- compTimeDist.filtered[[i]][with(compTimeDist.filtered[[i]], order(scheduler, compTime)),]

        #calculate the cumulative distribution of the completion times and add it to
        #as a new column to the compTimeDist.filtered dataframe
        compTime.distribution <- tapply(compTimeDist.filtered[[i]]$count, list(compTimeDist.filtered[[i]]$scheduler), function(x) cumsum(x)/sum(x))
        tmp <- c()
        for (cumDistVec in compTime.distribution) {
            tmp <- c(tmp, cumDistVec)
        }
        compTimeDist.filtered[[i]]$compTime.distribution <- tmp

        #plot the completion time distribution
        plotDist[[i]] <- ggplot(compTimeDist.filtered[[i]], aes(x=compTime, y=compTime.distribution)) + 
                geom_step(aes(color=scheduler), size = 1, alpha = 1/2) +
                labs(title = sprintf("Distribution of completion time, rho=%.2f, msgSize=%d", rho, floor(sizeMsg))) +
                aes(color=scheduler)
        plotList[[length(plotList) + 1]] <- plotDist[[i]]
    }
    pdf(sprintf("plots/compTimeDistribution_rho%d.pdf", floor(rho*100)), width=20, height=20 )
    do.call(grid.arrange, plotList)
    dev.off()
}

#plot rxQueue size distribution for different rho values
rxQueueSize <- read.table("output/rx_queue_total_size_dist", col.names=c('rho', 'scheduler', 'queue_size', 'count'), header=TRUE)
i <- 0
rxQueueDist <- list()
plotList <- list()
for (rho_ in rhos) {
    i <- i+1
    rxQueueDist[[i]] <- subset(rxQueueSize, rho == rho_, c('scheduler', 'queue_size', 'count'))
    rxQueueDist[[i]]$scheduler <- factor(rxQueueDist[[i]]$scheduler)
    rxQueueDist[[i]] <- rxQueueDist[[i]][with(rxQueueDist[[i]], order(scheduler, queue_size)),]
    queue_size_distribution <- tapply(rxQueueDist[[i]]$count, list(rxQueueDist[[i]]$scheduler), function(x) cumsum(x)/sum(x)) 
    tmp <- c()
    for (queueDistVec in queue_size_distribution){
        tmp <- c(tmp, queueDistVec)
    }
    rxQueueDist[[i]]$queue_size_distribution <- tmp
    plotList[[i]] <- ggplot(rxQueueDist[[i]], aes(x=queue_size, y=queue_size_distribution)) + geom_step(aes(color=scheduler), size = 1, alpha = 1/2) +
            labs(title = sprintf("Distribution of rxQueue Size, rho=%.2f", rho_)) + facet_wrap(~scheduler, scales="free_x", ncol=2)
}
pdf("plots/rxQueue_size_distribution.pdf", width=20, height=20 )
do.call(grid.arrange, plotList)
dev.off()

#plot average completion times per rho value
rhos = c()
schedulers = c()
avgCompTime = c()
for (rho_ in levels(compTimeDist$rho)){
    for (schdl in levels(compTimeDist$scheduler)){
        compTime <- data.frame()
        compTime <- subset(compTimeDist, rho == rho_ & scheduler == schdl, c('compTime', 'count'))
        avgTime <- sum(compTime$compTime * compTime$count)/sum(compTime$count)
        rhos <- c(rhos, rho_)
        schedulers <- c(schedulers, schdl)
        avgCompTime <- c(avgCompTime, avgTime)
    }
}
avgCompTimeDF <- data.frame(scheduler=schedulers, rho=rhos, avgCompTime)
plotAvgCompTime <- ggplot(avgCompTimeDF, aes(x=as.numeric(rho), y=avgCompTime))
pdf("plots/avg_completion_time.pdf")
plotAvgCompTime + geom_line(aes(color = scheduler), size = 1.5, alpha = 1/2) + 
        labs(title = "Average Completion Time Over All Message Sizes")
dev.off()
