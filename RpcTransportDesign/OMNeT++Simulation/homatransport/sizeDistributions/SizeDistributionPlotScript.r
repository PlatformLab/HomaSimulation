#!/usr/bin/Rscript

# plots the value size distribution of Facebook workload based on the paper
# "Workload Analysis of a Large-Scale Key-Value Store"

library(ggplot2)
library(gridExtra)
RTT <- 7.75e-6 #seconds
C <- 1e10 #Gb/s



ETHERNET_PREAMBLE_SIZE <- 8
ETHERNET_HDR_SIZE <- 14
MAX_ETHERNET_PAYLOAD_BYTES <- 1500
MIN_ETHERNET_PAYLOAD_BYTES <- 46
IP_HEADER_SIZE <- 20
UDP_HEADER_SIZE <- 8
ETHERNET_CRC_SIZE <- 4
INTER_PKT_GAP <- 12
HOMA_HDR_SIZE <- 16
 
msgSizeOnWire <- function(msgDataBytes)
{
    bytesOnWire <- 0
    maxDataInHomaPkt <- MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE
            - UDP_HEADER_SIZE - HOMA_HDR_SIZE

    numFullPkts <- msgDataBytes %/%  maxDataInHomaPkt
    bytesOnWire <- bytesOnWire + numFullPkts * (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE
            + ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP)
    numPartialBytes <- msgDataBytes - numFullPkts * maxDataInHomaPkt

    ## if all msgDataBytes fit in full pkts, we should return at this point
    if ((numFullPkts > 0) & (numPartialBytes == 0)) {
        return (bytesOnWire)
    }
    numPartialBytes <- numPartialBytes +  HOMA_HDR_SIZE + IP_HEADER_SIZE +
            UDP_HEADER_SIZE

    if (numPartialBytes < MIN_ETHERNET_PAYLOAD_BYTES) {
        numPartialBytes <- MIN_ETHERNET_PAYLOAD_BYTES
    }
    numPartialBytesOnWire <- numPartialBytes + ETHERNET_HDR_SIZE +
            ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP

    bytesOnWire <- bytesOnWire + numPartialBytesOnWire
    return (bytesOnWire)
}

maxUnsched <- RTT*C/8
unschedVec <- seq(0, 2*maxUnsched, maxUnsched/1000)

getUnschedFrac <- function(prob, msgSizes)
{
    unschedFrac <- c()
    for (unsched in unschedVec) {
        unschedFrac <- c(unschedFrac, sum(prob*sapply(pmin(unsched, msgSizes), msgSizeOnWire)))
    }
    unschedFrac <- unschedFrac / sum(prob*sapply(msgSizes, msgSizeOnWire))
    return(unschedFrac)
}
unschedFracFrame <- data.frame(Unsched=c(), CumUnschedFrac=c(), Workload=c())

predefProb <- read.table("FacebookKeyValueMsgSizeDist.txt", col.names=c('msgSize', 'CDF'), skip=1)
x<-10^(seq(log10(tail(predefProb$msgSize, 1) + 1), log10(100000), length.out=300))
sigma <- 214.476
k <- 0.348238
y0 <- 0.47
x0 <- 15
y<-(1-(1+k*(x-x0)/sigma)^(-1/k))*(1-y0)+y0
value <- c(predefProb$msgSize,x)
cumProb <- c(predefProb$CDF, y)
prob = cumProb-c(0,cumProb[1:length(cumProb)-1])
byteFrac = prob*sapply(value, msgSizeOnWire)
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
cdfFrame <- data.frame(MessageSize=value, CDF=cumProb, CBF=cumBytes,
        WorkLoad="Facebook KeyValue-Servers")
unschedFracFrame <- rbind(unschedFracFrame,
    data.frame(Unsched=unschedVec, CumUnschedFrac=getUnschedFrac(prob, value),
    Workload="Facebook KeyValue-Servers"))


predefProb <- read.table("DCTCP_MsgSizeDist.txt", col.names=c('MessageSize', 'CDF'), skip=1)
predefProb$MessageSize <-  predefProb$MessageSize*1500
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Web Search"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * sapply(predefProb$MessageSize, msgSizeOnWire)
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)
unschedFracFrame <- rbind(unschedFracFrame,
    data.frame(Unsched=unschedVec, CumUnschedFrac=getUnschedFrac(prob, predefProb$MessageSize),
    Workload="Web Search"))


predefProb <- read.table("Facebook_CacheFollowerDist_IntraCluster.txt",
        col.names=c('MessageSize', 'CDF'), skip=1)
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Facebook Cache-Servers"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * sapply(predefProb$MessageSize, msgSizeOnWire)
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)
unschedFracFrame <- rbind(unschedFracFrame,
    data.frame(Unsched=unschedVec, CumUnschedFrac=getUnschedFrac(prob, predefProb$MessageSize),
    Workload="Facebook Cache-Servers"))


predefProb <- read.table("Facebook_WebServerDist_IntraCluster.txt",
        col.names=c('MessageSize', 'CDF'), skip=1)
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Facebook Web-Servers"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * sapply(predefProb$MessageSize, msgSizeOnWire)
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)
unschedFracFrame <- rbind(unschedFracFrame,
    data.frame(Unsched=unschedVec, CumUnschedFrac=getUnschedFrac(prob, predefProb$MessageSize),
    Workload="Facebook Web-Servers"))


predefProb <- read.table("Facebook_HadoopDist_All.txt",
        col.names=c('MessageSize', 'CDF'), skip=1)
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Facebook Hadoop-Servers"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * sapply(predefProb$MessageSize, msgSizeOnWire)
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)
unschedFracFrame <- rbind(unschedFracFrame,
    data.frame(Unsched=unschedVec, CumUnschedFrac=getUnschedFrac(prob, predefProb$MessageSize),
    Workload="Facebook Hadoop-Servers"))


predefProb <- read.table("Fabricated_Heavy_Middle.txt",
        col.names=c('MessageSize', 'CDF'), skip=1)
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Fabricated Heavy Middle"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * sapply(predefProb$MessageSize, msgSizeOnWire)
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)
unschedFracFrame <- rbind(unschedFracFrame,
    data.frame(Unsched=unschedVec, CumUnschedFrac=getUnschedFrac(prob, predefProb$MessageSize),
    Workload="Fabricated Heavy Middle"))

predefProb <- read.table("Google_SearchRPC.txt",
        col.names=c('MessageSize', 'CDF'), skip=1)
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Google Search RPC"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * sapply(predefProb$MessageSize, msgSizeOnWire)
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)
unschedFracFrame <- rbind(unschedFracFrame,
    data.frame(Unsched=unschedVec, CumUnschedFrac=getUnschedFrac(prob, predefProb$MessageSize),
    Workload="Google Search RPC"))

predefProb <- read.table("Google_AllRPC.txt",
        col.names=c('MessageSize', 'CDF'), skip=1)
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Google All RPC"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * sapply(predefProb$MessageSize, msgSizeOnWire)
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)
unschedFracFrame <- rbind(unschedFracFrame,
    data.frame(Unsched=unschedVec, CumUnschedFrac=getUnschedFrac(prob, predefProb$MessageSize),
    Workload="Google All RPC"))


predefProb <- read.table("Fabricated_Heavy_Head.txt",
        col.names=c('MessageSize', 'CDF'), skip=1)
predefProb <- rbind(data.frame(MessageSize=c(0,predefProb$MessageSize[1]-1), CDF=c(0,0)), predefProb)
predefProb$WorkLoad <- "Fabricated Heavy Head"
prob = predefProb$CDF-c(0,predefProb$CDF[1:length(predefProb$CDF)-1])
byteFrac = prob * sapply(predefProb$MessageSize, msgSizeOnWire)
cumBytes <- cumsum(byteFrac)/sum(byteFrac)
predefProb$CBF <- cumBytes
cdfFrame <- rbind(predefProb, cdfFrame)
unschedFracFrame <- rbind(unschedFracFrame,
    data.frame(Unsched=unschedVec, CumUnschedFrac=getUnschedFrac(prob, predefProb$MessageSize),
    Workload="Fabricated Heavy Head"))

cdfFrame$WorkLoad <- factor(cdfFrame$WorkLoad)
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

cdfCbf[[4]] <- ggplot(unschedFracFrame, aes(x=Unsched, y=CumUnschedFrac)) +
        geom_line(aes(color=Workload), size = 2, alpha = 1) +
        labs(title = "Fraction of transmitted bytes in unsched. pkts (normalized by the max)") +
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
        scale_x_log10("Unsched. Limit (Bytes)")+
        scale_y_continuous(breaks = seq(0, 1, by = 0.1))

png(file='WorkloadCDFsCBFs.png', width=3000, height=5000)
args.list <- list()
args.list <- c(cdfCbf, list(ncol=1))
do.call(grid.arrange, args.list)
dev.off()
