/* -*-	Mode:C++; c-basic-offset:3; tab-width:3; indent-tabs-mode:t -*- */
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



#include"power_profile.h"
#include <scheduler.h>
#include<unistd.h>
#include <assert.h>
#include<stdio.h>




class PowerElementListItem {
  
public:  
	PowerElementListItem();
	PowerElementListItem(PowerElement el);
	~PowerElementListItem();
	PowerElementListItem* next;
	PowerElementListItem* prev;
	PowerElement el;

};
 

PowerElementListItem::PowerElementListItem()
{
	next=NULL;
	prev=NULL;
	el.power=0;
	el.time=0;
}


PowerElementListItem::PowerElementListItem(PowerElement srcel)
{
	next=NULL;
	prev=NULL;
	el = srcel; 
}
 


PowerElementListItem::~PowerElementListItem()
{
	if (next!=NULL) delete (next);	
}

#ifndef COMPILING_TEST_PROGRAM
static class PowerProfileClass : public TclClass {
public:
	PowerProfileClass() : TclClass("PowerProfile") {}
	TclObject* create(int, const char*const*) {
		return (new PowerProfile());
	}
} class_powerprofile;
#endif

  
PowerProfile::PowerProfile() 
{
	statusRecording = false;
	powerListHead = new PowerElementListItem;
	if (powerListHead == NULL)
		{
			printf("FATAL: PowerProfile::PowerProfile(): out of memory\n");
			fflush(stdout);
			exit(1);
		}

	powerListTail = powerListHead;
	numElements = 1;
	lastReadElIndex = 1;
	lastReadElPtr = powerListHead;
	accumulatedPower = 0;
#ifndef COMPILING_TEST_PROGRAM
	bind("debug_", &debug_);
#endif
}
  

PowerProfile::~PowerProfile() 
{
	delete powerListHead;
}


void PowerProfile::startNewRecording()
{
	if (statusRecording==true) {
		printf("WARNING: PowerProfile::startNewRecording() called while recording\n");			
	}

	/* Free whole list (except head element) */
	delete (powerListHead->next);
	powerListHead->next = NULL;
	powerListHead->prev = NULL;
	powerListTail = powerListHead;
	statusRecording = true;
	powerListHead->el.power = accumulatedPower;

	powerListHead->el.time = 0;

	numElements = 1;
	lastReadElIndex = 0;
	lastReadElPtr = powerListHead;
}



void PowerProfile::addElement(PowerElement el)
{

	if(debug_ > 1) printf("\t%f - PowerProfile.addElement - instantpower=%-2.20f el.power=%-2.20f\n", Scheduler::instance().clock(),accumulatedPower, el.power);
	accumulatedPower += el.power;

	/* If very near to zero, round to zero 
	 to try to reduce error propagation */ 
	if (accumulatedPower <= POWERPROFILE_ACC_POWER_ROUND_THRESH && accumulatedPower >= -POWERPROFILE_ACC_POWER_ROUND_THRESH)
		{
			accumulatedPower = 0;
		}

	if (statusRecording)
		{
			double time;

			time = el.time - referenceTime;

			if(debug_) printf("\t%f - PowerProfile.addElement - REC - instantpower=%-2.20f el.power=%-2.20f\n", Scheduler::instance().clock(),accumulatedPower, el.power);
			assert(time >= powerListTail->el.time);

			if (time == powerListTail->el.time)
				{
					/* no need to create a new element */
					powerListTail->el.power += el.power;																
				}			
			else 
				{
					powerListTail->next = new PowerElementListItem(el);
					(powerListTail->next)->prev = powerListTail;
					powerListTail=powerListTail->next;
					powerListTail->el.time = time;
					powerListTail->el.power = accumulatedPower;
					numElements++;
				}
		}

	else /* not recording */
		{
			referenceTime = el.time;
		}

}


void PowerProfile::stopRecording()
{
	if (statusRecording==true) {
		statusRecording = false;
	} else {
		printf("WARNING: PowerProfile::stopRecording() called but recording was already stopped\n");			
	}
}


PowerElement PowerProfile::getElement(int index)
{

	//	printf("Asked for index %d\n",index);

	if ( (index < 0) || (index > numElements-1))
		{
			printf("WARNING: PowerProfile::getElement() has been passed a wrong index %d (numElements:%d)\n",index,numElements);			
			PowerElement ret;
			ret.power = 0;
			ret.time = 0;
			return ret;
		}

	if (statusRecording) 
		{
			printf("WARNING: PowerProfile::getElement() should not be called while recording values\n");			
		}

	/* if everything is correct */


	while (index > lastReadElIndex)
		{
			lastReadElPtr = lastReadElPtr->next;	
			lastReadElIndex++;
			//printf("New index =  %d\n",lastReadElIndex);
			assert(lastReadElPtr != NULL);
		}
	
	while (index < lastReadElIndex)
		{
			lastReadElPtr = lastReadElPtr->prev;	
			lastReadElIndex--;
			//printf("New index =  %d\n",lastReadElIndex);
			assert(lastReadElPtr != NULL);
		}


// 	if (index < lastReadElIndex)
// 		{
// 			/* We must search from head of list */
// 			index = 0; 
// 			lastReadElPtr = powerListHead;
// 			lastReadElIndex = 0;
// 		}

	//lastReadElPtr = powerListHead;
	//lastReadElIndex = 0;

	//	printf("lastReadElIndex =  %d\n",lastReadElIndex);

	/* Now walk the list starting from lastReadEl */
// 	while (lastReadElIndex < index)
// 		{
// 			lastReadElIndex++;
// 			//printf("New index =  %d\n",lastReadElIndex);
// 			lastReadElPtr = lastReadElPtr->next;			
// 		}
	
	return (lastReadElPtr->el);
	

}
