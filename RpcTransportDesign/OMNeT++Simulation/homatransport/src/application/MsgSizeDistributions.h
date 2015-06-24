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
#include <exception>

class MsgSizeDistributions {

  public:
    enum DistributionChoice {
        DCTCP,
        FACEBOOK_KEY_VALUE
    };

    MsgSizeDistributions(const char* distFileName,
            DistributionChoice distSelector, int maxDataBytesPerPkt);
    ~MsgSizeDistributions() {};
    int sizeGeneratorWrapper();
    double getAvgMsgSize();
   
  private:
    std::vector<std::pair<int, double>> msgSizeProbDistVector;

    // Random number generator classes and datastructs
    std::random_device rdDevice;
    std::mt19937 randGen;
    std::uniform_real_distribution<> dist;
 
    // The value of this variable will determine which distribution should be
    // used for generating new messages in the sizeGenratorWrapper()
    DistributionChoice distSelector;

    // Average message size from the constructed distribution. This value is
    // read from the first line of the file 'distFileName'. 
    double avgMsgSize;

    // Max number of data bytes that one packet can carry.
    int maxDataBytesPerPkt;

    int generateSizeFromDctcpDist();
    int generateFacebookMsgSize();
};

struct MsgSizeDistException : std::exception {
    std::string msg;
    MsgSizeDistException(const char* msg)
        : msg(msg)
    {
    }
    
    const char* what() const noexcept
    {
        return msg.c_str(); 
    }
};



#endif /* MSGSIZEDISTRIBUTIONS_H_ */
