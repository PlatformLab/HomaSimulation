/*
 * HomaPkt.h
 *
 *  Created on: Sep 16, 2015
 *      Author: behnamm
 */

#ifndef HOMAPKT_H_
#define HOMAPKT_H_

#include "HomaPkt_m.h"

class HomaPkt : public HomaPkt_Base
{
  private:
    void copy(const HomaPkt& other);
                                                                                                                                          
  public:
    HomaPkt(const char *name=NULL, int kind=0);
    HomaPkt(const HomaPkt& other);
    HomaPkt& operator=(const HomaPkt& other);


    virtual HomaPkt *dup() const;

    // ADD CODE HERE to redefine and implement pure virtual functions from HomaPkt_Base

    /**
     * returns the header size of this packet.
     */
    uint32_t headerSize();

    /**
     * This function checks if there is a HomaPkt packet encapsulated in the messages
     * and returns it. Returns null if no HomaPkt is encapsulated.
     */
    static cPacket* searchEncapHomaPkt(cPacket* msg);

    /**
     * This function compares priority of two HomaPkt.
     */
    static int compareHomaPkts(cObject* obj1, cObject* obj2);

};

#endif /* HOMAPKT_H_ */
