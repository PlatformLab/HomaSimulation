#!/home/behnamm/local/bin/Rscript

# This scripts accepts command line arguments as below. All the command line
# arguments are mandatory
#
# ./PlotToolbox.r [PLOT_TYPE] [PLOT_TITLE] [FILE_PATH] [FILE_NAME_1] [LOAD_FACTOR_1] ... 
#           [FILE_NAME_N] [LOAD_FACTOR_N]
# The input file format are csv files that scavetool generates.

library(reshape2)
library(ggplot2)
library(gridExtra)
library(plyr)


args <- commandArgs(trailingOnly = TRUE)
print(args)

if (length(args) < 3) {
    print("Not enough arguments provided")
    quit()
}

plotType <- args[1]
plotTitle <- args[2]
filePath <- args[3]

rawDataList <- list() 
rawDataListOrdered <- list()
for (i in seq(4, length(args), 2)) {
    fileName <- paste(filePath, '/', args[i], sep="")
    loadFactor <- as.numeric(args[i+1])

    # Some of the lines in the file start with 'time'. We specify comment.char
    # as letter 't' to filter out those lines.
    rawDataList[[length(rawDataList) + 1]] <- 
        read.table(fileName, comment.char='t', sep=",", colClasses='numeric',
            col.names = c('time', 'value'))

    rawDataList[[length(rawDataList)]]$loadFactor <- 
            rep(loadFactor, length(rawDataList[[length(rawDataList)]]$value))

    rawDataList[[length(rawDataList)]]$loadFactor <- 
            factor(rawDataList[[length(rawDataList)]]$loadFactor)
}




if (identical(plotType, "cdf")) {
    print("plot CDF")
    cdfDataFrame <- data.frame()
    cdfDataFrame <- 
            ldply(rawDataList, function(rawDataFrame) {
                    valueSortedUnique <- sort(unique(rawDataFrame$value))
                    cdf <- ecdf(rawDataFrame$value)
                    dummyDataFrame <- 
                            data.frame(value=valueSortedUnique,
                            CDF=cdf(valueSortedUnique))
                    dummyDataFrame$loadFactor <-
                            factor(rawDataFrame$loadFactor[1])
                    return(dummyDataFrame)
                }

            )
    
    summary(cdfDataFrame)
    cdfPlot <- ggplot(cdfDataFrame, aes(x=value, y=CDF))
    pdf(paste(plotTitle,".","pdf",sep=""), width=20, height=20)

    print(cdfPlot + facet_wrap(~loadFactor, nrow=3)+
            geom_line(aes(color=loadFactor), size = 2, alpha = 1/2) +
            labs(title = plotTitle)) +
            theme(axis.text=element_text(size=16),
            axis.title=element_text(size=16,face="bold"))

    dev.off()
}

