#!/usr/bin/Rscript

cdfFromLogScaledCbf <- function()
{
    # This function fabricates a cdf from a
    # "Piecewise LogLinear Cumulative Byte Function" of a workload.
    # all you need to do is to specify sizeIntervals and cbf (cumulative byte
    # function) values in below. The assumption is that cbf is piecewise linear
    # function of log10(sizeIntervals) with breakpoints defined in sizeIntervals
    # and cbf vectors.

    # monotonically increasing vector of msg sizes
    #sizeIntervals <- c(1, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 3529904)
    sizeIntervals <- c(1, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 15158197)

    # monotonically increaing cbf (cumulative byte function) vector. First element
    # is always 0 and last element is always one. Each cbf value in this vector
    # corresponds to a size in the sizeIntervals vector.
    #cbf <- c(0, .0016, .0160256, .01923077, .07051282, .4455128, .525641, .6025641, .6987179, .7403846, .7532051, .7660256, .7884615, .900641, .9455128, .9519231, 1)
    cbf <- c(0, .0001205128, .001010256, .0070385, .01602564, .05128205, .07051282, .09294872, .1185897, .1762821, .2564103, .3012821, .3461538, .4679487, .5512321, .6826923, 1)

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
    numElemDouble = 3
    for (ind in seq(1, length(cdfRanges)))
    {
        numElemDouble <- numElemDouble*1.3
        numElem <- as.integer(numElemDouble)
        cdf <- c(cdf, seq(cdfBounds[ind], cdfBounds[ind+1], (cdfBounds[ind+1] - cdfBounds[ind])/numElem))
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

    filename = "Google_AllRPC.txt"
    write(avgSize, file=filename)
    df <- data.frame(sizes, cdf)
    write.table(df, file=filename, row.names=FALSE, col.names=FALSE, append=TRUE)
}

cdfFromCdfSamples <- function()
{
    # This function fabricates a cdf from a
    # "Piecewise LogLinear CDF Function" of a workload.
    # all you need to do is to specify sizeIntervals and cdf
    # values in below. The assumption is that cdf is piecewise linear
    # function of log10(sizeIntervals) with breakpoints defined in sizeIntervals
    # and cdf vectors.

    # monotonically increasing vector of msg sizes
    sizeIntervals <- c()


    # monotonically increaing cdf vector. First element
    # is always 0 and last element is always one. Each cdf value in this vector
    # corresponds to a size in the sizeIntervals vector.
    cdfIntervals <- c()

    # For Facebook Hadoop All
    sizeIntervals <- c(50, 92 ,217 ,271 ,300 ,326 ,376 ,425 ,480 ,600 ,679 ,737 ,800 ,885 ,1042 ,1277 ,
        1413 ,1534 ,1664 ,2214 ,3396 ,5210 ,7830 ,31946 ,36844 ,39973 ,44260 ,47050 ,51046 ,62583 ,
        70722 ,75180 ,81565 ,102058 ,115331 ,133013 ,156563 ,184284 ,212546 ,240177 ,288520 ,470507 ,
        736642 ,940700 ,1769244 ,2169118 ,10000000)
    cdfIntervals <- c(0, 0.0074 ,0.0149 ,0.0409 ,0.0520 ,0.1375 ,0.2026 ,0.2230 ,0.2639 ,0.4591 ,
        0.5167 ,0.5520 ,0.5762 ,0.5912 ,0.6097 ,0.6282 ,0.6394 ,0.6468 ,0.6673 ,0.6747 ,0.6840 ,
        0.6933 ,0.7082 ,0.7212 ,0.7305 ,0.7398 ,0.7584 ,0.7788 ,0.8160 ,0.8234 ,0.8327 ,0.8420 ,
        0.8717 ,0.8885 ,0.8978 ,0.9070 ,0.9164 ,0.9257 ,0.9349 ,0.9442 ,0.9535 ,0.9628 ,0.9684 ,
        0.9721 ,0.9814 ,0.9870 ,1.0000)

    # For FacebookWebServer
    #sizeIntervals <- c(50, 84 ,153 ,301 ,500 ,639 ,800 ,900 ,1000 ,1085 ,1357 ,1534 ,
    #    1664 ,1843 ,2000 ,3000 ,4000 ,5000 ,6000 ,7000 ,8000 ,9000 ,10000 ,14140 ,20000 ,
    #    24512 ,30000 ,34660 ,40000 ,45171 ,50000 ,60000 ,70000 ,80000 ,100000 ,144310 ,
    #    200000 ,260573 ,300000 ,353730 ,400000 ,442606 ,565213 ,778003 ,1000000)
    #
    #cdfIntervals <- c(0.0 ,0.061 ,0.126 ,0.215 ,0.226 ,0.2407 ,0.2537 ,0.2667 ,0.2926 ,
    #    0.3130 ,0.3352 ,0.3500 ,0.3722 ,0.3963 ,0.4093 ,0.4796 ,0.5259 ,0.5630 ,0.5889 ,
    #    0.6111 ,0.6260 ,0.6407 ,0.6519 ,0.6852 ,0.7148 ,0.7315 ,0.7463 ,0.7574 ,0.7667 ,
    #    0.7759 ,0.7815 ,0.7926 ,0.8 ,0.8074 ,0.8148 ,0.8241 ,0.8574 ,0.8944 ,0.9222 ,
    #    0.9556 ,0.9741 ,0.9833 ,0.9907 ,0.9963 ,1.0)

    # For Facebook CacheFollower Intracluster
    #sizeIntervals <- c(50 ,68 ,144 ,266 ,340 ,490 ,577 ,639 ,707 ,900 ,1042 ,1110 ,1303 ,1414 ,1440 ,2000 ,
    #    2450 ,3000 ,3466 ,4000 ,5000 ,6000 ,8000 ,10000 ,13855 ,16643 ,18057 ,25531 ,28270 ,36101 ,44260 ,
    #    62583 , 72178 ,81565 ,92173 ,102060 ,110726 ,127700 ,141398 ,156563 ,169860 ,184283 ,199933 ,212537 ,
    #    225934 ,245121 ,260573 ,276998 ,326042 ,391670 ,531696 ,588724 ,1385457 ,1503111 ,1597864 ,1681372 ,
    #    1787361 ,1842836 ,1959004 ,2125366 ,2213771 ,2329467 ,2553169 ,2827011 ,3006219 ,3260426 ,3837698)
    #cdfIntervals <- c(0.0 ,0.0204 ,0.0409 ,0.0539 ,0.1803 ,0.2770 ,0.3048 ,0.3178 ,0.3532 ,0.3643 ,0.3773 ,
    #    0.3996 ,0.4052 ,0.4126 ,0.4238 ,0.4368 ,0.4461 ,0.4535 ,0.4572 ,0.4610 ,0.4665 ,0.4703 ,0.4758 ,0.4777 ,
    #    0.4833 ,0.4870 ,0.4888 ,0.4944 ,0.5000 ,0.5037 ,0.5091 ,0.5186 ,0.5223 ,0.5260 ,0.5297 ,0.5335 ,0.5390 ,
    #    0.5465 ,0.5558 ,0.5651 ,0.5743 ,0.5836 ,0.5923 ,0.6022 ,0.6115 ,0.6208 ,0.6301 ,0.6394 ,0.6580 ,0.6766 ,
    #    0.6952 ,0.7138 ,0.7200 ,0.7230 ,0.7416 ,0.7602 ,0.7787 ,0.7974 ,0.8160 ,0.8346 ,0.8532 ,0.8717 ,0.9276 ,
    #    0.9740 ,0.9888 ,0.9944 ,1.0)

    cdfRanges <- cdfIntervals[2:length(cdfIntervals)]
    cdfConsts <- cdfIntervals[1:length(cdfIntervals)-1]

    #create cdf sample vector and find the corresponding sizes.
    #cdf sample vector must be sorted: cdf <- seq(0.001, 1, 0.001)
    cdf <- c()
    for (ind in seq(1, length(cdfRanges)))
    {
        cdf <- c(cdf, seq(cdfIntervals[ind], cdfIntervals[ind+1], (cdfIntervals[ind+1] - cdfIntervals[ind])/5))
    }
    cdf <- sort(unique(cdf))


    #Corresponding sizes for cdf values
    sizes <- c()
    for (cdfVal in cdf)
    {
        ind <- match(1, findInterval(cdfRanges, cdfVal))
        logMsgSize <- (cdfVal - cdfConsts[ind]) *
            (log10(sizeIntervals[ind+1])-log10(sizeIntervals[ind])) /
            (cdfRanges[ind]-cdfConsts[ind]) + log10(sizeIntervals[ind])
        sizes <- c(sizes, 10**(logMsgSize))
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

    filename = "Facebook_HadoopDist_All.txt"
    write(avgSize, file=filename)
    df <- data.frame(sizes, cdf)
    write.table(df, file=filename, row.names=FALSE, col.names=FALSE, append=TRUE)
}

cdfFromLogScaledCbf()
