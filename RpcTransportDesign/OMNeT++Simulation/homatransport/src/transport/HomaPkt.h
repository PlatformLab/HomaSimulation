/*
 * HomaPkt.h
 *
 *  Created on: Sep 16, 2015
 *      Author: behnamm
 */

#ifndef HOMAPKT_H_
#define HOMAPKT_H_

#include "transport/HomaPkt_m.h"

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

};

#endif /* HOMAPKT_H_ */
