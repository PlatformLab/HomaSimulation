#!/usr/bin/Rscript

# This scripts fabricates a cdf from a
# "Piecewise LogLinear Cumulative Byte Function" of heavy middle workload. 

avgSize <- log(10)/(.05*(1/50-1/1000)/(3-log10(50)) + .15*(1/1000-1/7200)/(log10(7200)-3) + .7*(1/7200-1/(10^4.8))/(4.8-log10(7200)) + .1*(1/(10^4.8)-1/(10^5.5))/(5.5-4.8))
cdfInv <- function(cdf) {
    ret <- cdf*log(10)/avgSize
    if (cdf < avgSize*(.05*(1/50-1/1000)/(3-log10(50)))/log(10)) {
        ret <- ret * (3-log10(50)) / 0.05
        ret <- 1 / (1/50 - ret)
    } else if (cdf < avgSize*(.05*(1/50-1/1000)/(3-log10(50)) + .15*(1/1000-1/7200)/(log10(7200)-3))/log(10)) {
        ret <- ret - .05*(1/50-1/1000)/(3-log10(50))
        ret <- ret * (log10(7200)-3) / 0.15
        ret <- 1 / (1/1000 - ret)
    } else if (cdf < avgSize*(.05*(1/50-1/1000)/(3-log10(50)) + .15*(1/1000-1/7200)/(log10(7200)-3) + .7*(1/7200-1/(10^4.8))/(4.8-log10(7200)))/log(10)) {
        ret <- ret - .05*(1/50-1/1000)/(3-log10(50)) - .15*(1/1000-1/7200)/(log10(7200)-3)
        ret <- ret * (4.8-log10(7200)) / 0.7
        ret <- 1 / (1/7200 - ret)
    } else {
        ret <- ret - .05*(1/50-1/1000)/(3-log10(50)) - .15*(1/1000-1/7200)/(log10(7200)-3) - .7*(1/7200-1/(10^4.8))/(4.8-log10(7200))
        ret <- ret * (5.5-4.8) / 0.1
        ret <- 1 / (1/(10^4.8) - ret)
    }
    return(ret)
}

sizes <- c()
cdf <- seq(0.001, 1, 0.001)
for (i in cdf)
{
    sizes <- c(sizes, cdfInv(i))
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

filename = "Fabricated_Heavy_Middle.txt"
write(avgSize, file=filename)
df <- data.frame(sizes, cdf)
write.table(df, file=filename, row.names=FALSE, col.names=FALSE, append=TRUE)
