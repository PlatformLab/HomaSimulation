/*
 * MsgSizeDistributions.h
 *
 *  Created on: May 22, 2015
 *      Author: neverhood
 */

#ifndef MSGSIZEDISTRIBUTIONS_H_
#define MSGSIZEDISTRIBUTIONS_H_

#include <string>
#include <vector>
#include <utility>
#include <random>

class MsgSizeDistributions {
  public:
    explicit MsgSizeDistributions(const char* distFileName);
    ~MsgSizeDistributions() {};
    int generateSizeFromDctcpDist(int maxPayloadBytes);
    int generateFacebookMsgSize();
   
  public:
    static const uint32_t MAX_ETHERNET_FRAME_SIZE = 1500;
    static const uint32_t IP_HEADER_SIZE = 20;
    static const uint32_t UDP_HEADER_SIZE = 8;


  private:
    std::vector<std::pair<int, double>> msgSizeProbDistVector;

    // Random number generator classes and datastructs
    std::random_device rdDevice;
    std::mt19937 randGen;
    std::uniform_real_distribution<> dist;
    
};



#endif /* MSGSIZEDISTRIBUTIONS_H_ */
