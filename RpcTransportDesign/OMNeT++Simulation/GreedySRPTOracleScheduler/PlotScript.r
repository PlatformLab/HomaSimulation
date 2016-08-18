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
        default="./FABRICATED_HEAVY_MIDDLE__77.50",
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


# Read simulation results from file
simResult <- read.table(opt$infile, col.names=c("mesgSize", "mesgSizeOnWire", "sizeHistBin",
    "creationTime", "completionTime", "stretch", "mesgId", "senderId", "senderIP", "recvrId", "recvrIP"), skip=1)

#simResult <- read.table("FACEBOOK_HADOOP_ALL__77.50", col.names=c("mesgSize", "mesgSizeOnWire", "sizeHistBin",
#    "creationTime", "completionTime", "stretch", "mesgId", "senderId", "senderIP", "recvrId", "recvrIP"), skip=1)

simResult <- simResult[order(simResult$sizeHistBin, simResult$stretch),]
simResult$mesgSizeOnWire <- as.numeric(simResult$mesgSizeOnWire)

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

# Plot Stretch
yLimit <- 25
plotList <- list()
i <- 0
for (statName in names(stretchStats)[-1]) {
    i <- i+1
    #txtYlim <- sprintf("pmin(%d/2, %s/2)",yLimit, statName) 
    plotList[[i]] <- ggplot(stretch, aes_string(x="cumCntFrac-cntFrac/2", y= statName, width="cntFrac")) + 
        geom_bar(stat="identity", position="identity", fill="white", color="darkgreen")
        #+ geom_text(data=stretch, aes_string(x="cumCntFract", y=txtYlim), angle=90, size=11)

    i <- i+1
    plotList[[i]] <- ggplot(stretch, aes_string(x="cumBytesFrac-bytesFrac/2", y= statName, width="bytesFrac")) + 
        geom_bar(stat="identity", position="identity", fill="white", color="darkgreen")
        #+ geom_text(data=stretch, aes_string(x="cumCntFract", y=txtYlim), angle=90, size=11)
}

pdf(sprintf("%s/Stretch.pdf", opt$outpath), width=20*2, height=15*i/2)
args.list <- c(plotList, list(ncol=2))
do.call(grid.arrange, args.list)
dev.off()

