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


#include <iostream>
#include <mac.h>
#include"mac-802_11mr.h"
#include "PER.h"


using namespace std;


#define ABS(x) (((x)>=0) ? (x) : (-x) )



int main(int argc, char *argv[])
{


PER* per = new PER;

 if (per==NULL)
   {
     printf("Memory error!\n");
 fflush(stdout);
     exit(1);
   }

 printf("alive\n");
 fflush(stdout);
 

per->set_per(Mode1Mb,20,1,0.4);
per->set_per(Mode1Mb,20,2,0.2);
per->set_per(Mode1Mb,10,1,0.2);
per->set_per(Mode1Mb,5,1,1);
per->set_per(Mode1Mb,5,1,1);


per->set_per(Mode2Mb,10,7,3);
per->set_per(Mode2Mb,10,6,3);
per->set_per(Mode2Mb,10,5,3);
per->set_per(Mode2Mb,10,4,3);
per->set_per(Mode2Mb,10,3,3);

per->set_per(Mode2Mb,10,2,1);
per->set_per(Mode2Mb,10,1,2);


per->set_per(Mode5_5Mb,10,1,1);
per->set_per(Mode5_5Mb,10,1,2);
per->set_per(Mode5_5Mb,10,1,3);
per->set_per(Mode5_5Mb,10,1,7);
per->set_per(Mode5_5Mb,10,1,6);
per->set_per(Mode5_5Mb,10,1,5);
per->set_per(Mode5_5Mb,10,1,4);


/* Comparing values obtained from PER module 
 * with hand-calculated interpolation 
 * of data specified above
 */

 if (( ABS(per->get_per(Mode1Mb,20,1.5) - 0.3)  > 1e-10)
     && ( ABS(per->get_per(Mode1Mb,20,1.5) - 0.3)  > 1e-10)
     && ( ABS(per->get_per(Mode1Mb,15,1) - 0.3)  > 1e-10)  )
   exit(0);
 else
   exit(1);  /* Test failed! */ 


};
