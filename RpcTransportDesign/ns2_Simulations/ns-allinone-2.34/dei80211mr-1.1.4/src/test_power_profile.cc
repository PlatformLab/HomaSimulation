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

#include<stdio.h>
#include <unistd.h>
#include <tcl.h>
#include <otcl.h>
#include <tclcl.h>
#include"power_profile.h"

 

#define NUMELS 50
#define RUNS 20 




// Use these value to check for memory bugs
//#define NUMELS 50000
//#define RUNS 10000
 


int single_test(PowerProfile* pp, double time_offset, int is_first_run)
{

  int d;
  int exitval = 0;
  static double accpower = 0.0;
  double tmppower;

  /* Testing if accumulated power is correct even when not recording */ 
  for (d=-NUMELS;d<0;d++)
    {
      struct PowerElement el;
      el.time = 2*d + time_offset;
      el.power = 1;
      accpower += el.power;
      pp->addElement(el);

    } 

  if ((pp->getNumElements() != 1) && is_first_run)
    {
      exitval = 1;
      printf("getNumElements() should return 1 instead of %d "
	     "if nothing has been recorded\n", pp->getNumElements());
    }

  pp->startNewRecording();


  for (d=0;d<NUMELS/2;d++)
    {
      struct PowerElement el;
      el.time = 2*d + time_offset;
      el.power = 1;
      pp->addElement(el);

    } 

  for (d;d<NUMELS;d++)
    {
      struct PowerElement el;
      el.time =  2*d + time_offset;
      el.power = -1;
      pp->addElement(el);
    } 

  pp->stopRecording();


  /* Checking if recorded values are correct */

  if (pp->getNumElements() != NUMELS+1)
    {
      exitval = 1;
      printf("getNumElements() returned %d instead of %d\n", pp->getNumElements(), NUMELS+1);
    }


  tmppower = pp->getElement(0).power ;


  if (tmppower != accpower)
    {
      exitval = 1;
      printf("Accumulated power not correct at the beginning of the recording!\n"
	     "Reported: %2.10f \t  Expected: %2.10f\n",
	     tmppower, accpower);
    }




  for (d=1;d<(NUMELS/2)+1;d++)
    {
      PowerElement el;
      double etime;
      el = pp->getElement(d);
      etime = 2*d;
      if (el.time != etime)
	{
	  printf("Wrong time for entry %d! \t reported time: %f \t expected time: %f\n",d,el.time,etime);
	  exitval = 1;
	}

      accpower += 1;
      if (el.power != accpower)
	{
	  printf("Wrong power for entry %d! \t reported power: %f \t expected power: %f\n",d,el.power,accpower);
	  exitval = 1;
	}
    } 


  for (d;d<=NUMELS;d++)
    {
      PowerElement el;
      double etime;
      el = pp->getElement(d);
      etime = 2*d;
      if (el.time != etime)
	{
	  printf("Wrong time for entry %d! \t reported time: %f \t expected time: %f\n",d,el.time,etime);
	  exitval = 1;
	}

      accpower -= 1;
      if (el.power != accpower)
	{
	  printf("Wrong power for entry %d! \t reported power: %f \t expected power: %f\n",d,el.power,accpower);
	  exitval = 1;
	}
    } 



  /* Now testing data retrieval in reverse order */



  for (d=NUMELS; d>=(NUMELS/2)+1; d--)
    {
      PowerElement el;
      double etime;
      el = pp->getElement(d);
      etime = 2*d;
      if (el.time != etime)
	{
	  printf("Wrong time for entry %d! \t reported time: %f \t expected time: %f\n",d,el.time,etime);
	  exitval = 1;
	}

      if (el.power != accpower)
	{
	  printf("Wrong power for entry %d! \t reported power: %f \t expected power: %f\n",d,el.power,accpower);
	  exitval = 1;
	}
      accpower += 1;
    } 


  for (d; d>0; d--)
    {
      PowerElement el;
      double etime;
      el = pp->getElement(d);
      etime = 2*d;
      if (el.time != etime)
	{
	  printf("Wrong time for entry %d! \t reported time: %f \t expected time: %f\n",d,el.time,etime);
	  exitval = 1;
	}

      if (el.power != accpower)
	{
	  printf("Wrong power for entry %d! \t reported power: %f \t expected power: %f\n",d,el.power,accpower);
	  exitval = 1;
	}
      accpower -= 1;

    } 


  //  if (!exitval) printf(".");

  /* Setting accumulated tx power for next test iteration */
  accpower =  pp->getElement(NUMELS).power;


  return exitval;
}


int main(int argc, char **argv)
{
  int i;
  double offset;

  PowerProfile* pp = new PowerProfile;


  for(i=0; i<RUNS; i++)
    {
      offset = 4*NUMELS*i + 7*NUMELS;
      if (single_test(pp, offset, i==0) != 0)
	{
	  printf("Test failed at iteration %d\n",i+1);
	  return 1;
	}
    }

  printf("Test succeeded!\n");
  return 0;

  delete pp;

}
