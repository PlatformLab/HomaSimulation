#include <fstream>
#include "MsgSizeDistributions.h"

int main() {
    MsgSizeDistributions dctcpDist(
            "../../sizeDistributions/DCTCP_MsgSizeDist.txt",
            1500, MsgSizeDistributions::InterArrivalDist::EXPONENTIAL,
            MsgSizeDistributions::DistributionChoice::DCTCP);
    MsgSizeDistributions facebookDist(
            "../../sizeDistributions/FacebookKeyValueMsgSizeDist.txt",
            1500, MsgSizeDistributions::InterArrivalDist::EXPONENTIAL,
            MsgSizeDistributions::DistributionChoice::FACEBOOK_KEY_VALUE);
    std::ofstream outDctcpStream;
    const char* dctcpFileName = "/tmp/dctcpSampleSizes.txt";
    outDctcpStream.open(dctcpFileName);
    std::ofstream outFacebookStream;
    const char* keyValueFileName = "/tmp/facebookSampleSizes.txt";
    outFacebookStream.open(keyValueFileName);
    for (int i = 0; i < 100000; i++) {
        int size = dctcpDist.sizeGeneratorWrapper();
        outDctcpStream << size << "\n"; 
        size = facebookDist.sizeGeneratorWrapper();
        outFacebookStream << size << "\n";
    }
    outDctcpStream.close();
    outFacebookStream.close();
    return 0;
}
