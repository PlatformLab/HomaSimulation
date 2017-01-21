#!/usr/bin/Rscript
library(reshape2)
library(ggplot2)
library(gridExtra)
library(plyr)

data <- read.table("homaVsBasic_Incast.txt", na.strings = "NA",
    col.names=c('Transport', 'SenderClients', 'min', 'median',
    'tail_99th', 'ReceiverBW_GBps'), header=TRUE, sep=',')
dataMelted <- melt(data, id.vars=c("Transport", "SenderClients"),
    measure.vars=c("min", "median", "tail_99th"))
names(dataMelted) <- c("Transport", "SenderClients", "Metric",
    "Latency_us")

p1 <- ggplot(dataMelted, aes(x=SenderClients, y=Latency_us)) +
        geom_line(aes(colour=Transport), size = 1.5, alpha = 2/3) +
        facet_grid(.~Metric)
pdf("plots/homaImpl/homaVsBasic_Incast.pdf", width=10, height=5)
print(p1)
dev.off()

p2 <- ggplot(data, aes(x=SenderClients, y=ReceiverBW_GBps)) +
        geom_line(aes(colour=Transport), size = 1.5, alpha = 2/3)
pdf("plots/homaImpl/homaVsBasic_Incast_BW.pdf", width=10, height=10)
print(p2)
dev.off()
