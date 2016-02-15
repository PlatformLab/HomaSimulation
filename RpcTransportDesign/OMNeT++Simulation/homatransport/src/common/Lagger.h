//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#ifndef __HOMATRANSPORT_LAGGER_H_
#define __HOMATRANSPORT_LAGGER_H_

#include <omnetpp.h>
#include "common/Minimal.h"

/**
 * delays a packet before sending it out. This module is intended for the use
 * in EthernetInterface module. More details in the ned file.
 */
class Lagger : public cSimpleModule
{
  PUBLIC:
    Lagger();
    ~Lagger(){}

  PROTECTED:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    virtual void inputHookPktHandler(cMessage* msg);
    virtual void outputHookPktHandler(cMessage* msg);


  PROTECTED:
    std::string hookType;
    cPar* delayPar;
    cModule* mac;
};

#endif
