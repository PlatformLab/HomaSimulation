/*
 * WorkloadEstimator.cc
 *
 *  Created on: Dec 23, 2015
 *      Author: behnamm
 */

#include <fstream>
#include <cstdio>
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include "WorkloadEstimator.h"
#include "transport/HomaPkt.h"

WorkloadEstimator::WorkloadEstimator(HomaConfigDepot* homaConfig)
    : cdfFromFile()
    , cbfFromFile()
    , cbfLastCapBytesFromFile()
    , rxCdfComputed()
    , sxCdfComputed()
    , avgSizeFromFile(0.0)
    , lastCbfCapMsgSize(0)
    , homaConfig(homaConfig)
{
    const char* workloadType = homaConfig->workloadType;
    if (workloadType != NULL) {
        std::string distFileName;
        if (strcmp(workloadType, "DCTCP") == 0) {
            distFileName = std::string(
                    "../../sizeDistributions/DCTCP_MsgSizeDist.txt");
        } else if (strcmp(workloadType, "TEST_DIST") == 0) {
            distFileName = std::string(
                    "../../sizeDistributions/TestDistribution.txt");
        } else if (strcmp(workloadType, "FACEBOOK_KEY_VALUE") == 0) {
            distFileName = std::string(
                    "../../sizeDistributions/FacebookKeyValueMsgSizeDist.txt");
        } else if (strcmp(workloadType, "FACEBOOK_WEB_SERVER_INTRACLUSTER")
                == 0) {
            distFileName = std::string("../../sizeDistributions/"
                    "Facebook_WebServerDist_IntraCluster.txt");
        } else if (strcmp(workloadType, "FACEBOOK_CACHE_FOLLOWER_INTRACLUSTER")
                == 0) {
            distFileName = std::string("../../sizeDistributions/"
                    "Facebook_CacheFollowerDist_IntraCluster.txt");
        } else if (strcmp(workloadType, "FACEBOOK_HADOOP_ALL")
                == 0) {
            distFileName = std::string("../../sizeDistributions/"
                    "Facebook_HadoopDist_All.txt");
        } else if (strcmp(workloadType, "FABRICATED_HEAVY_MIDDLE")
                == 0) {
            distFileName = std::string("../../sizeDistributions/"
                    "Fabricated_Heavy_Middle.txt");
        }  else if (strcmp(workloadType, "FABRICATED_HEAVY_HEAD")
                == 0) {
            distFileName = std::string("../../sizeDistributions/"
                    "Fabricated_Heavy_Head.txt");
        } else if (strcmp(workloadType, "PRESET_IN_FILE") == 0) {
            distFileName = std::string("../../sizeDistributions/"
                    "Fabricated_Heavy_Middle.txt");
        } else {
            throw cRuntimeError("'%s': Not a valid workload type.",
            workloadType);
        }

        std::ifstream distFileStream;
        distFileStream.open(distFileName.c_str());
        std::string avgMsgSizeStr;
        std::string sizeProbStr;

        // The first line of distFileName is the average message size of the
        // distribution.
        getline(distFileStream, avgMsgSizeStr);
        sscanf(avgMsgSizeStr.c_str(), "%lf", &avgSizeFromFile);
        if (strcmp(workloadType, "DCTCP") == 0) {
            avgSizeFromFile *= 1450; // AvgSize in terms of bytes
        }

        // reads msgSize<->probabilty pairs from "distFileName" file
        double prob;
        while(getline(distFileStream, sizeProbStr)) {
            int msgSize;
            sscanf(sizeProbStr.c_str(), "%d %lf",
                    &msgSize, &prob);
            cdfFromFile.push_back(std::make_pair(msgSize, prob));
        }
        distFileStream.close();

        // if the workloadType is FacebookKeyValue, half of the size
        // distribution is given by analytical formula and we need to complete
        // the distribution from that formula.
        if (strcmp(workloadType, "FACEBOOK_KEY_VALUE") == 0) {
            int sizeOffset = cdfFromFile.back().first;
            double probOffset = cdfFromFile.back().second;

            // Generalized pareto distribution parameters
            const double k = 0.348238;
            const double sigma = 214.476;

            const double maxProb = 0.999;
            double probIncr = (maxProb - probOffset) / 1000;
            double probInit = probOffset + probIncr;
            uint32_t size = 0;
            for (prob = probInit; prob <= maxProb; prob += probIncr) {
                size = (uint32_t)(round(sizeOffset +
                    (pow((1-probOffset)/(1-prob), k) - 1) * sigma / k));
                if (size != cdfFromFile.back().first) {
                    cdfFromFile.push_back(std::make_pair(size, prob));
                } else {
                    cdfFromFile.back().second = prob;
                }
            }
            cdfFromFile.back().second = 1.0;
        }
        getCbfFromCdf(cdfFromFile, UINT32_MAX);
    }
}

void
WorkloadEstimator::getCbfFromCdf(CdfVector& cdf, uint32_t cbfCapMsgSize)
{
    if (cbfCapMsgSize != lastCbfCapMsgSize) {
        lastCbfCapMsgSize = cbfCapMsgSize;
        // compute cbf from cdf and store it in the cbfFromFile
        double prevProb = 0.0;
        double cumBytes = 0.0;
        double cumRttBytes = 0.0;
        cbfFromFile.clear();
        cbfLastCapBytesFromFile.clear();
        CdfVector tempCbf = CdfVector();
        for (auto& sizeProbPair : cdfFromFile) {
            double prob = sizeProbPair.second - prevProb;
            prevProb = sizeProbPair.second;

            cumBytes += HomaPkt::getBytesOnWire(std::min(sizeProbPair.first,
                cbfCapMsgSize), PktType::UNSCHED_DATA) * prob;
            cbfFromFile.push_back(std::make_pair(sizeProbPair.first,
                cumBytes));

            
            double bytesOnSize;
            if (sizeProbPair.first <= cbfCapMsgSize) {
                bytesOnSize = HomaPkt::getBytesOnWire(sizeProbPair.first,
                    PktType::UNSCHED_DATA) * prob;
                cumRttBytes += bytesOnSize;
                tempCbf.push_back(
                    std::make_pair(sizeProbPair.first, bytesOnSize));
            } else if (sizeProbPair.first < 2 * cbfCapMsgSize) {
                bytesOnSize = HomaPkt::getBytesOnWire(cbfCapMsgSize,
                    PktType::UNSCHED_DATA) * prob;
                tempCbf.push_back(
                    std::make_pair(sizeProbPair.first, bytesOnSize));
                cumRttBytes += bytesOnSize;

                uint32_t remainBytes = sizeProbPair.first - cbfCapMsgSize;
                bytesOnSize = HomaPkt::getBytesOnWire(remainBytes,
                    PktType::UNSCHED_DATA) * prob;
                cumRttBytes += bytesOnSize;
                std::pair<uint32_t, double> pairToInsert =
                    std::make_pair(remainBytes, bytesOnSize);
                CdfVector::iterator it = std::lower_bound(
                    tempCbf.begin(), tempCbf.end(),
                    pairToInsert, CompCdfPairs());
                tempCbf.insert(it, pairToInsert);
            } else {
                bytesOnSize = HomaPkt::getBytesOnWire(cbfCapMsgSize,
                    PktType::UNSCHED_DATA) * prob;
                tempCbf.push_back(
                    std::make_pair(sizeProbPair.first, bytesOnSize));
                cumRttBytes += bytesOnSize;

                std::pair<uint32_t, double> pairToInsert =
                    std::make_pair(cbfCapMsgSize, bytesOnSize);
                CdfVector::iterator it = std::lower_bound(
                    tempCbf.begin(), tempCbf.end(),
                    pairToInsert, CompCdfPairs());
                if (it->first == cbfCapMsgSize) {
                    tempCbf[it-tempCbf.begin()].second += bytesOnSize;
                } else {
                    tempCbf.insert(it, pairToInsert);
                }
                cumRttBytes += bytesOnSize;
            }
        }

        for (auto& sizeProbPair : cbfFromFile) {
            sizeProbPair.second /= cbfFromFile.back().second;
        }
        
        double rollingSum = 0;
        for (auto& sizeProbStr : tempCbf) {
            rollingSum += sizeProbStr.second;
            cbfLastCapBytesFromFile.push_back(std::make_pair(sizeProbStr.first,
                rollingSum/cumRttBytes));
        }
        cbfLastCapBytesFromFile.back().second = 1.00;
    }
}

void
WorkloadEstimator::recomputeRxWorkload(uint32_t msgSize, simtime_t timeMsgArrived)
{}

void
WorkloadEstimator::recomputeSxWorkload(uint32_t msgSize, simtime_t timeMsgSent)
{}
