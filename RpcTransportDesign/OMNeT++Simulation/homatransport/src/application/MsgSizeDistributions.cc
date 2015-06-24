/*
 * MsgSizeDistributions.cc
 *
 *  Created on: May 22, 2015
 *      Author: neverhood
 */

#include <fstream>
#include <cstdio>
#include <math.h>
#include <stdio.h>
#include "MsgSizeDistributions.h"


MsgSizeDistributions::MsgSizeDistributions(const char* distFileName,
        DistributionChoice distSelector, int maxDataBytesPerPkt)
    : msgSizeProbDistVector()
    , rdDevice()
    , randGen(rdDevice())
    , dist(0, 1)
    , distSelector(distSelector)
    , avgMsgSize(0)
    , maxDataBytesPerPkt(maxDataBytesPerPkt)
{
    std::ifstream distFileStream;
    std::string avgMsgSizeStr;
    std::string sizeProbStr;
    distFileStream.open(distFileName);
    if (distFileStream.fail()) {
        char buf[100]; 
        snprintf(buf, 100, "%s: Failed to open file.", distFileName);
        throw MsgSizeDistException(buf);
    }

    // The first line of distFileName is the average message size of the
    // distribution.
    getline(distFileStream, avgMsgSizeStr); 
    sscanf(avgMsgSizeStr.c_str(), "%lf", &avgMsgSize);

    // reads msgSize<->probabilty pairs from "distFileName" file
    while(!distFileStream.eof()) {
        int msgSize;
        double prob;
        getline(distFileStream, sizeProbStr);
        sscanf(sizeProbStr.c_str(), "%d %lf", 
                &msgSize, &prob);
        msgSizeProbDistVector.push_back(std::make_pair(msgSize, prob));
    }

    switch (distSelector) {
        case DistributionChoice::DCTCP:
        case DistributionChoice::FACEBOOK_KEY_VALUE: 
            this->distSelector = distSelector;
            break;
        default:
            throw MsgSizeDistException(
                    "Invalide Message Size Distribution Selected");
    }


}

int
MsgSizeDistributions::sizeGeneratorWrapper()
{
    switch(distSelector) {
        case DistributionChoice::DCTCP:
            return generateSizeFromDctcpDist();
        case DistributionChoice::FACEBOOK_KEY_VALUE: 
            return generateFacebookMsgSize();
        default:
            return 0;
    }
}

double
MsgSizeDistributions::getAvgMsgSize()
{
    return avgMsgSize;
}

int
MsgSizeDistributions::generateSizeFromDctcpDist()
{
    
    double prob = dist(randGen);
    for (size_t i = 0; i < msgSizeProbDistVector.size(); ++i)
    {
        if (msgSizeProbDistVector[i].second >= prob)
            return (msgSizeProbDistVector[i].first) * maxDataBytesPerPkt;
    }

    return 0;
}

int
MsgSizeDistributions::generateFacebookMsgSize()
{
    // Facebook workload constants
    int sizeOffset = msgSizeProbDistVector.back().first; 
    double probOffset = msgSizeProbDistVector.back().second; 

    // Generalized pareto distribution parameters
    const double k = 0.348238;
    const double sigma = 214.476;

    // Maximum allowed Msg size
    const int maxSize = 0x40000000;

    double prob = dist(randGen);
    
    if (prob <= msgSizeProbDistVector.back().second) {
        int first = 0;
        int last = msgSizeProbDistVector.size() - 1;
        
        while (first < last) {
            int mid = first + (last - first)/2;
            if (prob <= msgSizeProbDistVector[mid].second) {
                last = mid;
            } else {
                first = mid + 1;
            }
        }
        return msgSizeProbDistVector[first].first;

    } else {
        double msgSize = 
            round( sizeOffset + 
            (pow((1-probOffset)/(1-prob), k) - 1) * sigma / k );

        return msgSize > maxSize ? maxSize : (int)(msgSize); 
    }
}
