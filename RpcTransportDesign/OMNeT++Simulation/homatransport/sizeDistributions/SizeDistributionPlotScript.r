#!/usr/bin/Rscript

# plots the value size distribution of Facebook workload based on the paper
# "Workload Analysis of a Large-Scale Key-Value Store"

library(ggplot2)
library(gridExtra)
predefProb <- read.table("FacebookKeyValueMsgSizeDist.txt", col.names=c('msgSize', 'CDF'), skip=1)
x<-10^(seq(log10(tail(predefProb$msgSize, 1) + 1), log10(1000000), length.out=100))
sigma <- 214.476
k <- 0.348238
y0 <- 0.47
x0 <- 15
y<-(1-(1+k*(x-x0)/sigma)^(-1/k))*(1-y0)+y0
value <- c(predefProb$msgSize,x)
cumProb <- c(predefProb$CDF, y)
prob = cumProb-c(0,cumProb[1:length(cumProb)-1])
byteFrac = prob*value
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
cdfFrame <- data.frame(MessageSize=value, CDF=cumProb, CBF=cumBytes,
        WorkLoad="Facebook KeyValue-Servers")

predefProb <- read.table("DCTCP_MsgSizeDist.txt", col.names=c('MessageSize', 'CDF'), skip=1) 
predefProb$MessageSize <-  predefProb$MessageSize*1500
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Web Search"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * predefProb$MessageSize
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)

predefProb <- read.table("Facebook_CacheFollowerDist_IntraCluster.txt",
        col.names=c('MessageSize', 'CDF'), skip=1) 
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Facebook Cache-Servers"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * predefProb$MessageSize
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)

predefProb <- read.table("Facebook_WebServerDist_IntraCluster.txt",
        col.names=c('MessageSize', 'CDF'), skip=1) 
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Facebook Web-Servers"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * predefProb$MessageSize
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)

predefProb <- read.table("Facebook_HadoopDist_All.txt",
        col.names=c('MessageSize', 'CDF'), skip=1) 
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Facebook Hadoop-Servers"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * predefProb$MessageSize
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)

predefProb <- read.table("Fabricated_Heavy_Middle.txt",
        col.names=c('MessageSize', 'CDF'), skip=1) 
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Fabricated Heavy Middle"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * predefProb$MessageSize
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)


cdfFrame$WorkLoad <- factor(cdfFrame$WorkLoad)
RTT <- 7.75e-6 #seconds
C <- 1e10 #Gb/s

cdfCbf = list()
cdfCbf[[1]] <- ggplot(cdfFrame, aes(x=MessageSize,y=CDF)) +
        geom_line(aes(color=WorkLoad), size = 2, alpha = 1) +
        labs(title = "Message Size Distribution Used In Simulation") +
        theme(axis.text=element_text(size=30),
            axis.title=element_text(size=30, face="bold"),
            panel.grid.major = 
                element_line(size = 0.75, linetype = 'solid', colour = "black"),
            panel.grid.minor = 
                element_line(size = 0.5, linetype = 'solid', colour = "gray"),
            legend.text=element_text(size=30),
            legend.title=element_text(size=20, face="bold"),
            legend.position="top",
        plot.title = element_text(size=20, face="bold")) +
        scale_x_log10("Message Sizes (Bytes)")+
        scale_y_continuous(breaks = round(seq(min(cdfFrame$CDF), max(cdfFrame$CDF), by = 0.1),1))

cdfCbf[[2]] <- ggplot(cdfFrame, aes(x=MessageSize,y=CBF)) +
        geom_line(aes(color=WorkLoad), size = 2, alpha = 1) +
        labs(title = "Message Byte Distribution Used In Simulation") +
        theme(axis.text=element_text(size=30),
            axis.title=element_text(size=30, face="bold"),
            panel.grid.major = 
                element_line(size = 0.75, linetype = 'solid', colour = "black"),
            panel.grid.minor = 
                element_line(size = 0.5, linetype = 'solid', colour = "gray"),
            legend.text=element_text(size=30),
            legend.title=element_text(size=20, face="bold"),
            legend.position="top",
        plot.title = element_text(size=20, face="bold")) +
        scale_x_log10("Message Sizes (Bytes)")+
        scale_y_continuous(breaks = round(seq(min(cdfFrame$CBF), max(cdfFrame$CBF), by = 0.1),1))

cdfCbf[[3]] <- ggplot(cdfFrame, aes(x=MessageSize, y=pmin(C*RTT*(1-CBF)/8, MessageSize))) +
        geom_line(aes(color=WorkLoad), size = 2, alpha = 1) +
        labs(title = "Upper bound for unsched bytes") +
        theme(axis.text=element_text(size=30),
            axis.title=element_text(size=30, face="bold"),
            panel.grid.major =
                element_line(size = 0.75, linetype = 'solid', colour = "black"),
            panel.grid.minor =
                element_line(size = 0.5, linetype = 'solid', colour = "gray"),
            legend.text=element_text(size=30),
            legend.title=element_text(size=20, face="bold"),
            legend.position="top",
        plot.title = element_text(size=20, face="bold")) +
        scale_x_log10("Message Sizes (Bytes)")+
        scale_y_continuous(breaks = seq(0, C*RTT/8, by = 0.1*C*RTT/8))

png(file='WorkloadCDFsCBFs.png', width=2400, height=3000)
do.call(grid.arrange, cdfCbf)
dev.off()
