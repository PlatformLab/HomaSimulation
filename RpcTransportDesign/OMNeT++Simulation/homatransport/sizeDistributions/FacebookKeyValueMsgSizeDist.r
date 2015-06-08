#!/usr/bin/Rscript

# plots the value size distribution of Facebook workload based on the paper
# "Workload Analysis of a Large-Scale Key-Value Store"

library(ggplot2)
predefProb<-c(0.00536 , 0.00583 , 0.18403 , 0.27642 , 0.27660 , 0.30400 , 0.30465 , 0.31071 , 0.31094 , 0.31931 , 0.32768 , 0.41757 , 0.41849 , 0.42175 , 0.44155)
x<-10^(seq(log10(15), log10(1000000), length.out=1000))
sigma <- 214.476
k <- 0.348238
y0 <- 0.47
x0 <- 15
y<-(1-(1+k*(x-x0)/sigma)^(-1/k))*(1-y0)+y0
value <- c(seq(0,14),x)
cumProb <- c(predefProb, y)
cdfFrame <- data.frame(value=value, CDF=cumProb)
valueCdf <- ggplot(cdfFrame, aes(x=log10(value),y=CDF))
pdf('FacebookWorkloadSizeDistribution.pdf',, width=30, height=20)

valueCdf + geom_line(color='blue', size = 2, alpha = 1/2) +
        theme(axis.text=element_text(size=30),
            axis.title=element_text(size=30, face="bold"),
            panel.grid.major = element_line(size = 0.75, linetype = 'solid',
                                colour = "black"), 
            panel.grid.minor = element_line(size = 0.5, linetype = 'solid',
                                colour = "gray")
            
             )
dev.off()

