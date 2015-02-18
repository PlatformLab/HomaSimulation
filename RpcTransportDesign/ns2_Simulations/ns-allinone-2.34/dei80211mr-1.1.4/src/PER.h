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

/* -*-	Mode:C++; c-basic-offset:3; tab-width:3; indent-tabs-mode:t -*- */

#ifndef _PER_
#define _PER_

#include <DLList.h>
#include <iostream>
#include <power_profile.h>
#include <packet.h>
#include <stdlib.h>
#include <phymodes.h>


	





// Packet error due to interference from other transmissions
#define  PKT_ERROR_INTERFERENCE 2
// Packet error due to noise only
#define  PKT_ERROR_NOISE 1
// Error-free packet 
#define   PKT_OK 0  

using namespace std;




/**
 * Struct used for output of data found in PER tables
 * 
 */
struct per_set{
int len[4];
double snr[4];
double per[4];
};



/**
 * Packet Error Rate module to be used within NS Mac modules
 * 
 */
class PER : public TclObject {
 public:
  
  
  DLList* PHYmod[NumPhyModes];
  
  PER();
  ~PER();

  /** 
   * Method for issuing commands within TCL
   * 
   * @param argc 
   * @param argv 
   * 
   * @return TCL_OK or TCL_ERROR
   */  
  int command(int argc, const char*const* argv);
  

  /** 
   * Provides the Packet Error Probability for a given packet and PowerProfile
   * 
   * @param pp pointer to the PowerProfile to be used for noise and interference calculations
   * @param pkt pointer to the packet under examination
	* @param snrptr (optional) location where packet SNR (in dB) will be stored
	* @param snirptr (optional) location where packet SNIR (in dB) will be stored
	* @param interfptr (optional) location where the interfering power (in dBW) will be stored
   * 
   * @return the packet error probability
   */
	int get_err(PowerProfile* pp, Packet* pkt, double* snrptr = NULL,  double* snriptr = NULL, double* interfptr = NULL);
  

  /** 
   * Adds a PER value to the inner table in an ascending ordered way
   * 
   * @param phy the PhyMode
   * @param l the packet length in bytes
   * @param snr the signal-to-noise ratio in dB 
   * @param per the corresponding PER value
   */
  void set_per(PhyMode phy, int l , double snr, double per);
  

  /** 
   * This routine take as inputs a PHYmode, a pkt len, 
   * a target SNR and returns the 4 closest entries in the PER table. 
   * Outcomes are set in the per_set struct

   * 
   * @param pm PHYmode
   * @param tsnr target SNR in dB
   * @param tlen pkt len in bytes
   * @param ps_ the output per_set struct. 
   * Each member of this struct is an array of 4 components. 
   * Each component has the following meaning:
   *   - greater or equal length than target one
   *     - [0] greater or equal snr
   *     - [1] smaller snr
   *   - smaller length than target one
   *     - [2] greater or equal snr
   *	 - [3] smaller snr
   *
   * 
   * @return 0 on success, 1 if value is not found
   */
  int get_per(PhyMode pm, double tsnr, double tlen, per_set* ps_);
  


  /** 
   * Returns the packet error rate for the given PhyMode, SNR and packet length.
   * 
   * This method retrieves from the PER table the PER values corresponding to the 
   * nearest greater and smaller approximations of SNR and packet length, 
   * and returns a PER value obtained by means of interpolation.
   * 
   * 
   * @param pm the PhyMode
   * @param tsnr the actual SNR
   * @param tlen the actual packet length
   * 
   * @return the interpolated PER
   */
  double get_per(PhyMode pm, double tsnr, double tlen);
  

  /** 
   * Computes the average power for the given time power profile and packet duration
   * 
   * @param pp the given PowerProfile
   * @param duration the duration of the packet. 
   * This is needed since packet transmission might extend over the time of the last
   * sample contained in the PowerProfile.
   * 
   * @return 
   */
	double avg(PowerProfile* pp, double duration);



  /** 
   * This routine take as inputs a PHYmode, a pkt len, 
   * a target PER and returns the 4 closest entries in the PER table. 
   * Outcomes are set in the per_set struct

   * 
   * @param pm PHYmode
   * @param tper target PER in [0,1]
   * @param tlen pkt len in bytes
   * @param ps_ the output per_set struct. 
   * Each member of this struct is an array of 4 components. 
   * Each component has the following meaning:
   *   - greater or equal length than target one
   *     - [0] smaller PER
   *     - [1] greater or equalPER
   *   - smaller length than target one
   *     - [2] smaller PER
   *	   - [3] greater or equal PER
   *
   * 
   * @return 0 on success, 1 if value is not found
   */
  int get_snr(PhyMode pm, double tper, double tlen, per_set* ps_);



  /** 
   * Returns the SNR for the given PhyMode, PER and packet length.
   * 
   * This method retrieves from the PER table the SNR values corresponding to the 
   * nearest greater and smaller approximations of PER and packet length, 
   * and returns a SNR value obtained by means of interpolation.
   * 
   * 
   * @param pm the PhyMode
   * @param tper the target PER
   * @param tlen the target packet length
   * 
   * @return the interpolated SNR
   */
  double get_snr(PhyMode pm, double tper, double tlen);


void print_data();
 
 protected:

	/// Noise power in W (Bandwidth in Hz times No=kT in W/Hz) to be set via TCL
	double noise_;
 
	/// Debug variable to be set within TCL
	int debug_;
 



};



#endif
