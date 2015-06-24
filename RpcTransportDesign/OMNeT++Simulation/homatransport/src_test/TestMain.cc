#include <fstream>
#include "MsgSizeDistributions.h"

int main() {
    MsgSizeDistributions dctcpDist(
            "../../sizeDistributions/DCTCP_MsgSizeDist.txt",
            MsgSizeDistributions::DistributionChoice::DCTCP, 1);
    MsgSizeDistributions facebookDist(
            "../../sizeDistributions/FacebookKeyValueMsgSizeDist.txt",
            MsgSizeDistributions::DistributionChoice::FACEBOOK_KEY_VALUE, 1 );
    std::ofstream outDctcpStream;
    std::ofstream outFacebookStream;
    outDctcpStream.open("/tmp/dctcpSampleSizes.txt");
    outFacebookStream.open("/tmp/facebookSampleSizes.txt");
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
