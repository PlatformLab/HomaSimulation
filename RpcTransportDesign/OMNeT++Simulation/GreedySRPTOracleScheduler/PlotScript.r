#!/home/behnamm/local/bin/Rscript
library(reshape2)
library(ggplot2)
library(gridExtra)
library(plyr)
library(optparse)

# set command line argument types for option parser
option_list = list(
    make_option(
        c("-i", "--infile"),
        type="character",
        default="FABRICATED_HEAVY_MIDDLE__81.38",
        help="Result file out of GreedySRPTOracleScheduler simulator",
        metavar="/path/to/file/filename"),
    make_option(
        c("-o", "--outpath"),
        type="character",
        default="./plots",
        help="Directory path to store plot files",
        metavar="/path/to/store/plots"));
opt_parser = OptionParser(option_list=option_list);
opt = parse_args(opt_parser);


if (opt$infile == option_list[[1]]@default) {
    print(sprintf("Using default value for --infile arg: %s", opt$infile));
}
if (opt$outpath == option_list[[2]]@default) {
    print(sprintf("Using default value for --outpath arg: %s", opt$outpath));
}

# Read simulation results from file, if a row is malformed, use NA for it
simResultRaw <- read.table(opt$infile, col.names=c("mesgSize", "mesgSizeOnWire", "sizeHistBin",
    "creationTime", "completionTime", "stretch", "mesgId", "senderId", "senderIP", "recvrId", "recvrIP"),
    skip=1, fill=TRUE)

#remove NA rows
na_rows <- complete.cases(simResultRaw)
simResult <- simResultRaw[na_rows,]

#simResult <- simResult[order(simResult$sizeHistBin, simResult$stretch),]
asNumeric <- function(simResult, colNames)
{
    # For the columns that are recorded as factor or integer, we need converti
    # them to numeric otherwise we may encounter errors later.
    for (colName in colNames) {
        if (is.factor(simResult[[colName]])) {
            simResult[[colName]] <- as.numeric(levels(simResult[[colName]]))[simResult[[colName]]]
        } else {
            simResult[[colName]] <- as.numeric(simResult[[colName]])
        }
    }
    return(simResult)
}
simResult <- asNumeric(simResult, c("mesgSize", "mesgSizeOnWire", "sizeHistBin", "creationTime", "completionTime", "stretch", "mesgId", "senderId", "recvrId"))

# calculate size distribution statistics
sizeStats <- aggregate(mesgSizeOnWire~sizeHistBin, simResult, sum)
names(sizeStats)[2] <- 'sumBytesOnWire'
cnts <- aggregate(mesgSizeOnWire~sizeHistBin, simResult, length)
names(cnts)[2]<-'counts'
sizeStats <- merge(sizeStats, cnts)
sizeStats$cntFrac <- sizeStats$counts / sum(sizeStats$counts)
sizeStats$cumCntFrac <- cumsum(sizeStats$cntFrac)
sizeStats$bytesFrac <- sizeStats$cntFrac * sizeStats$sumBytesOnWire
sizeStats$bytesFrac <- sizeStats$bytesFrac / sum(sizeStats$bytesFrac)
sizeStats$cumBytesFrac <- cumsum(sizeStats$bytesFrac)

# Find stretch stats
stretchStats <- aggregate(stretch~sizeHistBin, simResult, quantile, c(0,0.5,.99,1))
tmp <- data.frame(stretchStats$stretch)
stretchStats <- data.frame(stretchStats$sizeHistBin, tmp)
names(stretchStats) <- c('sizeHistBin', 'min', 'median','NinetyNinePct', 'max')
stretchMean <- aggregate(stretch~sizeHistBin, simResult, mean)
names(stretchMean)[2] <- 'mean'
stretchStats <- merge(stretchStats, stretchMean)

# Size and stretch stats combines
stretch <- merge(sizeStats, stretchStats)
stretch <- stretch[order(stretch$sizeHistBin),]

# Save the data in a file
fileName <- basename(opt$infile)
fileConn<-file(sprintf("%s/stretchVsTransport_%s.txt", opt$outpath,
    fileName))
lineContent <- c('TransportType      LoadFactor      WorkLoad        MsgSizeRange        SizeCntPercent      BytesPercent        UnschedBytes        MeanStretch     MedianStretch       99PercentStretch')

matched <- regexpr("[A-Za-z]+.+[A-Za-z]+", fileName)
wl <- substr(fileName, matched[1], attr(matched, "match.length"))
matched<-regexpr("[0-9]+\\.[0-9]+", fileName)
lf <- substr(fileName, matched[1],
    attr(matched, "match.length") + matched[1])
lf <- as.double(lf)

for (i in 1:nrow(stretch)) {
    stretchLine = stretch[i,]
    tt <- 'GreedySRPTOracle'
    msr <- stretchLine$sizeHistBin
    scp <- 100*stretchLine$cntFrac
    bp <- 100*stretchLine$bytesFrac
    ub <- 10000
    means <-stretchLine$mean
    meds <-stretchLine$median
    nnpct <-stretchLine$NinetyNinePct
    lineMean <- sprintf("%s     %f      %s     %d      %f     %f      %d     %f      NA     NA", tt, lf, wl, msr, scp, bp, ub, means)
    lineMedian <- sprintf("%s     %f      %s     %d      %f     %f      %d     NA      %f     NA", tt, lf, wl, msr, scp, bp, ub, meds)
    line99 <- sprintf("%s     %f      %s     %d      %f     %f      %d     NA      NA     %f", tt, lf, wl, msr, scp, bp, ub, nnpct)
    lineContent <- c(lineContent, lineMean, lineMedian, line99)
}
writeLines(lineContent, fileConn)
close(fileConn)


# Plot Stretch
textSize <- 35
titleSize <- 30
yLimit <- 25
plotList <- list()
i <- 0
for (statName in names(stretchStats)[-1]) {
    i <- i+1
    txtYlim <- sprintf("pmin(%d/2, %s/2)",yLimit, statName)
    txtLabel <- sprintf("paste(sizeHistBin, \":\", format(%s, digits=3))", statName)
    plotTitle = sprintf("%s VS. Cummulative Mesg Size Fraction", statName)

    plotList[[i]] <- ggplot(stretch, aes_string(x="cumCntFrac-cntFrac/2", y= statName, width="cntFrac")) +
        geom_bar(stat="identity", position="identity", fill="white", color="darkgreen") +
        geom_text(data=stretch, aes_string(x="cumCntFrac", y=txtYlim, label=txtLabel, angle="90", size="20")) +
        theme(text = element_text(size=textSize, face="bold"), axis.text.x = element_text(angle=75, vjust=0.5),
            strip.text.x = element_text(size = textSize), strip.text.y = element_text(size = textSize),
            plot.title = element_text(size = titleSize)) +
        scale_x_continuous(breaks = stretch$cumCntFrac, labels=as.character(round(stretch$cumCntFrac,2))) +

        coord_cartesian(ylim=c(0, min(yLimit, max(stretch[[statName]], na.rm=TRUE)))) +
        labs(title = plotTitle, x = "Cumulative Mesg Size Fraction", y = statName)

    i <- i+1
    plotTitle = sprintf("%s VS. Cummulative Mesg Bytes Fraction", statName)
    plotList[[i]] <- ggplot(stretch, aes_string(x="cumBytesFrac-bytesFrac/2", y= statName, width="bytesFrac")) +
        geom_bar(stat="identity", position="identity", fill="white", color="darkgreen") +
        geom_text(data=stretch, aes_string(x="cumBytesFrac", y=txtYlim, label=txtLabel, angle="90", size="20"))+
        theme(text = element_text(size=textSize, face="bold"), axis.text.x = element_text(angle=75, vjust=0.5),
            strip.text.x = element_text(size = textSize), strip.text.y = element_text(size = textSize),
            plot.title = element_text(size = titleSize)) +
        scale_x_continuous(breaks = stretch$cumBytesFrac, labels=as.character(round(stretch$cumBytesFrac,2))) +
        coord_cartesian(ylim=c(0, min(yLimit, max(stretch[[statName]], na.rm=TRUE)))) +
        labs(title = plotTitle, x = "Cumulative Mesg Bytes Fraction", y = statName)
}

pdf(sprintf("%s/Stretch_%s.pdf", opt$outpath, fileName), width=40*2, height=15*i/2)
args.list <- c(plotList, list(ncol=2))
do.call(grid.arrange, args.list)
dev.off()
