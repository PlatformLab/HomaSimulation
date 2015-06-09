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
    int generateSizeFromDctcpDist();
    int generateFacebookMsgSize();
   
  private:
    std::vector<std::pair<int, double>> msgSizeProbDistVector;

    // Random number generator classes and datastructs
    std::random_device rdDevice;
    std::mt19937 randGen;
    std::uniform_real_distribution<> dist;
  
};



#endif /* MSGSIZEDISTRIBUTIONS_H_ */
