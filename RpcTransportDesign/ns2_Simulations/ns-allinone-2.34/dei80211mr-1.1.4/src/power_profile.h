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



#ifndef POWER_PROFILE_H
#define POWER_PROFILE_H


/* used to avoid long-run errors in the calculus of accumulated power */
#define POWERPROFILE_ACC_POWER_ROUND_THRESH 1e-20


#include <tclcl.h>

/**
 * Used to store the power level associated with a certain time
 * 
 */
struct PowerElement {
	double power;  /**< power level, either absolute or differential depending on the context */
	double time;   /**< time, either absolute or differential depending on the context  */
};

class PowerElementListItem;


#ifndef COMPILING_TEST_PROGRAM
class PowerProfile : public TclObject {
#else
	class PowerProfile {
#endif

public: 
	PowerProfile();
	~PowerProfile();

		/** 
		 * Starts a new PowerProfile recording
		 * 
		 * Deletes all previosly recorded samples and
		 * sets the first sample to the current power level.
		 * 
		 */
	void startNewRecording();

		/** 
		 * Add a new element to the recording. 
		 *
		 * While recording, this method adds a new element to the record. 
		 * While not recording, this method has the only effect of updating the
		 * current accumulated power. 
		 * 
		 * @param el the element to be added. 
		 * Please note that in this case el.power is supposed to be the variation
		 * with the previous power sample (i.e. increment or decrement), while
		 * el.time is supposed to be an absolute time reference.
		 * 
		 */
	void addElement(PowerElement el);


		/** 
		 * Stops the current recording.
		 *
		 * All subseqent calls to addElement() will NOT result in newly recorded 
		 * values, only the accumulated power will be updated. 
		 * 
		 */
	void stopRecording();


		/** 
		 * Returns the number of elements currently recorded within the PowerProfile.
		 * 
		 * 
		 * @return the number of elements
		 */
	inline int getNumElements() {return numElements; }


		/** 
		 * Retrieve the previously recorded PowerElement in the given position
		 * 
		 * @param index the position index of the power elements, 
		 * ranging from 0 to getNumElements() - 1
		 * 
		 * @return the requested PwerElement. 
		 * An empty element (power=0, time=0) is returned if 
		 * the index is not valid
		 */
	PowerElement getElement(int index);
  
protected:
	bool statusRecording;                /**< True if recording, false if not recording */
	PowerElementListItem* powerListHead; /**< Pointer to head of list */
	PowerElementListItem* powerListTail; /**< Pointer to tail of list */
	int numElements;                     /**< Total elements stored in list */

	int lastReadElIndex;                 /**< Index of last element accessed thorugh getElement() */
	PowerElementListItem* lastReadElPtr; /**< Pointer to last elementaccessed thorugh getElement() */

	double referenceTime;                /**< this var holds the value of PowerElement.time in last call to 
													  * addElement() occured with statusRecording==false 
													  */

	double accumulatedPower;             /**< power accumulated through the life 
													  *of this power profile instance 
													  */
		int debug_;                          /**< debug variable to be exported via TCL */

};


#endif /*  POWER_PROFILE_H */

  
