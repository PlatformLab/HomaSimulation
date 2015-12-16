#!/usr/bin/Rscript

# This scripts fabricates a cdf from a
# "Piecewise LogLinear Cumulative Byte Function" of a workload.
# all you need to do is to specify sizeIntervals and cbf (cumulative byte
# function) values in below. The assumption is that cbf is piecewise linear
# function of log10(sizeIntervals) with breakpoints defined in sizeIntervals
# and cbf vectors.

# monotonically increasing vector of msg sizes
sizeIntervals <- c(50, 1400, 10000, 70000, 400000) 

# monotonically increaing cbf (cumulative byte function) vector. First element
# is always 0 and last element is always one. Each cbf value in this vector
# corresponds to a size in the sizeIntervals vector.
cbf <- c(0, 0.1, 0.85, 0.95, 1)

multipLiers <- (cbf[2:length(cbf)] - cbf[1:length(cbf)-1]) *
    (1/sizeIntervals[1:length(cbf)-1] - 1/sizeIntervals[2:length(cbf)]) /
    (log10(sizeIntervals[2:length(cbf)]) - log10(sizeIntervals[1:length(cbf)-1]))
avgSize <- log(10)/sum(multipLiers)
cdfRanges <- cumsum(multipLiers)/sum(multipLiers)
cdfConsts <- c(0, cdfRanges[1:length(cdfRanges)-1])


#create cdf sample vector and find the corresponding sizes.
#cdf sample vector must be sorted: cdf <- seq(0.001, 1, 0.001)
cdf <- c()
cdfBounds <- c(0, cdfRanges)
for (ind in seq(1, length(cdfRanges)))
{
    cdf <- c(cdf, seq(cdfBounds[ind], cdfBounds[ind+1], (cdfBounds[ind+1] - cdfBounds[ind])/50))
}
cdf <- sort(unique(cdf))


#Corresponding sizes for cdf values
sizes <- c()
for (cdfVal in cdf)
{
    ind <- match(1, findInterval(cdfRanges, cdfVal))
    invMsgSize <- (1 / sizeIntervals[ind]) - 
        ((cdfVal - cdfConsts[ind]) * log(10) *
        (log10(sizeIntervals[ind+1]) - log10(sizeIntervals[ind])) /
        (cbf[ind+1]-cbf[ind]) / avgSize)
    sizes <- c(sizes, 1/invMsgSize) 
}

# round the sizes to integers and remove possible duplicates that rounding might
# have caused.
sizes <- round(sizes)
unik <- !duplicated(sizes) #logical vector of unique values
ind <- seq_along(sizes)[unik] #indices of unique values
cdf <- cdf[ind]
sizes <- sizes[ind]
pdf <- cdf - c(0, cdf[1:length(cdf)-1])
avgSize <- sum(pdf*sizes)

filename = "Fabricated_Heavy_Head.txt"
write(avgSize, file=filename)
df <- data.frame(sizes, cdf)
write.table(df, file=filename, row.names=FALSE, col.names=FALSE, append=TRUE)
