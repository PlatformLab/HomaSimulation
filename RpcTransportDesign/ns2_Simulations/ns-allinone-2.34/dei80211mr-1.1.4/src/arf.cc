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


#include "arf.h"
#include"mac-802_11mr.h"

static class ARFClass : public TclClass {
public:
	ARFClass() : TclClass("RateAdapter/ARF") {}
	TclObject* create(int, const char*const*) {
		return (new ARF());
	}
} class_arf;



ARF::ARF() 
  : nSuccess_(0), nFailed_(0), 
    justTimedOut(false), justIncr(false)
{
  bind("timeout_", &timeout_);
  bind("numSuccToIncrRate_", &numSuccToIncrRate_);
  bind("numFailToDecrRate_", &numFailToDecrRate_);
  bind("numFailToDecrRateAfterTout_", &numFailToDecrRateAfterTout_);
  bind("numFailToDecrRateAfterIncr_", &numFailToDecrRateAfterIncr_);
}

ARF::~ARF()
{
}


int ARF::command(int argc, const char*const* argv)
{
  
  // Add TCL commands here...


  // Fallback for unknown commands
  return RateAdapter::command(argc, argv);
}



void ARF::counterEvent(CounterId id)
{

  if(debug_>3) 
    cerr << "ARF::counterEvent()" << endl;

  switch(id) 
    {

    case ID_MPDUTxSuccessful :    

      TxSuccessfulAction();

      break;

    case ID_ACKFailed:
    case ID_RTSFailed:   
   
      TxFailedAction();

      break;

    default:
      break;
      
    }
}



void ARF::TxSuccessfulAction()
{
  nSuccess_++;
  nFailed_ = 0;
  justTimedOut = 0;
  justIncr = 0;
      
  if(nSuccess_ == numSuccToIncrRate_)
    {	  
      if(incrMode() == 0)
	{
	  resched(timeout_);
	  justIncr = 1;
	}
      nSuccess_ = 0;
    }

}



void ARF::TxFailedAction()
{

  nFailed_++;
  nSuccess_ = 0;

  if ((justIncr && (nFailed_ >= numFailToDecrRateAfterIncr_))
      || (justTimedOut && (nFailed_ >= numFailToDecrRateAfterTout_))
      || (nFailed_ >= numFailToDecrRate_))
    {	  
      if (decrMode() == 0)
	{
	  resched(timeout_);
	}
      nFailed_ = 0;
      justTimedOut = 0;
      justIncr = 0;
    }
}




void ARF::expire(Event *e)
{

  nFailed_ = 0;
  justIncr = 0;
  if(incrMode())
    {
      justIncr = 1;
      resched(timeout_);
    }
  nSuccess_ = 0;
} 


void ARF::chBusy(double)
{
}
