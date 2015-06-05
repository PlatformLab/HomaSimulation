/*
 * MsgSizeDistributions.cc
 *
 *  Created on: May 22, 2015
 *      Author: neverhood
 */

#include <fstream>
#include <cstdio>
#include <math.h>
#include "MsgSizeDistributions.h"


MsgSizeDistributions::MsgSizeDistributions(const char* distFileName)
    : msgSizeProbDistVector()
    , rdDevice()
    , randGen(rdDevice())
    , dist(0, 1)
{
    std::ifstream distFileStream;
    std::string sizeProbStr;
    distFileStream.open(distFileName);

    while(!distFileStream.eof()) {
        int msgSize;
        double prob;
        getline(distFileStream, sizeProbStr);
        sscanf(sizeProbStr.c_str(), "%d %lf", 
                &msgSize, &prob);
        msgSizeProbDistVector.push_back(std::make_pair(msgSize, prob));
    }
}

int
MsgSizeDistributions::generateSizeFromDctcpDist(int maxPayloadBytes)
{
    if (maxPayloadBytes == 0) {
        maxPayloadBytes = 
                MAX_ETHERNET_FRAME_SIZE - IP_HEADER_SIZE - UDP_HEADER_SIZE;
    }
    
    double prob = dist(randGen);
    for (size_t i = 0; i < msgSizeProbDistVector.size(); ++i)
    {
        if (msgSizeProbDistVector[i].second >= prob)
            return (msgSizeProbDistVector[i].first) * maxPayloadBytes; 
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
