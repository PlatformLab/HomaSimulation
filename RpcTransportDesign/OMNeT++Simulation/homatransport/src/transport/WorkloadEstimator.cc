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
    , remainSizeCdf()
    , remainSizeCbf()
    , cbfLastCapBytesFromFile()
    , avgSizeFromFile(0.0)
    , rxCdfComputed()
    , sxCdfComputed()
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
        } else if (strcmp(workloadType, "GOOGLE_SEARCH_RPC")
                == 0) {
            distFileName = std::string("../../sizeDistributions/"
                    "Google_SearchRPC.txt");
        } else if (strcmp(workloadType, "GOOGLE_ALL_RPC")
                == 0) {
            distFileName = std::string("../../sizeDistributions/"
                    "Google_AllRPC.txt");
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
            HomaPkt homaPkt = HomaPkt();
            homaPkt.setPktType(PktType::UNSCHED_DATA);
            uint32_t maxDataBytesPerEthFrame = MAX_ETHERNET_PAYLOAD_BYTES -
                IP_HEADER_SIZE - UDP_HEADER_SIZE;
            uint32_t maxDataBytesPerPkt = maxDataBytesPerEthFrame -
                homaPkt.headerSize();
            avgSizeFromFile *= maxDataBytesPerPkt; // AvgSize in bytes.
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
    }
}

void
WorkloadEstimator::getRemainSizeCdfCbf(CdfVector& cdf, uint32_t cbfCapMsgSize,
    uint32_t boostTailBytesPrio)
{
    remainSizeCdf.clear();
    remainSizeCbf.clear();
    uint32_t req = homaConfig->defaultReqBytes;
    uint32_t unsched = homaConfig->defaultUnschedBytes;
    //uint32_t hdrs = ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE +
    //    ETHERNET_HDR_SIZE + INTER_PKT_GAP + IP_HEADER_SIZE + UDP_HEADER_SIZE;

    //HomaPkt homaReqPkt = HomaPkt();
    //homaReqPkt.setPktType(PktType::REQUEST);
    //uint32_t reqHdr = hdrs + homaReqPkt.headerSize();

    HomaPkt homaUnschedPkt = HomaPkt();
    homaUnschedPkt.setPktType(PktType::UNSCHED_DATA);
    uint32_t unschedPerPkt = MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE -
        UDP_HEADER_SIZE - homaUnschedPkt.headerSize();
    //uint32_t unschedHdr = hdrs + homaUnschedPkt.headerSize();

    HomaPkt homaSchedPkt = HomaPkt();
    homaSchedPkt.setPktType(PktType::SCHED_DATA);
    uint32_t schedPerPkt = MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE -
        UDP_HEADER_SIZE - homaSchedPkt.headerSize();
    //uint32_t schedHdr = hdrs + homaSchedPkt.headerSize();

    //HomaPkt homaGrantPkt = HomaPkt();
    //homaGrantPkt.setPktType(PktType::GRANT);
    //uint32_t grantHdr = hdrs + homaGrantPkt.headerSize();

    CdfList remSizePdf;
    CdfList remSizePbf;
    double prevProb = 0.0;
    double cumPktBytes = 0.0;
    double cumGrantBytes = 0.0;
    double cumPkts = 0.0;
    for (auto& sizeProbPair : cdf) {
        double prob = sizeProbPair.second - prevProb;
        prevProb = sizeProbPair.second;
        uint32_t size = sizeProbPair.first;

        // packetize the message size
        std::list<std::pair<uint32_t, uint32_t>> packets;
        uint32_t sizeRemain = size;
        uint32_t sizeOnWire = 0;
        uint32_t allGrantBytes = 0;
        uint32_t pktedSize = 0;
        uint32_t totalUnsched = std::min(req+unsched, size);
        while (sizeRemain) {
            uint32_t pkt;
            uint32_t pktOnWire;
            if (sizeRemain == size) {
                pkt = std::min(req, sizeRemain);
                //pktOnWire = pkt+reqHdr;
                pktOnWire = HomaPkt::getBytesOnWire(pkt, PktType::REQUEST);
            } else if (pktedSize < totalUnsched) {
                pkt = std::min(totalUnsched - pktedSize, unschedPerPkt);
                //pktOnWire = pkt+unschedHdr;
                pktOnWire = HomaPkt::getBytesOnWire(pkt, PktType::UNSCHED_DATA);
            } else {
                pkt = std::min(schedPerPkt, sizeRemain);
                //pktOnWire = pkt+schedHdr;
                pktOnWire = HomaPkt::getBytesOnWire(pkt, PktType::SCHED_DATA);
                //allGrantBytes += grantHdr;
                allGrantBytes += HomaPkt::getBytesOnWire(0, PktType::GRANT);
            }
            packets.push_back(std::make_pair(pkt, pktOnWire));
            sizeOnWire += pktOnWire;
            sizeRemain -= pkt;
            pktedSize += pkt;
        }

        cumGrantBytes += prob * allGrantBytes; 
        // fill in pdf list of remaining sizes
        CdfList::iterator it = remSizePdf.end(); 
        CdfList::iterator itt = remSizePbf.end(); 
        sizeRemain = size;
        uint32_t toSendOnWire = sizeOnWire;
        uint32_t pktedOnWire = 0;
        for (auto pktPair : packets) {
            if (pktedOnWire >= cbfCapMsgSize &&
                    toSendOnWire >= boostTailBytesPrio) {
                // We should only account the first cbfCapMsgSize and
                // boostTailBytesPrio bytes of the message in calculating cbf if
                // these conditions are specified. So update loop states and
                // skip loop iteration if these conditions are specified but not
                // met. 
                pktedOnWire += pktPair.second;
                toSendOnWire -= pktPair.second;
                sizeRemain -= pktPair.first;
                continue;
            }

            for ( ;it != remSizePdf.begin(); ) {
                it--;
                itt--;
                if ((*it).first <= sizeRemain) {
                    break;
                }
            }

            if (it == remSizePdf.end()) {
                //insert
                remSizePdf.insert(it,
                    std::make_pair(sizeRemain, prob));
                remSizePbf.insert(itt,
                    std::make_pair(sizeRemain, prob*pktPair.second));
            } else if ((*it).first == sizeRemain) {
                // add up
                (*it).second += prob;
                (*itt).second += prob*pktPair.second;

            } else if ((*it).first < sizeRemain) {
                // insert
                it++;
                remSizePdf.insert(it, std::make_pair(sizeRemain, prob));
                itt++;
                remSizePbf.insert(itt,
                    std::make_pair(sizeRemain, prob*pktPair.second));
            } else {
                ASSERT(it == remSizePdf.begin());
                // insert
                remSizePdf.insert(it, std::make_pair(sizeRemain, prob));
                remSizePbf.insert(itt,
                    std::make_pair(sizeRemain, prob*pktPair.second));

            }
            pktedOnWire += pktPair.second;
            toSendOnWire -= pktPair.second;
            sizeRemain -= pktPair.first;

            cumPktBytes += prob * pktPair.second;
            cumPkts += prob;
        }

    }

    // fill in remainSizeCdf and remainSizeCbf
    double cumsumPkts = 0.0;
    double cumsumBytes = 0.0;
    if (homaConfig->accountForGrantTraffic) {
        cumPktBytes += cumGrantBytes;
        cumsumBytes += cumGrantBytes/cumPktBytes;

        // grant traffic included as a message of size zero at the begining of
        // the cbf vector.
        remainSizeCbf.push_back(std::make_pair(0, cumsumBytes));
    }

    auto it = remSizePbf.begin();
    uint32_t prevRemSize = 0;
    for (auto cdfPair : remSizePdf) {
        ASSERT(prevRemSize < cdfPair.first && (*it).first == cdfPair.first);
        prevRemSize = cdfPair.first;
        cumsumPkts += cdfPair.second / cumPkts;
        remainSizeCdf.push_back(std::make_pair(cdfPair.first, cumsumPkts));
        cumsumBytes += (*it).second / cumPktBytes;
        remainSizeCbf.push_back(std::make_pair((*it).first, cumsumBytes));
        it++;
    }
}

void
WorkloadEstimator::getCbfFromCdf(CdfVector& cdf, uint32_t cbfCapMsgSize,
    uint32_t boostTailBytesPrio)
{
    cbfFromFile.clear();
    cbfLastCapBytesFromFile.clear();
    uint32_t req = homaConfig->defaultReqBytes;
    uint32_t unsched = homaConfig->defaultUnschedBytes;
    uint32_t maxEthFrame = MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_CRC_SIZE +
        ETHERNET_PREAMBLE_SIZE + ETHERNET_HDR_SIZE + INTER_PKT_GAP;

    HomaPkt homaUnschedPkt = HomaPkt();
    homaUnschedPkt.setPktType(PktType::UNSCHED_DATA);
    uint32_t unschedPerPkt = MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE -
        UDP_HEADER_SIZE - homaUnschedPkt.headerSize();

    HomaPkt homaSchedPkt = HomaPkt();
    homaSchedPkt.setPktType(PktType::SCHED_DATA);
    uint32_t schedPerPkt = MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE -
        UDP_HEADER_SIZE - homaSchedPkt.headerSize();

    if (homaConfig->accountForGrantTraffic) {
        // grant traffic included as a message of size zero at the begining of
        // the two vectors.
        cbfFromFile.push_back(std::make_pair(0, 0));
        cbfLastCapBytesFromFile.push_back(std::make_pair(0, 0));
    }

    // compute cbf from cdf and store it in the cbfFromFile
    double prevProb = 0.0;
    double cumBytes = 0.0;
    double cumRttBytes = 0.0;
    double cumGrants = 0.0;
    CdfVector tempCbf = CdfVector();
    for (auto& sizeProbPair : cdfFromFile) {
        double prob = sizeProbPair.second - prevProb;
        uint32_t size = sizeProbPair.first;
        prevProb = sizeProbPair.second;

        // compute cbf
        uint32_t reqOnWire = 0;
        uint32_t unschedOnWire = 0;
        uint32_t schedOnWire = 0;
        uint32_t sizeOnWire = 0;
        uint32_t grantsOnWire = 0;
        if (size < req) {
            reqOnWire = HomaPkt::getBytesOnWire(std::min(req, size),
                PktType::REQUEST);
        } else if (size < req+unsched) {
            reqOnWire = HomaPkt::getBytesOnWire(req, PktType::REQUEST);
            unschedOnWire = HomaPkt::getBytesOnWire(std::min(size-req, unsched),
                PktType::UNSCHED_DATA);
        } else {
            reqOnWire = HomaPkt::getBytesOnWire(req, PktType::REQUEST);
            unschedOnWire = HomaPkt::getBytesOnWire(unsched,
                PktType::UNSCHED_DATA);
            schedOnWire = HomaPkt::getBytesOnWire(size-req-unsched,
                PktType::SCHED_DATA);
            uint32_t numGrants = (uint32_t)ceil((1.0*schedOnWire)/maxEthFrame);
            grantsOnWire = numGrants*HomaPkt::getBytesOnWire(0, PktType::GRANT);
        }
        sizeOnWire = reqOnWire + unschedOnWire + schedOnWire;
        cumGrants += grantsOnWire * prob;
        cumBytes += sizeOnWire * prob;
        cbfFromFile.push_back(std::make_pair(sizeProbPair.first,
            sizeOnWire*prob));

        // compute cbfLastCapBytesFromFile 
        // packetize the message size
        std::list<std::pair<uint32_t, uint32_t>> packets;
        sizeOnWire = 0;
        uint32_t allGrantBytes = 0;
        uint32_t pktedSize = 0;
        uint32_t totalUnsched = std::min(req+unsched, size);
        uint32_t sizeRemain = size;
        while (sizeRemain) {
            uint32_t pkt;
            uint32_t pktOnWire;
            if (sizeRemain == size) {
                pkt = std::min(req, sizeRemain);
                //pktOnWire = pkt+reqHdr;
                pktOnWire = HomaPkt::getBytesOnWire(pkt, PktType::REQUEST);
            } else if (pktedSize < totalUnsched) {
                pkt = std::min(totalUnsched - pktedSize, unschedPerPkt);
                //pktOnWire = pkt+unschedHdr;
                pktOnWire = HomaPkt::getBytesOnWire(pkt, PktType::UNSCHED_DATA);
            } else {
                pkt = std::min(schedPerPkt, sizeRemain);
                //pktOnWire = pkt+schedHdr;
                pktOnWire = HomaPkt::getBytesOnWire(pkt, PktType::SCHED_DATA);
                //allGrantBytes += grantHdr;
                allGrantBytes += HomaPkt::getBytesOnWire(0, PktType::GRANT);
            }
            packets.push_back(std::make_pair(pkt, pktOnWire));
            sizeOnWire += pktOnWire;
            sizeRemain -= pkt;
            pktedSize += pkt;
        }

        sizeRemain = size;
        uint32_t toSendOnWire = sizeOnWire;
        uint32_t pktedOnWire = 0;
        uint32_t onWire = 0;
        for (auto& pktPair : packets) {
            if (pktedOnWire >= cbfCapMsgSize &&
                    toSendOnWire >= boostTailBytesPrio) {
                // We should only account the first cbfCapMsgSize and
                // boostTailBytesPrio bytes of the message in calculating cbf if
                // these conditions are specified. So skip loop iteration if
                // these conditions are specified but not met. 
                pktedOnWire += pktPair.second;
                toSendOnWire -= pktPair.second;
                continue;
            }
            pktedOnWire += pktPair.second;
            toSendOnWire -= pktPair.second;
            onWire += pktPair.second;
            cumRttBytes += pktPair.second*prob;
        }
        cbfLastCapBytesFromFile.push_back(std::make_pair(size, onWire*prob));
    }

    double rollingSum1 = 0;
    double rollingSum2 = 0;
    if (homaConfig->accountForGrantTraffic) {
        cumBytes += cumGrants;
        cumRttBytes += cumGrants;
        cbfFromFile[0].second = cumGrants;
        cbfLastCapBytesFromFile[0].second = cumGrants;
    }

    size_t i = 0;
    for (auto& sizeProbPair : cbfFromFile) {
        i++;
        rollingSum1 += sizeProbPair.second;
        sizeProbPair.second = rollingSum1 / cumBytes;
    }
    
    for (auto& sizeProbStr : cbfLastCapBytesFromFile) {
        rollingSum2 += sizeProbStr.second;
        sizeProbStr.second = rollingSum2 / cumRttBytes;
    }
}

void
WorkloadEstimator::recomputeRxWorkload(uint32_t msgSize, simtime_t timeMsgArrived)
{}

void
WorkloadEstimator::recomputeSxWorkload(uint32_t msgSize, simtime_t timeMsgSent)
{}
