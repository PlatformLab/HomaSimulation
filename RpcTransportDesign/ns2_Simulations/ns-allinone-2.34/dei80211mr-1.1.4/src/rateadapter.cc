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


#include"rateadapter.h"
#include"mac-802_11mr.h"

#include <errno.h>

static class RateAdapterClass : public TclClass {
public:
	RateAdapterClass() : TclClass("RateAdapter") {}
	TclObject* create(int, const char*const*) {
		return (new RateAdapter());
	}
} class_rate_adapter;




RateAdapter::RateAdapter()
  : curr_mode_index(0),
    file(0)
{
  bind("numPhyModes_", &numPhyModes_);  
  bind("debug_", &debug_);

  for (int i=0; i<RA_MAX_NUM_PHY_MODES; i++)
    modes[i] = ModeUnknown;

}


RateAdapter::~RateAdapter()
{
  fclose(file);
}

int RateAdapter::command(int argc, const char*const* argv)
{
  Tcl& tcl = Tcl::instance();

  if(argc == 2)
    {
      if (strcasecmp(argv[1], "getRate") == 0) {
	tcl.resultf("%d", getDataMode() );				                  return TCL_OK;				
 	return TCL_OK;	
      }
    } 			
  else if(argc == 3)
    {    
      if (strcasecmp(argv[1], "setmodeatindex") == 0) {
	int idx = atoi(argv[2]);
	if (debug_) 
	  cerr << " setmodeatindex " << idx << endl ; 
	setModeAtIndex(idx);
	return TCL_OK;
      }

      if (strcasecmp(argv[1], "initlog") == 0) 
	{	  
	  if (!file) 	  
	    file= fopen(argv[2],"a");
	  else 
	    {
	      cerr << "ERROR: RateAdapter::command(initlog, " 
		   << argv[2] << ") : FILE* file already initialized" << endl;
	      return TCL_ERROR;
	    }
	  if (file == NULL)
	    {
	      cerr << "ERROR: RateAdapter::command(initlog, " 
		   << argv[2] << ") cannot open file: " << strerror(errno) << endl;
	      return TCL_ERROR;
	    }
	  return TCL_OK;
	}
    }
  else if(argc == 4)
    {
      if (strcasecmp(argv[1], "phymode") == 0) 
	{
	  PhyMode mode = str2PhyMode(argv[2]);

	  if(mode == ModeUnknown)
	    {
	      cerr << "Error in RateAdapter::command(\"phymode\","<< argv[2] << "," << argv[3] << ") : unknown PHY mode \"" << argv[2]  <<"\"" <<endl;	      
	      return TCL_ERROR;
	    }

	  int index = atoi(argv[3]);
	  if ((index < 0) || (index >= RA_MAX_NUM_PHY_MODES))
	    {
	      cerr << "Error in  RateAdapter::command(\"phymode\","<< argv[2] << "," << argv[3] << ") : phymode index out of bounds" << endl;
	      return TCL_ERROR;
	    }
	  
	  modes[index] = mode;
	  
	  return TCL_OK;	    
	}	  
    }

  // Fallback for unknown commands
  return Mac80211EventHandler::command(argc, argv);
}



int RateAdapter::incrMode()
{
  checkConfiguration();
  
  if (curr_mode_index < (numPhyModes_ - 1))  
    {
      curr_mode_index++;
      setDataMode(modes[curr_mode_index]);

      if (file != 0)
	{
	  fprintf(file," %e %d \n", NOW, getDataMode());
	  fflush(file);         
	}
      
      if (debug_)
	cerr << "RateAdapter: increasing rate to " << PhyMode2str(getCurrMode()) << endl;

      return 0;
    }

  if (debug_)
    cerr << "RateAdapter: cannot increase rate above " << PhyMode2str(getCurrMode()) << endl;

  return 1;
}


int RateAdapter::decrMode()
{
  checkConfiguration();
  
  if (curr_mode_index > 0)
    {
      curr_mode_index--;
      setDataMode(modes[curr_mode_index]);
      
      if (file != 0)
	{
	  fprintf(file," %e %d \n", NOW, getDataMode());
	  fflush(file);         
	}

      if (debug_)
	cerr << "RateAdapter: decreasing rate to " << PhyMode2str(getCurrMode()) << endl;

      return 0;
    }

  if (debug_)
    cerr << "RateAdapter: cannot decrease rate below " << PhyMode2str(getCurrMode()) << endl;

  return 1;
}
 

PhyMode RateAdapter::getCurrMode()
{
  return (modes[curr_mode_index]);
}
 

PhyMode RateAdapter::getModeAtIndex(int idx)
{
  assert(idx >= 0);
  assert(idx <  RA_MAX_NUM_PHY_MODES);
  assert(idx < numPhyModes_);

  return modes[idx];
}


void RateAdapter::setModeAtIndex(int idx)
{
  assert(idx >= 0);
  assert(idx <  RA_MAX_NUM_PHY_MODES);
  assert(idx < numPhyModes_);

  curr_mode_index = idx;
  setDataMode(modes[curr_mode_index]);
  
  if (file != 0)   
    {
      fprintf(file," %e %d %e\n", NOW, getDataMode(), getPhyMib()->getRate(getDataMode()));
    }

}




void RateAdapter::checkConfiguration()
{
  if (numPhyModes_==0)
    {
      cerr << "RateAdapter::checkConfiguration() ERROR: numPhyModes_==0 " << endl
	   << "Did you initialize the PhyModes to be used for rate adaptation?" << endl;
      exit(1);
    }
  
    if (numPhyModes_>= RA_MAX_NUM_PHY_MODES)
    {
      cerr << "RateAdapter::checkConfiguration() ERROR: numPhyModes_ >= RA_MAX_NUM_PHY_MODES" << endl;
      exit(1);
    }

    if (getCurrMode() == ModeUnknown)
    {
      cerr << "RateAdapter::checkConfiguration() ERROR: Current Mode == ModeUnknown" << endl;
      exit(1);
    }


}


