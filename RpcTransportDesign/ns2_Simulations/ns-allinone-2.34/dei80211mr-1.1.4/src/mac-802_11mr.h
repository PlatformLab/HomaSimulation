/* -*-  Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- 
 *
 * Copyright (c) 1997 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * Ported from CMU/Monarch's code, nov'98 -Padma.
 * wireless-mac-802_11.h
 *
 * Ported 2006 from ns-2.29 by Federico Maguolo, Nicola Baldo and Simone Merlin 
 * (SIGNET lab, University of Padova, Department of Information Engineering)
 *
 */

#ifndef _MAC80211MULTIRATE_
#define _MAC80211MULTIRATE_

#include <mac.h>
#include <cmu-trace.h>
#include <mac-802_11.h>
#include <mac-timersmr.h>
#include <power_profile.h>
#include <peerstats.h>


#include <phymodes.h>
#include <PER.h>
#include <Mac80211EventHandler.h>

#define MAC802_11_HDRLEN (30 + ETHER_FCS_LEN)
#define LONGPREAMBLEDURATION (192e-6)
#define SHORTPREAMBLEDURATION (LONGPREAMBLEDURATION/2)
#define OFDMPREAMBLE	(26e-6)
#define OFDMServiceTail	22
#define OFDMSYMBDURATION 4e-6
#define HDR_MULTIRATE(p)		(MultiRateHdr::access(p))
#define DROP_MAC_SYNC_FAILED "SYN"
#define DROP_MAC_TX_ACTIVE "TXA"
#define DROP_MAC_UNKNOWN_TYPE "UKN"
#define MAX_NUM_STATIONS 500

typedef short int DEI_MacState;


class Mac802_11mr;

class MR_PHY_MIB {
public:
	MR_PHY_MIB() {}
	MR_PHY_MIB(Mac802_11mr *parent);

	inline u_int32_t getCWMin() { return(CWMin); }
        inline u_int32_t getCWMax() { return(CWMax); }
	inline double getSlotTime() { return(SlotTime); }
	inline double getSIFS() { return(SIFSTime); }
	inline double getPIFS() { return(SIFSTime + SlotTime); }
	inline double getDIFS() { return(SIFSTime + 2 * SlotTime); }
	inline u_int32_t getHdrLen11() {
		return(MAC802_11_HDRLEN);
	}
	
	inline u_int32_t getRTSlen() {
		return(sizeof(struct rts_frame));
	}
	
	inline u_int32_t getCTSlen() {
		return(sizeof(struct cts_frame));
	}
	
	inline u_int32_t getACKlen() {
		return(sizeof(struct ack_frame));
	}
	
	inline double getRate(PhyMode mode) {
		switch(mode)
		{
			case Mode1Mb:
				return 1e6;
			case Mode2Mb:
				return 2e6;
			case Mode5_5Mb:
				return 5.5e6;
			case Mode11Mb:
				return 11e6;
			case Mode6Mb:
				return 6e6;
			case Mode9Mb:
				return 9e6;
			case Mode12Mb:
				return 12e6;
			case Mode18Mb:
				return 18e6;
			case Mode24Mb:
				return 24e6;
			case Mode36Mb:
				return 36e6;
			case Mode48Mb:
				return 48e6;
			case Mode54Mb:
				return 54e6;
			default:
				return 0.0;
		}
		
	}
	inline double getNBits(PhyMode mode) {
		switch(mode)
		{
			case Mode6Mb:
				return 24.0;
			case Mode9Mb:
				return 36.0;
			case Mode12Mb:
				return 48.0;
			case Mode18Mb:
				return 72.0;
			case Mode24Mb:
				return 96.0;
			case Mode36Mb:
				return 144.0;
			case Mode48Mb:
				return 192.0;
			case Mode54Mb:
				return 216.0;
			default:
				return 0.0;
		}
		
	}
	
	inline double getPHYhdrDuration(PhyMode mode)
	{
		switch(mode)
		{
			case Mode1Mb:
			case Mode2Mb:
			case Mode5_5Mb:
			case Mode11Mb:
				if(useShortPreamble)
					return SHORTPREAMBLEDURATION;
				else
					return LONGPREAMBLEDURATION;
			case Mode6Mb:
			case Mode9Mb:
			case Mode12Mb:
			case Mode18Mb:
			case Mode24Mb:
			case Mode36Mb:
			case Mode48Mb:
			case Mode54Mb:
				return OFDMPREAMBLE;
			default:
				return 0.0;
		}
		
	}
	
	inline double getEIFS() {
		// If you ACTUALLY look at (802.11-1999, 9.2.10) 
		// you'll see that EIFS is always calculated for 1Mbps mode,
		// regardless of the actual PHY mode which will be used 
		// for the ACK (which cannot be known in advance).
		// Moreover, the 802.11g specs do not introduce any modifications 
		// in EIFS calculation.
		// This might lead to overestimation of the NAV.
		// The specs say overestimation can be corrected by shrinking the NAV
		// upon next correct packet reception (e.g., if we receive the ACK
		// and see that it is shorter than expected).
		return(SIFSTime + getDIFS() + getPHYhdrDuration(Mode1Mb) + (8 *  getACKlen())/getRate(Mode1Mb));
	}

	inline double getSyncInterval(PhyMode mode) {
	  switch(mode)
	    {
			case Mode1Mb:
			case Mode2Mb:
			case Mode5_5Mb:
			case Mode11Mb:
				return(bSyncInterval);
			case Mode6Mb:
			case Mode9Mb:
			case Mode12Mb:
			case Mode18Mb:
			case Mode24Mb:
			case Mode36Mb:
			case Mode48Mb:
			case Mode54Mb:
				return(gSyncInterval);
		}	  
	}

	void setAPSnr(double snr);
	double getAPSnr();

	void setAPAckMode(PhyMode pm) {AckMode_AP = pm;}
	PhyMode getAPAckMode() {return AckMode_AP;}
	

 protected:
	u_int32_t	CWMin;
	u_int32_t	CWMax;
	double		SlotTime;
	double		SIFSTime;
	int		useShortPreamble;	//Preamble indicator for rates of 802.11b
	double		bSyncInterval;  //Time interval in which re-synchronization on a stronger TX is allowed (802.11b)
	double		gSyncInterval;  //Time interval in which re-synchronization on a stronger TX is allowed (802.11g) 
	double          snr_AP;         // SNR of the AP this station is associated with
	PhyMode         AckMode_AP;     // last PHY mode used by the AP for ACK packets
};




class MR_MAC_MIB {
	friend class Mac802_11mr;
	friend class Mac80211Tuning;
	friend class Mac80211EventHandler;
	
public:
	MR_MAC_MIB();
	MR_MAC_MIB(Mac802_11mr *parent);
	
	inline u_int32_t getFrameErrors() {return FrameErrors;}
	inline u_int32_t getDataFrameErrors() {return DataFrameErrors;}
	inline u_int32_t getCtrlFrameErrors() {return CtrlFrameErrors;}
	inline u_int32_t getMgmtFrameErrors() {return MgmtFrameErrors;}

	inline u_int32_t getFrameErrorsNoise() {return FrameErrorsNoise;}
	inline u_int32_t getDataFrameErrorsNoise() {return DataFrameErrorsNoise;}
	inline u_int32_t getCtrlFrameErrorsNoise() {return CtrlFrameErrorsNoise;}
	inline u_int32_t getMgmtFrameErrorsNoise() {return MgmtFrameErrorsNoise;}

	inline u_int32_t getFrameReceives() {return FrameReceives;}
	inline u_int32_t getDataFrameReceives() {return DataFrameReceives;}
	inline u_int32_t getCtrlFrameReceives() {return CtrlFrameReceives;}
	inline u_int32_t getMgmtFrameReceives() {return MgmtFrameReceives;}

	inline u_int32_t getFrameDropSyn() {return FrameDropSyn;}
	inline u_int32_t getDataFrameDropSyn() {return DataFrameDropSyn;}
	inline u_int32_t getCtrlFrameDropSyn() {return CtrlFrameDropSyn;}
	inline u_int32_t getMgmtFrameDropSyn() {return MgmtFrameDropSyn;}


	inline u_int32_t getFrameDropTxa() {return FrameDropTxa;}
	inline u_int32_t getDataFrameDropTxa() {return DataFrameDropTxa;}
	inline u_int32_t getCtrlFrameDropTxa() {return CtrlFrameDropTxa;}
	inline u_int32_t getMgmtFrameDropTxa() {return MgmtFrameDropTxa;}

	inline u_int32_t getACKFailed() {return ACKFailed;}
	inline u_int32_t getRTSFailed() {return RTSFailed;}

	inline u_int32_t getMPDURxSuccessful() {return MPDURxSuccessful;}
	inline u_int32_t getMPDUTxSuccessful() {return MPDUTxSuccessful;}
	inline u_int32_t getMPDUTxOneRetry() {return MPDUTxOneRetry;}
	inline u_int32_t getMPDUTxMultipleRetries() {return MPDUTxMultipleRetries;}
	inline u_int32_t getMPDUTxFailed() {return MPDUTxFailed;}

	inline u_int32_t getidleSlots() {return idleSlots;}
	inline u_int32_t getidle2Slots() {return idle2Slots;}
	
	inline u_int32_t getRTSThreshold() {return RTSThreshold;}
	inline u_int32_t getShortRetryLimit() {return ShortRetryLimit;}
	inline u_int32_t getLongRetryLimit() {return LongRetryLimit;}
	
	inline void setRTSThreshold(u_int32_t newTr) { RTSThreshold = newTr;}
	inline void setShortRetryLimit(u_int32_t newLimit) { ShortRetryLimit = newLimit;}
	inline void setLongRetryLimit(u_int32_t newLimit) { LongRetryLimit = newLimit;}

	virtual double getPerr();
	virtual double getPcoll();


private:
	u_int32_t	RTSThreshold;
	u_int32_t	ShortRetryLimit;
	u_int32_t	LongRetryLimit;




protected: 
	u_int32_t FrameErrors;           /**< total number of frames
					  * sensed on the channel but
					  * not correctly received  
					  */ 
	u_int32_t DataFrameErrors;       /**< number of data frames
					  * sensed on the channel but
					  * not correctly received. 
					  * @note non-standard counter 
					  */ 
	u_int32_t CtrlFrameErrors;	 /**< number of control frames
					  * (ACK, RTS,CTS, CTS) sensed
					  * on the channel but not
					  * correctly received. 
					  * @note non-standard counter   
					  */ 
	u_int32_t MgmtFrameErrors;	 /**< number of management
					  * frames (BEACONS?) sensed
					  * on the channel but not
					  * correctly received. 
					  * @note non-standard counter   
					  */ 

	u_int32_t FrameErrorsNoise;           /**< total number of frames
					  * sensed on the channel but
					  * not correctly received due to noise.
 					  * @note non-standard counter  
					  */ 
	u_int32_t DataFrameErrorsNoise;       /**< number of data frames
					  * sensed on the channel but
					  * not correctly received due to noise. 
					  * @note non-standard counter 
					  */ 
	u_int32_t CtrlFrameErrorsNoise;	 /**< number of control frames
					  * (ACK, RTS,CTS, CTS) sensed
					  * on the channel but not
					  * correctly received due to noise. 
					  * @note non-standard counter   
					  */ 
	u_int32_t MgmtFrameErrorsNoise;	 /**< number of management
					  * frames (BEACONS?) sensed
					  * on the channel but not
					  * correctly received due to noise. 
					  * @note non-standard counter   
					  */ 

	u_int32_t FrameReceives;         /**< total number of correct
					  * frames sensed on the
					  * channel, including frames
					  * not addressed to the
					  * device  
					  */	 
	u_int32_t DataFrameReceives;     /**< number of correct data
					  * frames sensed on the
					  * channel, including frames
					  * not addressed to the
					  * device. 
					  * @note non-standard counter 
					  */ 
	u_int32_t CtrlFrameReceives;     /**< number of correct
					  * control frames sensed on
					  * the channel, including
					  * frames not addressed to
					  * the device.
					  * @note non-standard counter
					  */
	u_int32_t MgmtFrameReceives;     /**< number of correct
					  * management frames sensed
					  * on the channel, including
					  * frames not addressed to
					  * the device.
					  * @note non-standard counter    
					  */

	u_int32_t FrameDropSyn;         /**< total number of 
					 * frames for which reception
					 * is not attempted since the
					 * receiver was synchronized
					 * on another packet
					 * @note non-standard counter 
					 */	 
	u_int32_t DataFrameDropSyn;     /**< number of data
					 * frames for which reception
					 * is not attempted since the
					 * receiver was synchronized
					 * on another packet
					 * @note non-standard counter 
					  */ 
	u_int32_t CtrlFrameDropSyn;     /**< number of control 
					 * frames for which reception
					 * is not attempted since the
					 * receiver was synchronized
					 * on another packet
					 * @note non-standard counter 
					  */
	u_int32_t MgmtFrameDropSyn;     /**< number of management 
					 * frames for which reception
					 * is not attempted since the
					 * receiver was synchronized
					 * on another packet
					 * @note non-standard counter 
					 */

	u_int32_t FrameDropTxa;         /**< total number of 
					 * frames for which reception
					 * is not attempted since the
					 * receiver was synchronized
					 * on another packet
					 * @note non-standard counter 
					 */	 
	u_int32_t DataFrameDropTxa;     /**< number of data
					 * frames for which reception
					 * is not attempted since the
					 * receiver was transmitting
					 * @note non-standard counter 
					  */ 
	u_int32_t CtrlFrameDropTxa;     /**< number of control 
					 * frames for which reception
					 * is not attempted since the
					 * receiver was transmitting
					 * @note non-standard counter 
					  */
	u_int32_t MgmtFrameDropTxa;     /**< number of management 
					 * frames for which reception
					 * is not attempted since the
					 * receiver was transmitting
					 * @note non-standard counter 
					 */






	u_int32_t ACKFailed;             /**< number of frame
					  * transmissions for which an
					  * acknowledgment response
					  * frame was expected but not
					  * received 
					  */ 
	u_int32_t RTSFailed;             /**< number of RTS
					  * transmissions for which a
					  * CTS frame was expected but
					  * not received  
					  */ 
	u_int32_t MPDURxSuccessful;      /**< total number of
					  * correctly received unicast
					  * data packets addressed to
					  * the device  
					  */ 
	u_int32_t MPDUTxSuccessful;      /**< total number of
					  * successfully transmitted
					  * unicast data packets  
					  */ 
	u_int32_t MPDUTxOneRetry;        /**< number of successfully
					  * transmitted data packets
					  * at the first transmission
					  * attempt  
					  */ 
	u_int32_t MPDUTxMultipleRetries; /**< number of transmitted
					  * data packets successful
					  * after more than one
					  * retransmission  
					  */ 
	u_int32_t MPDUTxFailed;          /**< number of dropped data
					  * pkts due to exceeded retry
					  * limit  
					  */
	u_int32_t idleSlots;             /**< number of idle timeslots 
					  */
	u_int32_t idle2Slots;            /**< number of idle timeslots
					  *  (2nd version) 
					  */       
	double startIdleTime;            /**< when the channel will
					  * become free again,
					  * {D,S,E}IFS included 
					  */ 
	double startIdle2Time;           /**< when the channel will
					  * become free again,
					  * {D,S,E}IFS included (2nd
					  * version) 
					  */
	double startIdle2Time_min;       /**< minimum value to which
					  * startIdle2Time can be
					  * rolled back by a forced
					  * update 
					  */
	

	
// [Nicola] Counters below are obsoleted by the new counters introduced above.
// public:
// 	u_int32_t	FailedCount;
// 	u_int32_t	RTSFailureCount;
// 	u_int32_t	ACKFailureCount;
	

 public:
/*       inline u_int32_t getRTSThreshold() { return(RTSThreshold);}
       inline u_int32_t getShortRetryLimit() { return(ShortRetryLimit);}
       inline u_int32_t getLongRetryLimit() { return(LongRetryLimit);}*/
 	void printCounters(int index);
	char* counters2string(char* str, int size);

	int VerboseCounters_;
};

class Mac802_11mr : public Mac {
	friend class Mr_DeferTimer;
	friend class Mr_BackoffTimer;
	friend class Mr_IFTimer;
	friend class Mr_NavTimer;
	friend class Mr_RxTimer;
	friend class Mr_TxTimer;
	friend class Mac80211EventHandler;
	friend class Mac80211Tuning;
public:
	Mac802_11mr();
	virtual void		recv(Packet *p, Handler *h);
	virtual int	hdr_dst(char* hdr, int dst = -2);
	virtual int	hdr_src(char* hdr, int src = -2);
	virtual int	hdr_type(char* hdr, u_int16_t type = 0);
	
	inline int bss_id() { return bss_id_; }
	inline int getIndex() { return index_; }
	
	// Added by Sushmita to support event tracing
        void trace_event(char *, Packet *);
        EventTrace *et_;

	virtual void printMacCounters(void);
	virtual double txtime(int psz, PhyMode mode);

	virtual void updateSnr(u_int32_t src, u_int32_t dst, double snr);
	virtual void updateSnir(u_int32_t src, u_int32_t dst, double snir);
	virtual void updateInterf(u_int32_t src, u_int32_t dst, double interf);

	virtual double getSnr(u_int32_t addr);
	virtual double getSnir(u_int32_t addr);
	virtual double getInterf(u_int32_t addr);

protected:
	virtual void	backoffHandler(void);
	void	deferHandler(void);
	void	navHandler(void);
	void	recvHandler(void);
	void	sendHandler(void);
	void	txHandler(void);

// private:
	int		command(int argc, const char*const* argv);

	/*
	 * Called by the timers.
	 */
	virtual void		recv_timer(void);
	virtual void		send_timer(void);
	int		check_pktCTRL();
	int		check_pktRTS();
	int		check_pktTx();

	/*
	 * Packet Transmission Functions.
	 */
	virtual void	send(Packet *p, Handler *h);
	virtual void 	sendRTS(int dst);
	virtual void	sendCTS(int dst, double duration);
	virtual void	sendACK(int dst);
	virtual void	sendDATA(Packet *p);
	void	RetransmitRTS();
	void	RetransmitDATA();

	/*
	 * Packet Reception Functions.
	 */
	virtual void	recvRTS(Packet *p);
	virtual void	recvCTS(Packet *p);
	virtual void	recvACK(Packet *p);
	virtual void	recvDATA(Packet *p);

/* 	void		capture(Packet *p); */
/* 	void		collision(Packet *p); */
	virtual void	discard(Packet *p, const char* why);
	virtual void	rx_resume(void);
	virtual void	tx_resume(void);

	virtual int	is_idle(void);

	/*
	 * Debugging Functions.
	 */
	void		trace_pkt(Packet *p);
	void		dump(char* fname);

	inline int initialized() {	
		return (cache_ && logtarget_
                        && Mac::initialized());
	}

	inline void mac_log(Packet *p) {
                logtarget_->recv(p, (Handler*) 0);
        }

	double txtime(Packet *p);
	double txtime(int bytes) { /* clobber inherited txtime() */ abort(); return 0;}

	virtual void transmit(Packet *p, double timeout);
	virtual void checkBackoffTimer(void);
	//virtual void postBackoff(int pri);
	virtual void setRxState(DEI_MacState newState);
	virtual void setTxState(DEI_MacState newState);


	inline void inc_cw() {
		cw_ = (cw_ << 1);
		if(cw_ > phymib_.getCWMax())
			cw_ = phymib_.getCWMax();
	}
	inline void rst_cw() { cw_ = phymib_.getCWMin(); }

	inline double sec(double t) { return(t *= 1.0e-6); }
	inline u_int16_t usec(double t) {
		u_int16_t us = (u_int16_t)floor((t *= 1e6) + 0.5);
		return us;
	}
	inline void set_nav(u_int16_t us) {
		set_nav((double) us * 1e-6);
		}
	inline void set_nav(double t) {
		double now = Scheduler::instance().clock();
		if((now + t) > nav_) {
			nav_ = now + t;
			if(mhNav_.busy())
				mhNav_.stop();
			mhNav_.start(t);
		}
	}

protected:
	MR_PHY_MIB         phymib_;
        MR_MAC_MIB         macmib_;

	PeerStatsDB*         peerstatsdb_; /// Database of Peer
					   /// Statistics plugged via TCL
	
	void incFrameErrors(u_int32_t src, u_int8_t  type, int reason);
	void incFrameReceives(u_int32_t src, u_int8_t  type);
	void incFrameDropSyn(u_int32_t src, u_int8_t  type);	
	void incFrameDropTxa(u_int32_t src, u_int8_t  type);	
	void incACKFailed(u_int32_t dst);
	void incRTSFailed(u_int32_t dst);
	void incMPDURxSuccessful(u_int32_t src);
	void incMPDUTxSuccessful(u_int32_t dst);
	void incMPDUTxOneRetry(u_int32_t dst);
	void incMPDUTxMultipleRetries(u_int32_t dst);
	void incMPDUTxFailed(u_int32_t dst);
	void incidleSlots(int step=1);
	void incidle2Slots(int step=1);



	enum {UIT_NORMAL=0, UIT_WEAK, UIT_FORCED};

	/**
	 * updates channel idle time reference. 
	 * 
         * @param nextIdleTime time interval from now till when 
	 * the channel will become free again. Must be strictly positive.
	 *
	 * @param mode 
	 *  - if UIT_NORMAL, idle time reference is 
	 *    either delayed or left untouched. 
	 *  - if UIT_WEAK, do a weak update which can be rolled back up to 
	 *    last normal value by a forced update. Weak updates should be used e.g.
	 *    when nextIdleTime is computed using EIFS.
	 *  - if UIT_FORCED, do a forced update. This is used e.g. to 
	 *    correct NAV overestimation made by EIFS (802.11-1999, sec. 9.2.3.4)
	 *    upon successful reception of a packet. 
	 * 
	 */
	void updateIdleTime(double nextIdleTime, int mode = UIT_NORMAL);


	void addMEH(TclObject* tclobjptr);

	void addMEH(Mac80211EventHandler* mehptr);

	
	Mac80211EventHandler *mehlist_;

       /* the macaddr of my AP in BSS mode; for IBSS mode
        * this is set to a reserved value IBSS_ID - the
        * MAC_BROADCAST reserved value can be used for this
        * purpose
        */
       int     bss_id_;
       enum    {IBSS_ID=MAC_BROADCAST};


// private:
	PhyMode		basicMode_;
 	PhyMode		dataMode_;

	PowerProfile *profile_;
	PER *per_; 
	
	/*
	 * Mac Timers
	 */
	Mr_IFTimer		mhIF_;		// interface timer
	Mr_NavTimer	mhNav_;		// NAV timer
	Mr_RxTimer		mhRecv_;		// incoming packets
	Mr_TxTimer		mhSend_;		// outgoing packets

	Mr_DeferTimer	mhDefer_;	// defer timer
	Mr_BackoffTimer	mhBackoff_;	// backoff timer

	/* ============================================================
	   Internal MAC State
	   ============================================================ */
	double		nav_;		// Network Allocation Vector

	DEI_MacState	rx_state_;	// incoming state (MAC_RECV or MAC_IDLE)
	DEI_MacState	tx_state_;	// outgoint state
	int		tx_active_;	// transmitter is ACTIVE
	double		rx_time;        // time of last transition of rx_state from MAC_IDLE to MAC_RECV

	Packet          *eotPacket_;    // copy for eot callback

	Packet		*pktRTS_;	// outgoing RTS packet
	Packet		*pktCTRL_;	// outgoing non-RTS packet

	u_int32_t	cw_;		// Contention Window
	u_int32_t	ssrc_;		// STA Short Retry Count
	u_int32_t	slrc_;		// STA Long Retry Count

	int		min_frame_len_;

	NsObject*	logtarget_;
	NsObject*       EOTtarget_;     // given a copy of packet at TX end

	
	

	/* ============================================================
	   Duplicate Detection state
	   ============================================================ */
	u_int16_t	sta_seqno_;	// next seqno that I'll use
	int		cache_node_count_;
	Host		*cache_;

};

#endif
