/* -*-	Mode:C++; -*- */

/*
 * Copyright (c) 2007 Regents of the SIGNET lab, University of Padova.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Padova (SIGNET lab) nor the 
 *    names of its contributors may be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RATEADAPTER_H
#define RATEADAPTER_H

#include<Mac80211EventHandler.h>
#include<otcl.h>
#include<stdio.h>
#include<fstream>


#define  RA_MAX_NUM_PHY_MODES 12

class RateAdapter : public Mac80211EventHandler
{

public:

  RateAdapter();
  virtual ~RateAdapter();

  virtual int command(int argc, const char*const* argv);

protected:

  PhyMode modes[RA_MAX_NUM_PHY_MODES]; /**< Array of PhyModes to be used
					* for rate adaptation */ 


  /** 
   * Increases the Phy Mode being used
   * 
   * @return 0 if the mode was increased, nonzero if the max mode was already reached
   */
  int incrMode();

  /** 
   * Decreases the Phy Mode being used
   * 
   * @return 0 if the mode was decreased, nonzero if the min mode was already reached
   */
  int decrMode();

  /** 
   * get current Phy Mode
   * 
   * @return the Phy Mode being used
   */
  PhyMode getCurrMode();


  /** 
   * sets the current Phy Mode to modes[idx]
   * 
   * @param idx index of modes[], must be >= 0, < RA_MAX_NUM_PHY_MODES
   * and < numPhyModes_
   */
  void setModeAtIndex(int idx);


  PhyMode getModeAtIndex(int idx);


  void checkConfiguration();


  int curr_mode_index;              /**< modes[curr_mode_index] is the
				     * PhyMode being currently used */ 

  int numPhyModes_;                 /**< How many Phy Modes will be
				     * considered for rate adaptation */ 

 
  int debug_;

  FILE *file;

};



#endif /* RATEADAPTER_H */
