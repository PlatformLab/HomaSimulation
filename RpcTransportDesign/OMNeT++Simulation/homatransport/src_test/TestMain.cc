#include <fstream>
#include "MsgSizeDistributions.h"

int main() {
    MsgSizeDistributions dctcpDist(
            "../../sizeDistributions/DCTCP_MsgSizeDist.txt");
    MsgSizeDistributions facebookDist(
            "../../sizeDistributions/FacebookKeyValueMsgSizeDist.txt");
    std::ofstream outDctcpStream;
    std::ofstream outFacebookStream;
    outDctcpStream.open("/tmp/dctcpSampleSizes.txt");
    outFacebookStream.open("/tmp/facebookSampleSizes.txt");
    for (int i = 0; i < 100000; i++) {
        int size = dctcpDist.generateSizeFromDctcpDist(1);
        outDctcpStream << size << "\n"; 
        size = facebookDist.generateFacebookMsgSize();
        outFacebookStream << size << "\n";
    }
    outDctcpStream.close();
    outFacebookStream.close();
    
    return 0;
}
