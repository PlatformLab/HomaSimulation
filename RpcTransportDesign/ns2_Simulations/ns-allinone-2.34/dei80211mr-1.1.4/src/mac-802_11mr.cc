/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*-
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
 * Contributions by:
 *   - Mike Holland
 *   - Sushmita
 *
 * Ported 2006 from ns-2.29 
 * by Federico Maguolo, Nicola Baldo and Simone Merlin 
 * (SIGNET lab, University of Padova, Department of Information Engineering)
 *
 */

#include <delay.h>
#include <connector.h>
#include <packet.h>
#include <random.h>
#include <mobilenode.h>

// // #define DEBUG 99

#include <arp.h>
#include <ll.h>
#include <mac.h>
#include <cmu-trace.h>

// Added by Sushmita to support event tracing
#include <agent.h>
#include <basetrace.h>
 
#include "mac-802_11mr.h"
#include "mac-timersmr.h"



PhyMode str2PhyMode(const char* str)
{
	if (strcmp(str, "Mode1Mb") == 0) {
	  return Mode1Mb;
	} else if (strcmp(str, "Mode2Mb") == 0) {
	  return Mode2Mb;
	} else if (strcmp(str, "Mode5_5Mb") == 0) {
	  return Mode5_5Mb;
	} else if (strcmp(str, "Mode11Mb") == 0) {
	  return Mode11Mb;
	} else if (strcmp(str, "Mode6Mb") == 0) {
	  return Mode6Mb;
	} else if (strcmp(str, "Mode9Mb") == 0) {
	  return Mode9Mb;
	} else if (strcmp(str, "Mode12Mb") == 0) {
	  return Mode12Mb;
	} else if (strcmp(str, "Mode18Mb") == 0) {
	  return Mode18Mb;
	} else if (strcmp(str, "Mode24Mb") == 0) {
	  return Mode24Mb;
	} else if (strcmp(str, "Mode36Mb") == 0) {
	  return Mode36Mb;
	} else if (strcmp(str, "Mode48Mb") == 0) {
	  return Mode48Mb;
	} else if (strcmp(str, "Mode54Mb") == 0) {
	  return Mode54Mb;
	} else 	return ModeUnknown;
}


static char* PhyModeStr[NumPhyModes]={"Mode1Mb",
				      "Mode2Mb",
				      "Mode5_5Mb",
				      "Mode11Mb",
				      "Mode6Mb",
				      "Mode9Mb",
				      "Mode12Mb",
				      "Mode18Mb",
				      "Mode24Mb",
				      "Mode36Mb",
				      "Mode48Mb",
				      "Mode54Mb"};

const char* PhyMode2str(PhyMode pm)
{
	if (pm == ModeUnknown)
		return "ModeUnknown";
	else
		return PhyModeStr[pm];
}


/* ======================================================================
   Mac  and Phy MIB Class Functions
   ====================================================================== */

MR_PHY_MIB::MR_PHY_MIB(Mac802_11mr *parent) 
	: snr_AP(1000),
	  AckMode_AP(Mode1Mb)
{
	/*
	 * Bind the phy mib objects.  Note that these will be bound
	 * to Mac/802_11 variables
	 */

	parent->bind("CWMin_", &CWMin);
	parent->bind("CWMax_", &CWMax);
	parent->bind("SlotTime_", &SlotTime);
	parent->bind("SIFS_", &SIFSTime);
	parent->bind_bool("useShortPreamble_",&useShortPreamble);
	parent->bind("bSyncInterval_",&bSyncInterval);
	parent->bind("gSyncInterval_",&gSyncInterval);
}


void MR_PHY_MIB::setAPSnr(double snr)
{
	snr_AP =  snr;
}


double MR_PHY_MIB::getAPSnr()
{
	return snr_AP;
}


MR_MAC_MIB::MR_MAC_MIB() :
	MPDUTxSuccessful(0),MPDUTxOneRetry(0),MPDUTxMultipleRetries(0),
	MPDUTxFailed(0), RTSFailed(0), ACKFailed(0), 
	MPDURxSuccessful(0), FrameReceives(0),  DataFrameReceives(0), CtrlFrameReceives(0), MgmtFrameReceives(0),
	FrameErrors(0), DataFrameErrors(0), CtrlFrameErrors(0), MgmtFrameErrors(0),
	FrameErrorsNoise(0), DataFrameErrorsNoise(0), CtrlFrameErrorsNoise(0), MgmtFrameErrorsNoise(0),	
	FrameDropSyn(0), DataFrameDropSyn(0), CtrlFrameDropSyn(0), MgmtFrameDropSyn(0),
	FrameDropTxa(0), DataFrameDropTxa(0), CtrlFrameDropTxa(0), MgmtFrameDropTxa(0),
	idleSlots(0), idle2Slots(0),
	VerboseCounters_(0)
{
}

/* !!!WARNING!!!  remember that MR_MAC_MIB has TWO constructors when changing base initializers */

MR_MAC_MIB::MR_MAC_MIB(Mac802_11mr *parent) :
	MPDUTxSuccessful(0),MPDUTxOneRetry(0),MPDUTxMultipleRetries(0),
	MPDUTxFailed(0), RTSFailed(0), ACKFailed(0), 
	MPDURxSuccessful(0), FrameReceives(0),  DataFrameReceives(0), CtrlFrameReceives(0), MgmtFrameReceives(0),
	FrameErrors(0), DataFrameErrors(0), CtrlFrameErrors(0), MgmtFrameErrors(0),
	FrameErrorsNoise(0), DataFrameErrorsNoise(0), CtrlFrameErrorsNoise(0), MgmtFrameErrorsNoise(0),	
	FrameDropSyn(0), DataFrameDropSyn(0), CtrlFrameDropSyn(0), MgmtFrameDropSyn(0),
	FrameDropTxa(0), DataFrameDropTxa(0), CtrlFrameDropTxa(0), MgmtFrameDropTxa(0),
	idleSlots(0), idle2Slots(0),
	VerboseCounters_(0)
{
	/*
	 * Bind the phy mib objects.  Note that these will be bound
	 * to Mac/802_11 variables
	 */
	
	parent->bind("RTSThreshold_", &RTSThreshold);
	parent->bind("ShortRetryLimit_", &ShortRetryLimit);
	parent->bind("LongRetryLimit_", &LongRetryLimit);
	parent->bind("VerboseCounters_", &VerboseCounters_);
}


void MR_MAC_MIB::printCounters(int index)
{
	fflush(stdout);

	char str[300];
	counters2string(str,300);
	
	fprintf(stdout,
		"\nMAC counters node %d at %2.9f \t%s",
		index, Scheduler::instance().clock(),str);

	fflush(stdout);

}


char* MR_MAC_MIB::counters2string(char* str, int size) 
{
	// Output conforming to plotlog.m 
// 	snprintf(str,size,"\t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d",
// 		MPDUTxSuccessful,MPDUTxOneRetry,MPDUTxMultipleRetries,MPDUTxFailed,
// 		MPDURxSuccessful, 0, 0, RTSFailed, ACKFailed, 
// 		FrameReceives, FrameErrors, 0, 0, idleSlots, 0, 0, 0, idle2Slots);

	
	char* formatstr;
	
	if (VerboseCounters_)
		formatstr=" TxSucc %5d %5d %5d  TxFail %5d %5d %5d  RXSucc %5d %5d %5d %5d %5d  RXFail %5d %5d %5d %5d  RXFailNoise %5d %5d %5d %5d  SYN %5d %5d %5d %5d  TXA %5d %5d %5d %5d  IdleSlost %8d %8d";
	else
		formatstr="%5d %5d %5d   %5d %5d %5d   %5d %5d %5d %5d %5d   %5d %5d %5d %5d   %5d %5d %5d %5d   %5d %5d %5d %5d   %5d %5d %5d %5d   %8d %8d";
				       
	snprintf(str,size,formatstr,
 /* TX Succ */	   MPDUTxSuccessful,MPDUTxOneRetry,MPDUTxMultipleRetries,  
 /* TX Fail */	   MPDUTxFailed, RTSFailed, ACKFailed, 
 /* RX Succ */     MPDURxSuccessful, FrameReceives, DataFrameReceives, CtrlFrameReceives, MgmtFrameReceives,
/* RX Fail Tot */  FrameErrors, DataFrameErrors, CtrlFrameErrors, MgmtFrameErrors,
/* RX Fail Noise*/ FrameErrorsNoise, DataFrameErrorsNoise, CtrlFrameErrorsNoise, MgmtFrameErrorsNoise,	
/* NRX SYN */	   FrameDropSyn, DataFrameDropSyn, CtrlFrameDropSyn, MgmtFrameDropSyn,	 
/* NRX TXA */	   FrameDropTxa, DataFrameDropTxa, CtrlFrameDropTxa, MgmtFrameDropTxa,		 
/* old, wrong  */  idleSlots, 
/* new, correct */ idle2Slots);

	str[size-1]='\0';
	return str;
}


double MR_MAC_MIB::getPerr()
{
	if (DataFrameErrors == 0)
		return 0;
	else
		return ((double)DataFrameErrorsNoise/(double)DataFrameErrors);
}


double MR_MAC_MIB::getPcoll()
{
	if (DataFrameErrors == 0)
		return 0;
	else
		return ((double)(DataFrameErrors - DataFrameErrorsNoise)
			/ (double) DataFrameErrors);
}


void
Mac802_11mr::checkBackoffTimer()
{
	if(is_idle() && mhBackoff_.paused())
		mhBackoff_.resume(phymib_.getDIFS());
	if(! is_idle() && mhBackoff_.busy() && ! mhBackoff_.paused())
		mhBackoff_.pause();
}

void
Mac802_11mr::transmit(Packet *p, double timeout)
{
	tx_active_ = 1;
	
	if (EOTtarget_) {
		assert (eotPacket_ == NULL);
		eotPacket_ = p->copy();
	}

	/*
	 * If I'm transmitting without doing CS, such as when
	 * sending an ACK, any incoming packet will be "missed"
	 * and hence, must be discarded.
	 */
	if(rx_state_ != MAC_IDLE) {
		//assert(dh->dh_fc.fc_type == MAC_Type_Control);
		//assert(dh->dh_fc.fc_subtype == MAC_Subtype_ACK);
		assert(pktRx_);
		struct hdr_cmn *ch = HDR_CMN(pktRx_);
		ch->error() = 1;        /* force packet discard */
	}

	// This is the minimum time we must wait before attempting another TX
	updateIdleTime(txtime(p) + phymib_.getDIFS() + phymib_.getSlotTime());


	/*
	 * If this is not a broadcast packet, we expect an ACK,
	 * so we cannot TX before an EIFS. We do a weak update since 
	 * we don't know the actual rate that the receiver will use 
	 * for the ACK, but eventually we want to be able to reduce 
	 * this waiting time if we actually receive a shorter ACK.
	 * See 802.11-1999, sec. 9.2.3.4
	 */
	hdr_mac802_11 *mh = HDR_MAC802_11(p);
	u_int32_t dst = ETHER_ADDR(mh->dh_ra);
	if(dst != (u_int32_t)MAC_BROADCAST) {
		updateIdleTime(txtime(p) + phymib_.getEIFS() + phymib_.getSlotTime(), UIT_WEAK);
	}



	/*
	 * pass the packet on the "interface" which will in turn
	 * place the packet on the channel.
	 *
	 * NOTE: a handler is passed along so that the Network
	 *       Interface can distinguish between incoming and
	 *       outgoing packets.
	 */
	downtarget_->recv(p->copy(), this);	
	mhSend_.start(timeout); 
	mhIF_.start(txtime(p));
}
void
Mac802_11mr::setRxState(DEI_MacState newState)
{



	if (!is_idle() && newState==MAC_IDLE){
		macmib_.startIdleTime=Scheduler::instance().clock();
//		printf("start idle %f \n", startIdleTime);
	}	
	if (is_idle() && newState!=MAC_IDLE){
		//macmib_.idleSlots+=int ((Scheduler::instance().clock()-macmib_.startIdleTime)/phymib_.getSlotTime());
		incidleSlots(int ((Scheduler::instance().clock()-macmib_.startIdleTime)/phymib_.getSlotTime())); 
//		printf("end idle %f \n", Scheduler::instance().clock()-startIdleTime);
	}

	rx_state_ = newState;
	if(debug_) printf("%f - %i --- setRxState - change RX state to %i (MAC_IDLE=%i MAC_RECV=%i MAC_COLL=%i)\n",Scheduler::instance().clock(), index_, newState,MAC_IDLE,MAC_RECV,MAC_COLL);
	checkBackoffTimer();
}

void
Mac802_11mr::setTxState(DEI_MacState newState)
{
	
	
	if (!is_idle() && newState==MAC_IDLE){
		macmib_.startIdleTime=Scheduler::instance().clock();
//		printf("start idle %f \n", startIdleTime);
	}
	if (is_idle() && newState!=MAC_IDLE){
		//macmib_.idleSlots+=int ((Scheduler::instance().clock()-macmib_.startIdleTime)/phymib_.getSlotTime());
		incidleSlots( int ((Scheduler::instance().clock()-macmib_.startIdleTime)/phymib_.getSlotTime()));
		//printf("end idle %f \n", Scheduler::instance().clock()-startIdleTime);
//printf("slot time %f\n", phymib_.getSlotTime());	
//fprintf(stdout, "slots %d",idleSlots );
	}

	tx_state_ = newState;
	if(debug_) printf("%f - %i --- setTxState - change TX state to %i (MAC_IDLE=%i MAC_SEND=%i)\n",Scheduler::instance().clock(), index_, newState,MAC_IDLE,MAC_SEND);
	checkBackoffTimer();
}

/* ======================================================================
   TCL Hooks for the simulator
   ====================================================================== */
static class Mac802_11mrClass : public TclClass {
public:
	Mac802_11mrClass() : TclClass("Mac/802_11/Multirate") {}
	TclObject* create(int, const char*const*) {
		return (new Mac802_11mr());
	}
} class_mac802_11mr;

// void Mac802_11mr::bindPhyMode(const char* var, PhyMode* val)
// {
// 		create_instvar(var);
// 		init(new InstVarPhyMode(var, val), var);
// }




Mac802_11mr::Mac802_11mr() : 
	Mac(), phymib_(this), macmib_(this), mhIF_(this), mhNav_(this), 
	mhRecv_(this), mhSend_(this), 
	mhDefer_(this), mhBackoff_(this),
	profile_(0), per_(0), mehlist_(0), peerstatsdb_(0)
{
	
	nav_ = 0.0;
	tx_state_ = rx_state_ = MAC_IDLE;
	macmib_.startIdleTime=Scheduler::instance().clock();
	tx_active_ = 0;
	eotPacket_ = NULL;
	pktRTS_ = 0;
	pktCTRL_ = 0;		
	cw_ = phymib_.getCWMin();
	ssrc_ = slrc_ = 0;
	// Added by Sushmita
        et_ = new EventTrace();
	
	sta_seqno_ = 1;
	cache_ = 0;
	cache_node_count_ = 0;
	
	// chk if basic/data rates are set
	// otherwise use bandwidth_ as default;
/*	bindPhyMode("basicMode_", &basicMode_);
	bindPhyMode("dataMode_", &dataMode_);*/
// 	Tcl& tcl = Tcl::instance();
// 	tcl.evalf("Mac/802_11/Multirate set basicRate_");
// 	if (strcmp(tcl.result(), "0") != 0) 
// 		bind_bw("basicRate_", &basicRate_);
// 	else
// 		basicRate_ = bandwidth_;
// 
// 	tcl.evalf("Mac/802_11/Multirate set dataRate_");
// 	if (strcmp(tcl.result(), "0") != 0) 
// 		bind_bw("dataRate_", &dataRate_);
// 	else
// 		dataRate_ = bandwidth_;

        EOTtarget_ = 0;
       	bss_id_ = IBSS_ID;
	//printf("bssid in constructor %d\n",bss_id_);
}


int
Mac802_11mr::command(int argc, const char*const* argv)
{
  Tcl& tcl = Tcl::instance();
	if (argc == 2) {
		if(strcasecmp(argv[1], "printMacCounters") == 0) {
			printMacCounters();
			return (TCL_OK);
                }else 
		if(strcasecmp(argv[1], "getMacCounters") == 0) {
			char str[300];
			tcl.resultf("%s",macmib_.counters2string(str,300));
			return (TCL_OK);
                }else 
		if(strcasecmp(argv[1], "printOPTCounters") == 0) {
			// Not supported any more!!!
			// nse_->print();
			// return (TCL_OK);
			return (TCL_ERROR);
                }else 
		if(strcasecmp(argv[1], "resetIdleSlots") == 0) {
			macmib_.idleSlots=0;
			macmib_.idle2Slots=0;
			return (TCL_OK);
                }else 
		if (strcasecmp(argv[1], "getAPSnr") == 0) {
			tcl.resultf("%6.3f", phymib_.getAPSnr());
			return TCL_OK;
		}
		 
	}else if (argc == 3) {
		if (strcasecmp(argv[1], "eot-target") == 0) {
			EOTtarget_ = dynamic_cast<NsObject*> (TclObject::lookup(argv[2]));
			if (EOTtarget_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		} else 	if (strcasecmp(argv[1], "powerProfile") == 0) {
			profile_ = dynamic_cast<PowerProfile*> (TclObject::lookup(argv[2]));
			if (profile_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		} else 	if (strcasecmp(argv[1], "per") == 0) {
          		per_ = dynamic_cast<PER *> (TclObject::lookup(argv[2]));
			if (per_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		} else 	if (strcasecmp(argv[1], "PeerStatsDB") == 0) {
			peerstatsdb_ = dynamic_cast<PeerStatsDB *> (TclObject::lookup(argv[2]));
			if (peerstatsdb_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		} else if (strcasecmp(argv[1], "bss_id") == 0) {
			bss_id_ = atoi(argv[2]);
			return TCL_OK;
		} else if (strcasecmp(argv[1], "log-target") == 0) { 
			logtarget_ = dynamic_cast<NsObject*> (TclObject::lookup(argv[2]));
			if(logtarget_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		} else if(strcasecmp(argv[1], "nodes") == 0) {
			if(cache_) return TCL_ERROR;
			cache_node_count_ = atoi(argv[2]);
			cache_ = new Host[cache_node_count_ + 1];
			assert(cache_);
			bzero(cache_, sizeof(Host) * (cache_node_count_+1 ));
			return TCL_OK;
		} else if(strcasecmp(argv[1], "eventtrace") == 0) {
			// command added to support event tracing by Sushmita
                        et_ = (EventTrace *)TclObject::lookup(argv[2]);
                        return (TCL_OK);
		} else if(strcasecmp(argv[1], "dataMode_") == 0) {
			dataMode_ = str2PhyMode(argv[2]);
			if (dataMode_ == ModeUnknown)
				return(TCL_ERROR);
			return (TCL_OK);
                } else if(strcasecmp(argv[1], "basicMode_") == 0) {
			basicMode_ = str2PhyMode(argv[2]);
			if (basicMode_ == ModeUnknown)
				return(TCL_ERROR);
			return (TCL_OK);
                } else if (strcasecmp(argv[1], "eventhandler") == 0) {
			addMEH(TclObject::lookup(argv[2]));
			return TCL_OK;

		} else if (strcasecmp(argv[1], "getSnr") == 0) {
			u_int32_t addr = atoi(argv[2]);
			tcl.resultf("%6.3f", getSnr(addr));
			return TCL_OK;
		}
	}
	return Mac::command(argc, argv);
}



void Mac802_11mr::addMEH(TclObject* tclobjptr) 
{
	Mac80211EventHandler* mehptr;
	mehptr = dynamic_cast<Mac80211EventHandler*> (tclobjptr);
	if (mehptr == 0) 	{
		cerr << "Mac802_11mr::addMEH() ERROR: dynamic cast failed!!!" << endl;
		exit(1);
	}       
	addMEH(mehptr);
}



void Mac802_11mr::addMEH(Mac80211EventHandler* mehptr) 
{
	if (mehlist_ == 0) {
		mehlist_ = mehptr;
	} else {
		Mac80211EventHandler* curr = mehlist_;
		while (curr->next)
			curr = curr->next;
		curr->next = mehptr;
	}	
}



// Added by Sushmita to support event tracing
void Mac802_11mr::trace_event(char *eventtype, Packet *p) 
{
        if (et_ == NULL) return;
        char *wrk = et_->buffer();
        char *nwrk = et_->nbuffer();
	
        //char *src_nodeaddr =
	//       Address::instance().print_nodeaddr(iph->saddr());
        //char *dst_nodeaddr =
        //      Address::instance().print_nodeaddr(iph->daddr());
	
        struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	
        //struct hdr_cmn *ch = HDR_CMN(p);
	
	if(wrk != 0) {
		sprintf(wrk, "E -t "TIME_FORMAT" %s %2x ",
			et_->round(Scheduler::instance().clock()),
                        eventtype,
                        //ETHER_ADDR(dh->dh_sa)
                        ETHER_ADDR(dh->dh_ta)
                        );
        }
        if(nwrk != 0) {
                sprintf(nwrk, "E -t "TIME_FORMAT" %s %2x ",
                        et_->round(Scheduler::instance().clock()),
                        eventtype,
                        //ETHER_ADDR(dh->dh_sa)
                        ETHER_ADDR(dh->dh_ta)
                        );
        }
        et_->dump();
}

void Mac802_11mr::printMacCounters(void) 
{

	if (is_idle()){
		
		//macmib_.idleSlots+=int ((Scheduler::instance().clock()-macmib_.startIdleTime)/phymib_.getSlotTime());
		incidleSlots( int ((Scheduler::instance().clock()-macmib_.startIdleTime)/phymib_.getSlotTime()));
		macmib_.startIdleTime=Scheduler::instance().clock();
	}	
	
	macmib_.printCounters(index_);

	printf("%d \n", dataMode_);
}



/* ======================================================================
   Debugging Routines
   ====================================================================== */
void
Mac802_11mr::trace_pkt(Packet *p) 
{
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	u_int16_t *t = (u_int16_t*) &dh->dh_fc;

	fprintf(stderr, "\t[ %2x %2x %2x %2x ] %x %s %d\n",
		*t, dh->dh_duration,
		 ETHER_ADDR(dh->dh_ra), ETHER_ADDR(dh->dh_ta),
		index_, packet_info.name(ch->ptype()), ch->size());
}

void
Mac802_11mr::dump(char *fname)
{
	fprintf(stderr,
		"\n%s --- (INDEX: %d, time: %2.9f)\n",
		fname, index_, Scheduler::instance().clock());

	fprintf(stderr,
		"\ttx_state_: %x, rx_state_: %x, nav: %2.9f, idle: %d\n",
		tx_state_, rx_state_, nav_, is_idle());

	fprintf(stderr,
		"\tpktTx_: %lx, pktRx_: %lx, pktRTS_: %lx, pktCTRL_: %lx, callback: %lx\n",
		(long) pktTx_, (long) pktRx_, (long) pktRTS_,
		(long) pktCTRL_, (long) callback_);

	fprintf(stderr,
		"\tDefer: %d, Backoff: %d (%d), Recv: %d, Timer: %d Nav: %d\n",
		mhDefer_.busy(), mhBackoff_.busy(), mhBackoff_.paused(),
		mhRecv_.busy(), mhSend_.busy(), mhNav_.busy());
	fprintf(stderr,
		"\tBackoff Expire: %f\n",
		mhBackoff_.expire());
}


/* ======================================================================
   Packet Headers Routines
   ====================================================================== */
int
Mac802_11mr::hdr_dst(char* hdr, int dst )
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;
	
	if(debug_ > 2) printf("%f - %i --- hdr_dst - mark the destination address to %i\n",Scheduler::instance().clock(), index_, dst);
       if (dst > -2) {
               if ((bss_id() == ((int)IBSS_ID)) || (addr() == bss_id()) || dst == MAC_BROADCAST) {
                       /* if I'm AP (2nd condition above!), the dh_3a
                        * is already set by the MAC whilst fwding; if
                        * locally originated pkt, it might make sense
                        * to set the dh_3a to myself here! don't know
                        * how to distinguish between the two here - and
                        * the info is not critical to the dst station
                        * anyway!
                        */
                       STORE4BYTE(&dst, (dh->dh_ra));
               } else {
                       /* in BSS mode, the AP forwards everything;
                        * therefore, the real dest goes in the 3rd
                        * address, and the AP address goes in the
                        * destination address
                        */
                       STORE4BYTE(&bss_id_, (dh->dh_ra));
                       STORE4BYTE(&dst, (dh->dh_3a));
               }
       }


       return (u_int32_t)ETHER_ADDR(dh->dh_ra);
}

int 
Mac802_11mr::hdr_src(char* hdr, int src )
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;
	if(debug_ > 2) printf("%f - %i --- hdr_dst - mark the source address to %i\n",Scheduler::instance().clock(), index_, src);
        if(src > -2)
               STORE4BYTE(&src, (dh->dh_ta));
        return ETHER_ADDR(dh->dh_ta);
}

int 
Mac802_11mr::hdr_type(char* hdr, u_int16_t type)
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;
	if(type)
		STORE2BYTE(&type,(dh->dh_body));
	return GET2BYTE(dh->dh_body);
}


/* ======================================================================
   Misc Routines
   ====================================================================== */
int
Mac802_11mr::is_idle()
{
	if(rx_state_ != MAC_IDLE)
		return 0;
	if(tx_state_ != MAC_IDLE)
		return 0;
	if(nav_ > Scheduler::instance().clock())
		return 0;
	
	return 1;
}

void
Mac802_11mr::discard(Packet *p, const char* why)
{
	hdr_mac802_11* mh = HDR_MAC802_11(p);
	hdr_cmn *ch = HDR_CMN(p);

	/* if the rcvd pkt contains errors, a real MAC layer couldn't
	   necessarily read any data from it, so we just toss it now */
	if(ch->error() != 0) {
		Packet::free(p);
		return;
	}

	switch(mh->dh_fc.fc_type) {
	case MAC_Type_Management:
		drop(p, why);
		return;
	case MAC_Type_Control:
		switch(mh->dh_fc.fc_subtype) {
		case MAC_Subtype_RTS:
			 if((u_int32_t)ETHER_ADDR(mh->dh_ta) ==  (u_int32_t)index_) {
				drop(p, why);
				return;
			}
			/* fall through - if necessary */
		case MAC_Subtype_CTS:
		case MAC_Subtype_ACK:
			if((u_int32_t)ETHER_ADDR(mh->dh_ra) == (u_int32_t)index_) {
				drop(p, why);
				return;
			}
			break;
		default:
			fprintf(stderr, "invalid MAC Control subtype\n");
			exit(1);
		}
		break;
	case MAC_Type_Data:
		switch(mh->dh_fc.fc_subtype) {
		case MAC_Subtype_Data:
			if((u_int32_t)ETHER_ADDR(mh->dh_ra) == \
                           (u_int32_t)index_ ||
                          (u_int32_t)ETHER_ADDR(mh->dh_ta) == \
                           (u_int32_t)index_ ||
                          (u_int32_t)ETHER_ADDR(mh->dh_ra) == MAC_BROADCAST) {
                                drop(p,why);
                                return;
			}
			break;
		default:
			fprintf(stderr, "invalid MAC Data subtype\n");
			exit(1);
		}
		break;
	default:
		fprintf(stderr, "invalid MAC type (%x)\n", mh->dh_fc.fc_type);
		trace_pkt(p);
		exit(1);
	}
	Packet::free(p);
}

// void
// Mac802_11mr::capture(Packet *p)
// {
// 	/*
// 	 * Update the NAV so that this does not screw
// 	 * up carrier sense.
// 	 */	
// 	set_nav(phymib_.getEIFS() + txtime(p));
// 	Packet::free(p);
// }

// void
// Mac802_11mr::collision(Packet *p)
// {
// 	switch(rx_state_) {
// 	case MAC_RECV:
// 		setRxState(MAC_COLL);
// 		/* fall through */
// 	case MAC_COLL:
// 		assert(pktRx_);
// 		assert(mhRecv_.busy());
// 		/*
// 		 *  Since a collision has occurred, figure out
// 		 *  which packet that caused the collision will
// 		 *  "last" the longest.  Make this packet,
// 		 *  pktRx_ and reset the Recv Timer if necessary.
// 		 */
// 		if(txtime(p) > mhRecv_.expire()) {
// 			mhRecv_.stop();
// 			discard(pktRx_, DROP_MAC_COLLISION);
// 			pktRx_ = p;
// 			mhRecv_.start(txtime(pktRx_));
// 		}
// 		else {
// 			discard(p, DROP_MAC_COLLISION);
// 		}
// 		break;
// 	default:
// 		assert(0);
// 	}
// }

void
Mac802_11mr::tx_resume()
{
	double rTime;
	assert(mhSend_.busy() == 0);
	assert(mhDefer_.busy() == 0);

	if(pktCTRL_) {
		/*
		 *  Need to send a CTS or ACK.
		 */
		mhDefer_.start(phymib_.getSIFS());
	} else if(pktRTS_) {
		if (mhBackoff_.busy() == 0) {
			/* Modifica Federico 9/12/2004 */
			/*rTime = (Random::random() % cw_) * phymib_.getSlotTime();
			mhDefer_.start( phymib_.getDIFS() + rTime);*/
		       
			mhBackoff_.start(cw_, is_idle(), phymib_.getDIFS());
			/*
			Fine modifica Federico 9/12/2004
			*/
		}
	} else if(pktTx_) {
		if (mhBackoff_.busy() == 0) {
			hdr_cmn *ch = HDR_CMN(pktTx_);
			struct hdr_mac802_11 *mh = HDR_MAC802_11(pktTx_);
			
			if ((u_int32_t) ch->size() < macmib_.getRTSThreshold()
			    || (u_int32_t) ETHER_ADDR(mh->dh_ra) == MAC_BROADCAST) {
				/* Modifica Federico 9/12/2004 */
				/*rTime = (Random::random() % cw_)
					* phymib_.getSlotTime();
				mhDefer_.start(phymib_.getDIFS() + rTime);*/
			       
			       
				mhBackoff_.start(cw_, is_idle(), phymib_.getDIFS());
			       
			       //Fine modifica Federico 9/12/2004
                        } else {
				mhDefer_.start(phymib_.getSIFS());
                        }
		}
	} else if(callback_) {
		Handler *h = callback_;
		callback_ = 0;
		h->handle((Event*) 0);
	}
	setTxState(MAC_IDLE);
}

void
Mac802_11mr::rx_resume()
{
	assert(pktRx_ == 0);
	assert(mhRecv_.busy() == 0);
	setRxState(MAC_IDLE);
}


/* ======================================================================
   Timer Handler Routines
   ====================================================================== */
void
Mac802_11mr::backoffHandler()
{
	if(pktCTRL_) {
		assert(mhSend_.busy() || mhDefer_.busy());
		return;
	}

	if(check_pktRTS() == 0)
		return;

	if(check_pktTx() == 0)
		return;
}

void
Mac802_11mr::deferHandler()
{
	assert(pktCTRL_ || pktRTS_ || pktTx_);

	if(check_pktCTRL() == 0)
		return;
	assert(mhBackoff_.busy() == 0);
	if(check_pktRTS() == 0)
		return;
	if(check_pktTx() == 0)
		return;
}

void
Mac802_11mr::navHandler()
{
	if(is_idle() && mhBackoff_.paused())
		mhBackoff_.resume(phymib_.getDIFS());

	if (!is_idle() && tx_state_==MAC_IDLE && rx_state_==MAC_IDLE )
		macmib_.startIdleTime=Scheduler::instance().clock();
	

}

void
Mac802_11mr::recvHandler()
{
	recv_timer();
}

void
Mac802_11mr::sendHandler()
{
	send_timer();
}


void
Mac802_11mr::txHandler()
{
	if (EOTtarget_) {
		assert(eotPacket_);
		EOTtarget_->recv(eotPacket_, (Handler *) 0);
		eotPacket_ = NULL;
	}
	if(debug_) printf("%f - %i --- txHandler - end transmission of packet\n",Scheduler::instance().clock(), index_);
	tx_active_ = 0;
}


/* ======================================================================
   The "real" Timer Handler Routines
   ====================================================================== */
void
Mac802_11mr::send_timer()
{
	switch(tx_state_) {
	/*
	 * Sent a RTS, but did not receive a CTS.
	 */
	case MAC_RTS:
		RetransmitRTS();
		break;
	/*
	 * Sent a CTS, but did not receive a DATA packet.
	 */
	case MAC_CTS:
		assert(pktCTRL_);
		Packet::free(pktCTRL_); 
		pktCTRL_ = 0;
		break;
	/*
	 * Sent DATA, but did not receive an ACK packet.
	 */
	case MAC_SEND:
		if(debug_) printf("%f - %i --- send_timer - retransmit data\n",Scheduler::instance().clock(), index_);
		RetransmitDATA();
		break;
	/*
	 * Sent an ACK, and now ready to resume transmission.
	 */
	case MAC_ACK:
		assert(pktCTRL_);
		Packet::free(pktCTRL_); 
		pktCTRL_ = 0;
		break;
	case MAC_IDLE:
		break;
	default:
		assert(0);
	}
	if(debug_) printf("%f - %i --- send_timer - call tx_resume\n",Scheduler::instance().clock(), index_);
	tx_resume();
}


/* ======================================================================
   Outgoing Packet Routines
   ====================================================================== */
int
Mac802_11mr::check_pktCTRL()
{
	struct hdr_mac802_11 *mh;
	double timeout;

	if(pktCTRL_ == 0)
		return -1;
	if(tx_state_ == MAC_CTS || tx_state_ == MAC_ACK)
		return -1;

	mh = HDR_MAC802_11(pktCTRL_);
							  
	switch(mh->dh_fc.fc_subtype) {
	/*
	 *  If the medium is not IDLE, don't send the CTS.
	 */
	case MAC_Subtype_CTS:
		if(!is_idle()) {
			discard(pktCTRL_, DROP_MAC_BUSY); pktCTRL_ = 0;
			return 0;
		}
		setTxState(MAC_CTS);
		/*
		 * timeout:  cts + data tx time calculated by
		 *           adding cts tx time to the cts duration
		 *           minus ack tx time -- this timeout is
		 *           a guess since it is unspecified
		 *           (note: mh->dh_duration == cf->cf_duration)
		 */		
		 timeout = txtime(phymib_.getCTSlen(), basicMode_)
                        + DSSS_MaxPropagationDelay                      // XXX
                        + sec(mh->dh_duration)
                        + DSSS_MaxPropagationDelay                      // XXX
                       - phymib_.getSIFS()
                       - txtime(phymib_.getACKlen(), basicMode_);
		break;
		/*
		 * IEEE 802.11 specs, section 9.2.8
		 * Acknowledments are sent after an SIFS, without regard to
		 * the busy/idle state of the medium.
		 */
	case MAC_Subtype_ACK:		
		if(debug_) printf("%f - %i --- check_pktCTRL - transmit ack\n",Scheduler::instance().clock(), index_);
		setTxState(MAC_ACK);
		timeout = txtime(phymib_.getACKlen(), basicMode_);
		break;
	default:
		fprintf(stderr, "check_pktCTRL:Invalid MAC Control subtype\n");
		exit(1);
	}
	transmit(pktCTRL_, timeout);
	return 0;
}

int
Mac802_11mr::check_pktRTS()
{
	struct hdr_mac802_11 *mh;
	double timeout;

	assert(mhBackoff_.busy() == 0);

	if(pktRTS_ == 0)
 		return -1;
	mh = HDR_MAC802_11(pktRTS_);
	
	/*
	During the backoff stage the basicMode_ could be changed by the rate adaptation algorithm
	*/
	hdr_cmn* ch = HDR_CMN(pktRTS_);
	ch->txtime() = txtime(ch->size(), basicMode_);
	MultiRateHdr *mrh = HDR_MULTIRATE(pktRTS_);
	mrh->mode() = basicMode_;
	struct rts_frame *rf = (struct rts_frame*)pktRTS_->access(hdr_mac::offset_);
	rf->rf_duration = usec(phymib_.getSIFS()
			       + txtime(phymib_.getCTSlen(), basicMode_)
			       + phymib_.getSIFS()
                               + txtime(pktTx_)
			       + phymib_.getSIFS()
			       + txtime(phymib_.getACKlen(), basicMode_));	

 	switch(mh->dh_fc.fc_subtype) {
	case MAC_Subtype_RTS:
		if(! is_idle()) {
			/*
			This happens when we have a simultaneous packet tx and rx and the rx event is dispatched first!!!
			*/
			inc_cw();
			mhBackoff_.start(cw_, is_idle());
			if (debug_) printf("MOOOOOOLTO STRANO!!! C'e' qualcosa che non quadra!!\n");
			return 0;
		}
		setTxState(MAC_RTS);
		timeout = txtime(phymib_.getRTSlen(), basicMode_)
			+ DSSS_MaxPropagationDelay                      // XXX
			+ phymib_.getSIFS()
			+ txtime(phymib_.getCTSlen(), basicMode_)
			+ DSSS_MaxPropagationDelay; /* Should we add a DIFS here??? */
		break;
	default:
		fprintf(stderr, "check_pktRTS:Invalid MAC Control subtype\n");
		exit(1);
	}

	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->chBusy(ch->txtime() + rf->rf_duration * 1e-6);
		mehptr = mehptr->next;
	}

	transmit(pktRTS_, timeout);
  

	return 0;
}

int
Mac802_11mr::check_pktTx()
{
	struct hdr_mac802_11 *mh;
	double timeout;
	
	assert(mhBackoff_.busy() == 0);

	if(pktTx_ == 0)
		return -1;

	mh = HDR_MAC802_11(pktTx_);
	/*
	During the backoff stage the basicMode_ could be changed by the rate adaptation algorithm
	*/
	hdr_cmn* ch = HDR_CMN(pktTx_);
	ch->txtime() = txtime(ch->size(), dataMode_);
	MultiRateHdr *mrh = HDR_MULTIRATE(pktTx_);
	if((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST) {
		/* store data tx time for unicast packets */
		mrh->mode() = dataMode_;
		ch->txtime() = txtime(ch->size(), dataMode_);
		mh->dh_duration = usec(txtime(phymib_.getACKlen(), basicMode_)
				       + phymib_.getSIFS());
	} else {
		/* store data tx time for broadcast packets (see 9.6) */
		mrh->mode() = basicMode_;
		ch->txtime() = txtime(ch->size(), basicMode_);
		mh->dh_duration = 0;
	}

	switch(mh->dh_fc.fc_subtype) {
	case MAC_Subtype_Data:
		if(! is_idle()) {
			/*
			This happens when we have a simultaneous packet tx and rx and the rx event is dispatched first!!!
			*/
			sendRTS(ETHER_ADDR(mh->dh_ra));
			inc_cw();
			mhBackoff_.start(cw_, is_idle());
			if(debug_) printf("(check_pktTx) MOOOOOOLTO STRANO!!! C'e' qualcosa che non quadra!!\n");
			return 0;
		}
		setTxState(MAC_SEND);
		if((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST)
                        timeout = txtime(pktTx_)
                                + DSSS_MaxPropagationDelay              // XXX
                               + phymib_.getSIFS()
                               + txtime(phymib_.getACKlen(), basicMode_)
                               + DSSS_MaxPropagationDelay;             // XXX
		else
			timeout = txtime(pktTx_);
		if(debug_) printf("%f - %i --- check_pktTx - Begin transmission of data (timeout=%f, mh->dh_ra=%i,MAC_BROADCAST=%i, pkttime=%f ack=%f)\n",Scheduler::instance().clock(), index_,timeout, (u_int32_t)ETHER_ADDR(mh->dh_ra), MAC_BROADCAST, txtime(pktTx_), txtime(phymib_.getACKlen(), basicMode_));
		break;
	default:
		fprintf(stderr, "check_pktTx:Invalid MAC Control subtype\n");
		exit(1);
	}

	// Why do we need this test?
	if((u_int32_t) ch->size() < macmib_.getRTSThreshold()) {
		Mac80211EventHandler* mehptr = mehlist_;
		while(mehptr) {
			mehptr->chBusy(ch->txtime() + mh->dh_duration * 1e-6);
			mehptr = mehptr->next;
		}
	}

	transmit(pktTx_, timeout);
	return 0;
}
/*
 * Low-level transmit functions that actually place the packet onto
 * the channel.
 */
void
Mac802_11mr::sendRTS(int dst)
{
	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	MultiRateHdr *mrh = HDR_MULTIRATE(p);
	struct rts_frame *rf = (struct rts_frame*)p->access(hdr_mac::offset_);
	
	assert(pktTx_);
	assert(pktRTS_ == 0);

	/*
	 *  If the size of the packet is larger than the
	 *  RTSThreshold, then perform the RTS/CTS exchange.
	 */
	if( (u_int32_t) HDR_CMN(pktTx_)->size() < macmib_.getRTSThreshold() ||
            (u_int32_t) dst == MAC_BROADCAST) {
		Packet::free(p);
		return;
	}

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = phymib_.getRTSlen();
	ch->iface() = -2;
	ch->error() = 0;

	bzero(rf, MAC_HDR_LEN);

	rf->rf_fc.fc_protocol_version = MAC_ProtocolVersion;
 	rf->rf_fc.fc_type	= MAC_Type_Control;
 	rf->rf_fc.fc_subtype	= MAC_Subtype_RTS;
 	rf->rf_fc.fc_to_ds	= 0;
 	rf->rf_fc.fc_from_ds	= 0;
 	rf->rf_fc.fc_more_frag	= 0;
 	rf->rf_fc.fc_retry	= 0;
 	rf->rf_fc.fc_pwr_mgt	= 0;
 	rf->rf_fc.fc_more_data	= 0;
 	rf->rf_fc.fc_wep	= 0;
 	rf->rf_fc.fc_order	= 0;

	//rf->rf_duration = RTS_DURATION(pktTx_);
	STORE4BYTE(&dst, (rf->rf_ra));
	
	/* store rts tx time */
	mrh->mode() = basicMode_;
 	ch->txtime() = txtime(ch->size(), basicMode_);
	
	STORE4BYTE(&index_, (rf->rf_ta));

	/* calculate rts duration field */	
	rf->rf_duration = usec(phymib_.getSIFS()
			       + txtime(phymib_.getCTSlen(), basicMode_)
			       + phymib_.getSIFS()
                               + txtime(pktTx_)
			       + phymib_.getSIFS()
			       + txtime(phymib_.getACKlen(), basicMode_));
	pktRTS_ = p;
}

void
Mac802_11mr::sendCTS(int dst, double rts_duration)
{
	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	MultiRateHdr *mrh = HDR_MULTIRATE(p);
	struct cts_frame *cf = (struct cts_frame*)p->access(hdr_mac::offset_);
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) p->access(hdr_mac::offset_);

	assert(pktCTRL_ == 0);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = phymib_.getCTSlen();


	ch->iface() = -2;
	ch->error() = 0;
	//ch->direction() = hdr_cmn::DOWN;
	bzero(cf, MAC_HDR_LEN);

	cf->cf_fc.fc_protocol_version = MAC_ProtocolVersion;
	cf->cf_fc.fc_type	= MAC_Type_Control;
	cf->cf_fc.fc_subtype	= MAC_Subtype_CTS;
 	cf->cf_fc.fc_to_ds	= 0;
 	cf->cf_fc.fc_from_ds	= 0;
 	cf->cf_fc.fc_more_frag	= 0;
 	cf->cf_fc.fc_retry	= 0;
 	cf->cf_fc.fc_pwr_mgt	= 0;
 	cf->cf_fc.fc_more_data	= 0;
 	cf->cf_fc.fc_wep	= 0;
 	cf->cf_fc.fc_order	= 0;
	
	//cf->cf_duration = CTS_DURATION(rts_duration);
	STORE4BYTE(&dst, (cf->cf_ra));

	// store transmitter address. 
	// Not part of the standard, included for tracing and statistical purposes.
	// This procedure overwrites the cf_fcs field, which is anyway unused.
	STORE4BYTE(&index_, (dh->dh_ta));
	
	/* store cts tx time */
	mrh->mode() = basicMode_;
	ch->txtime() = txtime(ch->size(), basicMode_);
	
	/* calculate cts duration */
	cf->cf_duration = usec(sec(rts_duration)
                              - phymib_.getSIFS()
                              - txtime(phymib_.getCTSlen(), basicMode_));


	
	pktCTRL_ = p;
	
}

void
Mac802_11mr::sendACK(int dst)
{
	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	MultiRateHdr *mrh = HDR_MULTIRATE(p);
	struct ack_frame *af = (struct ack_frame*)p->access(hdr_mac::offset_);
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) p->access(hdr_mac::offset_);


	assert(pktCTRL_ == 0);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	// CHANGE WRT Mike's code
	ch->size() = phymib_.getACKlen();
	ch->iface() = -2;
	ch->error() = 0;
	
	bzero(af, MAC_HDR_LEN);

	af->af_fc.fc_protocol_version = MAC_ProtocolVersion;
 	af->af_fc.fc_type	= MAC_Type_Control;
 	af->af_fc.fc_subtype	= MAC_Subtype_ACK;
 	af->af_fc.fc_to_ds	= 0;
 	af->af_fc.fc_from_ds	= 0;
 	af->af_fc.fc_more_frag	= 0;
 	af->af_fc.fc_retry	= 0;
 	af->af_fc.fc_pwr_mgt	= 0;
 	af->af_fc.fc_more_data	= 0;
 	af->af_fc.fc_wep	= 0;
 	af->af_fc.fc_order	= 0;

	//af->af_duration = ACK_DURATION();
	STORE4BYTE(&dst, (af->af_ra));

	// store transmitter address. 
	// Not part of the standard, included for tracing and statistical purposes.
	// This procedure overwrites the af_fcs field, which is anyway unused.
	STORE4BYTE(&index_, (dh->dh_ta));

	/* store ack tx time */
	mrh->mode() = basicMode_;
 	ch->txtime() = txtime(ch->size(), basicMode_);
	
	/* calculate ack duration */
 	af->af_duration = 0;	
	
	pktCTRL_ = p;
}

void
Mac802_11mr::sendDATA(Packet *p)
{
	hdr_cmn* ch = HDR_CMN(p);
	MultiRateHdr *mrh = HDR_MULTIRATE(p);
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);

	assert(pktTx_ == 0);

	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->sendDATA(p);
		mehptr = mehptr->next;
	}

	/*
	 * Update the MAC header
	 */
	ch->size() += phymib_.getHdrLen11();

	dh->dh_fc.fc_protocol_version = MAC_ProtocolVersion;
	dh->dh_fc.fc_type       = MAC_Type_Data;
	dh->dh_fc.fc_subtype    = MAC_Subtype_Data;
	
	dh->dh_fc.fc_to_ds      = 0;
	dh->dh_fc.fc_from_ds    = 0;
	dh->dh_fc.fc_more_frag  = 0;
	dh->dh_fc.fc_retry      = 0;
	dh->dh_fc.fc_pwr_mgt    = 0;
	dh->dh_fc.fc_more_data  = 0;
	dh->dh_fc.fc_wep        = 0;
	dh->dh_fc.fc_order      = 0;

	/* store data tx time */
 	//ch->txtime() = txtime(ch->size(), dataMode_);

	if((u_int32_t)ETHER_ADDR(dh->dh_ra) != MAC_BROADCAST) {
		/* store data tx time for unicast packets */
		mrh->mode() = dataMode_;
		ch->txtime() = txtime(ch->size(), dataMode_);
		
		dh->dh_duration = usec(txtime(phymib_.getACKlen(), basicMode_)
				       + phymib_.getSIFS());



	} else {
		/* store data tx time for broadcast packets (see 9.6) */
		mrh->mode() = basicMode_;
		ch->txtime() = txtime(ch->size(), basicMode_);
		
		dh->dh_duration = 0;
	}
	pktTx_ = p;

}

/* ======================================================================
   Retransmission Routines
   ====================================================================== */
void
Mac802_11mr::RetransmitRTS()
{
	assert(pktTx_);
	assert(pktRTS_);
	assert(mhBackoff_.busy() == 0);


	struct hdr_mac802_11 *mh = HDR_MAC802_11(pktTx_);
	u_int32_t dst = (u_int32_t)ETHER_ADDR(mh->dh_ra);

	incRTSFailed(dst);


	ssrc_ += 1;			// STA Short Retry Count
		
	if(ssrc_ >= macmib_.getShortRetryLimit()) {
		discard(pktRTS_, DROP_MAC_RETRY_COUNT_EXCEEDED); pktRTS_ = 0;
		/* tell the callback the send operation failed 
		   before discarding the packet */
		hdr_cmn *ch = HDR_CMN(pktTx_);
		if (ch->xmit_failure_) {
                        /*
                         *  Need to remove the MAC header so that 
                         *  re-cycled packets don't keep getting
                         *  bigger.
                         */
			ch->size() -= phymib_.getHdrLen11();
                        ch->xmit_reason_ = XMIT_REASON_RTS;
                        ch->xmit_failure_(pktTx_->copy(),
                                          ch->xmit_failure_data_);
                }
		discard(pktTx_, DROP_MAC_RETRY_COUNT_EXCEEDED); 
		pktTx_ = 0;
		ssrc_ = 0;
		incMPDUTxFailed(dst);
		rst_cw();
	} else {
		struct rts_frame *rf;
		rf = (struct rts_frame*)pktRTS_->access(hdr_mac::offset_);
		rf->rf_fc.fc_retry = 1;

		inc_cw();
		//mhBackoff_.start(cw_, is_idle());
		mhBackoff_.start(cw_, 0);
		if(is_idle())
			mhBackoff_.resume(0.0);
	}
}

void
Mac802_11mr::RetransmitDATA()
{
	struct hdr_cmn *ch;
	struct hdr_mac802_11 *mh;
	u_int32_t *rcount, thresh;
	assert(mhBackoff_.busy() == 0);

	assert(pktTx_);
	assert(pktRTS_ == 0);

	ch = HDR_CMN(pktTx_);
	mh = HDR_MAC802_11(pktTx_);
	u_int32_t dst = (u_int32_t)ETHER_ADDR(mh->dh_ra);

	/*
	 *  Broadcast packets don't get ACKed and therefore
	 *  are never retransmitted.
	 */
	if(dst == MAC_BROADCAST) {
		Packet::free(pktTx_); 
		pktTx_ = 0;

		/*
		 * Backoff at end of TX.
		 */
		rst_cw();
// 		mhBackoff_.start(cw_, is_idle());
		mhBackoff_.start(cw_, 0);
		if(is_idle())
			mhBackoff_.resume(0.0);

		return;
	}

	//macmib_.ACKFailed++;
	incACKFailed(dst);

	if((u_int32_t) ch->size() <= macmib_.getRTSThreshold()) {
                rcount = &ssrc_;
               thresh = macmib_.getShortRetryLimit();
        } else {
                rcount = &slrc_;
               thresh = macmib_.getLongRetryLimit();
        }

	(*rcount)++;

	if(*rcount >= thresh) {
		/* IEEE Spec section 9.2.3.5 says this should be greater than
		   or equal */
		//macmib_.MPDUTxFailed++;
		incMPDUTxFailed(dst);
		/* tell the callback the send operation failed 
		   before discarding the packet */
		hdr_cmn *ch = HDR_CMN(pktTx_);
		if (ch->xmit_failure_) {
                        ch->size() -= phymib_.getHdrLen11();
			ch->xmit_reason_ = XMIT_REASON_ACK;
                        ch->xmit_failure_(pktTx_->copy(),
                                          ch->xmit_failure_data_);
                }

		discard(pktTx_, DROP_MAC_RETRY_COUNT_EXCEEDED); 
		pktTx_ = 0;
		*rcount = 0;
		rst_cw();
	}
	else {

		mh->dh_fc.fc_retry = 1;

		sendRTS(dst);
		inc_cw();
		//mhBackoff_.start(cw_, is_idle());
		mhBackoff_.start(cw_, 0);
		if(is_idle())
			mhBackoff_.resume(0.0);
	}
}

/* ======================================================================
   Incoming Packet Routines
   ====================================================================== */
void
Mac802_11mr::send(Packet *p, Handler *h)
{
	double rTime;
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);

/*	EnergyModel *em = netif_->node()->energy_model();
	if (em && em->sleep()) {
		em->set_node_sleep(0);
		em->set_node_state(EnergyModel::INROUTE);
	}*/
	
	callback_ = h;
	sendDATA(p);
	sendRTS(ETHER_ADDR(dh->dh_ra));

	/*
	 * Assign the data packet a sequence number.
	 */
	dh->dh_scontrol = sta_seqno_++;

	/*
	 *  If the medium is IDLE, we must wait for a DIFS
	 *  Space before transmitting.
	 */
	if(mhBackoff_.busy() == 0) {
		mhBackoff_.start(cw_, is_idle(), phymib_.getDIFS());
	}
}

void
Mac802_11mr::recv(Packet *p, Handler *h)
{
	struct hdr_cmn *hdr = HDR_CMN(p);

	/*
	 * Sanity Check
	 */
	assert(initialized());

	/*
	 *  Handle outgoing packets.
	 */
	if(hdr->direction() == hdr_cmn::DOWN) {
                send(p, h);
                return;
        }
	/*
	 *  Handle incoming packets.
	 *
	 *  We just received the 1st bit of a packet on the network
	 *  interface.
	 *
	 */

	// we don't know which type of packet it is 
	// and if it will be received correctly,
	// so we account only for its duration + DIFS
	updateIdleTime(txtime(p) + phymib_.getDIFS() + phymib_.getSlotTime());

	/*
	 *  If the interface is currently in transmit mode, then
	 *  it probably won't even see this packet.  However, the
	 *  "air" around me is BUSY so I need to let the packet
	 *  proceed.  Just set the error flag in the common header
	 *  to that the packet gets thrown away.
	 */
// 	if(tx_active_ && hdr->error() == 0) {
// 		hdr->error() = 1;
// 	}

	
	/**
	 * If the interface is currently in transmit mode, then
	 * it probably won't even see this packet, and we can discard it.
	 * We have to set the NAV accordingly because the air will possibly
	 * be busy even when transmission by current node ends.
	 */
	if(tx_active_) {
		set_nav(txtime(p));
		hdr_mac802_11 *mh = HDR_MAC802_11(p);
		u_int32_t src = ETHER_ADDR(mh->dh_ta); 
		u_int8_t  type = mh->dh_fc.fc_type;
		incFrameDropTxa(src, type);
		discard(p, DROP_MAC_TX_ACTIVE);
		return;
	}

	
	if(debug_) printf("%f - %i --- recv - received a packet(error=%i,rx_state=%i MAC_IDLE=%i, id=%i)\n",\
			  Scheduler::instance().clock(), index_, hdr->error(), rx_state_,MAC_IDLE,hdr->uid());

	if(rx_state_ == MAC_IDLE) {
		rx_time = Scheduler::instance().clock();
		setRxState(MAC_RECV);
		pktRx_ = p;
		/*
		 * Schedule the reception of this packet, in
		 * txtime seconds.
		 */
		mhRecv_.start(txtime(p));
		assert(profile_);
		profile_->startNewRecording();

	} else {
		
		double si = phymib_.getSyncInterval(HDR_MULTIRATE(pktRx_)->mode_);

		/* this sanity check has almost no sense, 
		   since 802.11b and g packets can coexist */
// 		if (si != phymib_.getSyncInterval(HDR_MULTIRATE(p->mode_))
// 			printf("WARNING: two colliding packets have different SyncInterval\n" );
			       

		/* 
		 * if the starting time of the new packet is within a SyncInterval 
		 * of the first packet we synchronized onto, it is possible to 
		 * re-sync on the newer packet, if it's stronger.
		 */

		if ((Scheduler::instance().clock() - rx_time) < si ) {


			if(p->txinfo_.RxPr > pktRx_->txinfo_.RxPr ) {				

				/* make p the new pktRx 
				 * and reschedule the recv_timer accordigly  */ 

				Packet* old;
				
				old = pktRx_;
				pktRx_ = p;
				p = old;

				mhRecv_.stop();
				mhRecv_.start(txtime(pktRx_));

				if(debug_) printf("%f - %i --- recv_timer - Synchronizing on a stronger packet\n",
						  Scheduler::instance().clock(), index_);
			

			}

		} 

	       /* The receiver stays syncronized on pktRx,
		* and packet p is discarded.
		*/

		if(debug_) printf("%f - %i --- recv_timer - dropping packet because synchronized on another packet\n",
						  Scheduler::instance().clock(), index_);

		set_nav(txtime(p));

		hdr_mac802_11 *mh = HDR_MAC802_11(p);
		u_int32_t src = ETHER_ADDR(mh->dh_ta); 
		u_int8_t  type = mh->dh_fc.fc_type;
		incFrameDropSyn(src, type);			
		discard(p, DROP_MAC_SYNC_FAILED);
	}
}
	


void
Mac802_11mr::recv_timer()
{
	hdr_cmn *ch = HDR_CMN(pktRx_);
	hdr_mac802_11 *mh = HDR_MAC802_11(pktRx_);
	u_int32_t src = ETHER_ADDR(mh->dh_ta); 
	u_int32_t dst = ETHER_ADDR(mh->dh_ra);
	
	u_int8_t  type = mh->dh_fc.fc_type;
	u_int8_t  subtype = mh->dh_fc.fc_subtype;

	assert(pktRx_);
	assert(rx_state_ == MAC_RECV || rx_state_ == MAC_COLL);
	
	profile_->stopRecording();

	if(debug_) printf("%f - %i --- recv_timer - tx_active=%i error=%i per_=%p\n",Scheduler::instance().clock(), index_, tx_active_, ch->error(), per_);



	/*
	 * This block appears to be useless.
	 * 1) if tx_active was true upon recv() then packet had been discarded at that time 
	 *    and no recv_timer should have been scheduled.
	 * 2) if tx_active was false upon recv(), it means that a transmit() had occurred afterwards
	 *    (this happens when sending a packet without CS, e.g., when sending an ACK)
	 *    but in that case ch->error is set by Mac802_11mr::transmit() 
         */
        if(tx_active_ && !ch->error()) {
		if (debug_) {
			fprintf(stderr,"%f - STRANGE!!! (node %d) tx_active==true && ch->error == 0 in mac-802_11mr::recv_timer()\n",Scheduler::instance().clock(),index_);
			dump("Mac802_11mr::recv_timer()");
			fprintf(stderr,"pktTx_:");
			trace_pkt(pktTx_);
			fprintf(stderr,"pktRx_:");
			trace_pkt(pktRx_);
		}
		Packet::free(pktRx_);
		assert(0);
                goto done;
        }


	/*	
         * If the interface enters TRANSMIT mode between recv() and recv_timer()
	 * of this packet (this happens when sending a packet without CS, e.g., when sending an ACK)
	 * ch->error is set by Mac802_11mr::transmit() since this packet cannot be received.
	 * We do a silent discard without adjusting the NAV.	 
	 */
	if( ch->error() ) {
		Packet::free(pktRx_);
		//set_nav(phymib_.getEIFS(BasicMode_));
		updateIdleTime(phymib_.getEIFS() + phymib_.getSlotTime(), UIT_WEAK);
		goto done;
	}


	if (per_) {
		/* 
		* Check for packet errors
		*/
		double snr;
		double snir;
		double interf;
		switch(per_->get_err(profile_, pktRx_, &snr, &snir, &interf)) 
		{
	
			case PKT_ERROR_INTERFERENCE:
				if(debug_) printf("%f - %i --- recv_timer - drop for interference (COLLISION)\n",Scheduler::instance().clock(), index_);
				discard(pktRx_, DROP_MAC_COLLISION);		
				//set_nav(phymib_.getEIFS(BasicMode_));
				updateIdleTime(phymib_.getEIFS() + phymib_.getSlotTime(), UIT_WEAK);
				incFrameErrors(src, type, PKT_ERROR_INTERFERENCE);
				goto done;
				break;
		
			case PKT_ERROR_NOISE:
				if(debug_) printf("%f - %i --- recv_timer - drop for noise (ERR)\n",Scheduler::instance().clock(), index_);
				discard(pktRx_, DROP_MAC_PACKET_ERROR);		
				//set_nav(phymib_.getEIFS(BasicMode_));
				updateIdleTime(phymib_.getEIFS() + phymib_.getSlotTime(), UIT_WEAK);
				incFrameErrors(src, type, PKT_ERROR_NOISE);
				goto done;
				break;
		
			case PKT_OK:
				incFrameReceives(src, type);
				break;
		
			default:
				assert(0);
				break;
		}

		updateSnr(src, dst, snr);
		updateSnir(src, dst, snir);
		updateInterf(src, dst, interf);

	}	    


	/*
	 * IEEE 802.11 specs, section 9.2.5.6
	 *	- update the NAV (Network Allocation Vector)
	 */
	if(dst != (u_int32_t)index_) {
		set_nav(mh->dh_duration);

		// here NAV can be shrunk. See 802.11-1999, sec. 9.2.3.4
		updateIdleTime(sec(mh->dh_duration) + phymib_.getDIFS() + phymib_.getSlotTime(), UIT_FORCED);
	}

        /* tap out - */
        if (tap_ && type == MAC_Type_Data &&
            MAC_Subtype_Data == subtype ) 
		tap_->tap(pktRx_);
	/*
	 * Adaptive Fidelity Algorithm Support - neighborhood infomation 
	 * collection
	 *
	 * Hacking: Before filter the packet, log the neighbor node
	 * I can hear the packet, the src is my neighbor
	 */

	/* Beyond sanity checking, 
	 * this will enable execution in standard NS 
	 * and prevent it in NS-Miracle 
	 */	
	if (netif_ 
	    && netif_->node()
	    && netif_->node()->energy_model()  
	    && netif_->node()->energy_model()->adaptivefidelity()) {		
		netif_->node()->energy_model()->add_neighbor(src);
	}	

	/*
	 * Address Filtering
	 */
	if(dst != (u_int32_t)index_ && dst != MAC_BROADCAST) {
		/*
		 *  We don't want to log this event, so we just free
		 *  the packet instead of calling the drop routine.
		 */
		if(debug_) printf("%f - %i --- recv_timer - the packet is not for me!!\n",Scheduler::instance().clock(), index_);
		//discard(pktRx_, "---");
		Packet::free(pktRx_);
		goto done;
	}
	if(debug_) printf("%f - %i --- recv_timer - the packet is for me (ra=%i, pktid=%i)!!\n",Scheduler::instance().clock(), index_, dst, ch->uid());

	switch(type) {

	case MAC_Type_Management:
		discard(pktRx_, DROP_MAC_UNKNOWN_TYPE);
		goto done;
	case MAC_Type_Control:
		switch(subtype) {
		case MAC_Subtype_RTS:
			recvRTS(pktRx_);
			break;
		case MAC_Subtype_CTS:
			recvCTS(pktRx_);
			break;
		case MAC_Subtype_ACK:
			if(debug_) printf("%f - %i --- recv_timer - received an ACK!!\n",Scheduler::instance().clock(), index_);
			recvACK(pktRx_);
			break;
		default:
			fprintf(stderr,"recvTimer1:Invalid MAC Control Subtype %x\n",
				subtype);
			exit(1);
		}
		break;
	case MAC_Type_Data:
		switch(subtype) {
		case MAC_Subtype_Data:
			incMPDURxSuccessful(src);
			if(debug_) printf("%f - %i --- recv_timer - received a data packet!!\n",Scheduler::instance().clock(), index_);
			recvDATA(pktRx_);
			break;
		default:
			fprintf(stderr, "recv_timer2:Invalid MAC Data Subtype %x\n",
				subtype);
			exit(1);
		}
		break;
	default:
		fprintf(stderr, "recv_timer3:Invalid MAC Type %x\n", subtype);
		exit(1);
	}
 done:
	pktRx_ = 0;
	if(debug_) printf("%f - %i --- recv_timer - call rx_resume\n",Scheduler::instance().clock(), index_);
	rx_resume();
}


void
Mac802_11mr::recvRTS(Packet *p)
{
	struct rts_frame *rf = (struct rts_frame*)p->access(hdr_mac::offset_);

	if(tx_state_ != MAC_IDLE) {
		discard(p, DROP_MAC_BUSY);
		return;
	}

	/*
	 *  If I'm responding to someone else, discard this RTS.
	 */
	if(pktCTRL_) {
		discard(p, DROP_MAC_BUSY);
		return;
	}

	sendCTS(ETHER_ADDR(rf->rf_ta), rf->rf_duration);

	/*
	 *  Stop deferring - will be reset in tx_resume().
	 */
	if(mhDefer_.busy()) mhDefer_.stop();

	tx_resume();

	mac_log(p);
}

/*
 * txtime()	- pluck the precomputed tx time from the packet header
 */
double
Mac802_11mr::txtime(Packet *p)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	double t = ch->txtime();
	if (t < 0.0) {
		drop(p, "XXX");
 		exit(1);
	}
	return t;
}

 
/*
 * txtime()	- calculate tx time for packet of size "psz" bytes 
 *		  at rate "drt" bps
 */
double
Mac802_11mr::txtime(int psz, PhyMode mode)
{
/*	double dsz = psz - phymib_.getPLCPhdrLen();
        int plcp_hdr = phymib_.getPLCPhdrLen() << 3;	
	int datalen = (int)dsz << 3;
	double t = (((double)plcp_hdr)/phymib_.getPLCPDataRate())
                                       + (((double)datalen)/drt);*/
	switch(mode)
	{
		case Mode1Mb:
		case Mode2Mb:
		case Mode5_5Mb:
		case Mode11Mb:
			return(phymib_.getPHYhdrDuration(mode) + (8 *  (double)psz)/phymib_.getRate(mode));
		case Mode6Mb:
		case Mode9Mb:
		case Mode12Mb:
		case Mode18Mb:
		case Mode24Mb:
		case Mode36Mb:
		case Mode48Mb:
		case Mode54Mb:
			return(phymib_.getPHYhdrDuration(mode) + ceil((OFDMServiceTail+(double)psz*8)/phymib_.getNBits(mode))*OFDMSYMBDURATION);
		default:
			fprintf(stderr,"Unknown PHY mode (%i)!!!\n", mode);
			exit(1);
	}
}



void
Mac802_11mr::recvCTS(Packet *p)
{
	if(tx_state_ != MAC_RTS) {
		discard(p, DROP_MAC_INVALID_STATE);
		return;
	}

	assert(pktRTS_);
	Packet::free(pktRTS_); pktRTS_ = 0;

	assert(pktTx_);	
	mhSend_.stop();

	/*
	 * The successful reception of this CTS packet implies
	 * that our RTS was successful. 
	 * According to the IEEE spec 9.2.5.3, you must 
	 * reset the ssrc_, but not the congestion window.
	 */
	ssrc_ = 0;
	tx_resume();

	mac_log(p);
}

void
Mac802_11mr::recvDATA(Packet *p)
{
	struct hdr_mac802_11 *dh = HDR_MAC802_11(p);
	u_int32_t dst, src, size;
	struct hdr_cmn *ch = HDR_CMN(p);

	dst = ETHER_ADDR(dh->dh_ra);
	src = ETHER_ADDR(dh->dh_ta);
	size = ch->size();
	/*
	 * Adjust the MAC packet size - ie; strip
	 * off the mac header
	 */
	ch->size() -= phymib_.getHdrLen11();
	ch->num_forwards() += 1;

	/*
	 *  If we sent a CTS, clean up...
	 */
	if(dst != MAC_BROADCAST) {
		if(size >= macmib_.getRTSThreshold()) {
			if (tx_state_ == MAC_CTS) {
				assert(pktCTRL_);
				Packet::free(pktCTRL_); pktCTRL_ = 0;
				mhSend_.stop();
				/*
				 * Our CTS got through.
				 */
			} else {
				discard(p, DROP_MAC_BUSY);
				return;
			}
			sendACK(src);
			tx_resume();
		} else {
			/*
			 *  We did not send a CTS and there's no
			 *  room to buffer an ACK.
			 */
			if(pktCTRL_) {
				discard(p, DROP_MAC_BUSY);
				return;
			}
			sendACK(src);
			if(mhSend_.busy() == 0)
				tx_resume();
		}
	}
	
	/* ============================================================
	   Make/update an entry in our sequence number cache.
	   ============================================================ */

	/* Changed by Debojyoti Dutta. This upper loop of if{}else was 
	   suggested by Joerg Diederich <dieder@ibr.cs.tu-bs.de>. 
	   Changed on 19th Oct'2000 */

        if(dst != MAC_BROADCAST) {
                if (src < (u_int32_t) cache_node_count_) {
                        Host *h = &cache_[src];

                        if(h->seqno && h->seqno == dh->dh_scontrol) {
                                discard(p, DROP_MAC_DUPLICATE);
                                return;
                        }
                        h->seqno = dh->dh_scontrol;
                } else {
			static int count = 0;
			if (++count <= 10) {
				printf ("MAC_802_11: accessing MAC cache_ array out of range (src %u, dst %u, size %d)!\n", src, dst, cache_node_count_);
				if (count == 10)
					printf ("[suppressing additional MAC cache_ warnings]\n");
			};
		};
	}

	/*
	 *  Pass the packet up to the link-layer.
	 *  XXX - we could schedule an event to account
	 *  for this processing delay.
	 */
	
	/* in BSS mode, if a station receives a packet via
	 * the AP, and higher layers are interested in looking
	 * at the src address, we might need to put it at
	 * the right place - lest the higher layers end up
	 * believing the AP address to be the src addr! a quick
	 * grep didn't turn up any higher layers interested in
	 * the src addr though!
	 * anyway, here if I'm the AP and the destination
	 * address (in dh_3a) isn't me, then we have to fwd
	 * the packet; we pick the real destination and set
	 * set it up for the LL; we save the real src into
	 * the dh_3a field for the 'interested in the info'
	 * receiver; we finally push the packet towards the
	 * LL to be added back to my queue - accomplish this
	 * by reversing the direction!*/

	if ((bss_id() == addr()) && ((u_int32_t)ETHER_ADDR(dh->dh_ra)!= MAC_BROADCAST)&& ((u_int32_t)ETHER_ADDR(dh->dh_3a) != ((u_int32_t)addr()))) {
		struct hdr_cmn *ch = HDR_CMN(p);
		u_int32_t dst = ETHER_ADDR(dh->dh_3a);
		u_int32_t src = ETHER_ADDR(dh->dh_ta);
		/* if it is a broadcast pkt then send a copy up
		 * my stack also
		 */
		if (dst == MAC_BROADCAST) {
			uptarget_->recv(p->copy(), (Handler*) 0);
		}

		ch->next_hop() = dst;
		STORE4BYTE(&src, (dh->dh_3a));
		ch->addr_type() = NS_AF_ILINK;
		ch->direction() = hdr_cmn::DOWN;
	}

	uptarget_->recv(p, (Handler*) 0);
}


void
Mac802_11mr::recvACK(Packet *p)
{	
	if(tx_state_ != MAC_SEND) {
		discard(p, DROP_MAC_INVALID_STATE);
		return;
	}
	assert(pktTx_);

	struct MultiRateHdr *hdm = HDR_MULTIRATE(p);
	struct hdr_mac802_11 *pmh = HDR_MAC802_11(p);
	PhyMode pm = hdm->mode();
	u_int32_t ack_src = (u_int32_t)ETHER_ADDR(pmh->dh_ta);

	struct hdr_mac802_11 *mh = HDR_MAC802_11(pktTx_);
	u_int32_t dst = (u_int32_t)ETHER_ADDR(mh->dh_ra);

	assert(ack_src == dst);

	if (ack_src == bss_id())
		phymib_.setAPAckMode(pm);
	

	mhSend_.stop();

	incMPDUTxSuccessful(dst);

	/*
	 * The successful reception of this ACK packet implies
	 * that our DATA transmission was successful.  Hence,
	 * we can reset the Short/Long Retry Count and the CW.
	 *
	 * need to check the size of the packet we sent that's being
	 * ACK'd, not the size of the ACK packet.
	 */
	if((u_int32_t) HDR_CMN(pktTx_)->size() <= macmib_.getRTSThreshold()) {
		if (ssrc_ > 1)  { 
			incMPDUTxMultipleRetries(dst);
		} else if (ssrc_ == 1){
			incMPDUTxOneRetry(dst);
		}			
		ssrc_ = 0;
	} else {
		if (slrc_ > 1)  { 
			macmib_.MPDUTxMultipleRetries++; 
			incMPDUTxMultipleRetries(dst);
		} else if (slrc_ == 1){
			incMPDUTxOneRetry(dst);	
		}	
		slrc_ = 0;
	}
	rst_cw();
	Packet::free(pktTx_); 
	pktTx_ = 0;
	
	/*
	 * Backoff before sending again.
	 */
	assert(mhBackoff_.busy() == 0);
	mhBackoff_.start(cw_, 0);
	if(is_idle())
		mhBackoff_.resume(phymib_.getDIFS());

	tx_resume();

	mac_log(p);
}



void Mac802_11mr::incFrameErrors(u_int32_t src, u_int8_t  type, int reason)
{
	macmib_.FrameErrors++;

	if (reason == PKT_ERROR_NOISE)
		macmib_.FrameErrorsNoise++;	

	MR_MAC_MIB* peermacmib = 0;

	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats(src, (u_int32_t)index_)->macmib;

	if (peermacmib) {
		peermacmib->FrameErrors++;
		if (reason == PKT_ERROR_NOISE)
			peermacmib->FrameErrorsNoise++;
	}
			
	
	switch (type) 	{

	case MAC_Type_Management:
		macmib_.MgmtFrameErrors++;
		if (peermacmib)
			peermacmib->MgmtFrameErrors++;

		if (reason == PKT_ERROR_NOISE) {
			macmib_.MgmtFrameErrorsNoise++;
			if (peermacmib)
				peermacmib->MgmtFrameErrorsNoise++;
		}
		break;

	case MAC_Type_Control:		
		macmib_.CtrlFrameErrors++;
		if (peermacmib)
			peermacmib->CtrlFrameErrors++;
		if (reason == PKT_ERROR_NOISE) {
			macmib_.CtrlFrameErrorsNoise++;
			if (peermacmib)
				peermacmib->CtrlFrameErrorsNoise++;
		}
		break;

	case MAC_Type_Data:
		macmib_.DataFrameErrors++;
		if (peermacmib)
			peermacmib->DataFrameErrors++;

		if (reason == PKT_ERROR_NOISE) {
			macmib_.DataFrameErrorsNoise++;
			if (peermacmib)
				peermacmib->DataFrameErrorsNoise++;
		}
		break;

	default:
		fprintf(stderr,"Mac802_11mr::incFrameErrors() unknown MAC_type\n");
		assert(0);
		break;		
	}
	
	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_FrameErrors);
		mehptr = mehptr->next;
	}
}

void Mac802_11mr::incFrameReceives(u_int32_t src, u_int8_t  type)
{
	macmib_.FrameReceives++;
	MR_MAC_MIB* peermacmib = 0;

	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats(src, (u_int32_t)index_)->macmib;

	if (peermacmib)
		peermacmib->FrameReceives++;
	
	switch (type) 	{

	case MAC_Type_Management:
		macmib_.MgmtFrameReceives++;
		if (peermacmib)
			peermacmib->MgmtFrameReceives++;
		break;

	case MAC_Type_Control:		
		macmib_.CtrlFrameReceives++;
		if (peermacmib)
			peermacmib->CtrlFrameReceives++;
		break;

	case MAC_Type_Data:
		macmib_.DataFrameReceives++;
		if (peermacmib)
			peermacmib->DataFrameReceives++;
		break;

	default:
		fprintf(stderr,"Mac802_11mr::incFrameReceives() unknown MAC_type\n");
		assert(0);
		break;		
	}
	
	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_FrameReceives);
		mehptr = mehptr->next;
	}
}



void Mac802_11mr::incFrameDropSyn(u_int32_t src, u_int8_t  type)
{
	macmib_.FrameDropSyn++;
	MR_MAC_MIB* peermacmib = 0;

	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats(src, (u_int32_t)index_)->macmib;

	if (peermacmib)
		peermacmib->FrameDropSyn++;
	
	switch (type) 	{

	case MAC_Type_Management:
		macmib_.MgmtFrameDropSyn++;
		if (peermacmib)
			peermacmib->MgmtFrameDropSyn++;
		break;

	case MAC_Type_Control:		
		macmib_.CtrlFrameDropSyn++;
		if (peermacmib)
			peermacmib->CtrlFrameDropSyn++;
		break;

	case MAC_Type_Data:
		macmib_.DataFrameDropSyn++;
		if (peermacmib)
			peermacmib->DataFrameDropSyn++;
		break;

	default:
		fprintf(stderr,"Mac802_11mr::incFrameReceives() unknown MAC_type\n");
		assert(0);
		break;		
	}
	
	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_FrameDropSyn);
		mehptr = mehptr->next;
	}
}


void Mac802_11mr::incFrameDropTxa(u_int32_t src, u_int8_t  type)
{
	macmib_.FrameDropTxa++;
	MR_MAC_MIB* peermacmib = 0;

	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats(src, (u_int32_t)index_)->macmib;

	if (peermacmib)
		peermacmib->FrameDropTxa++;
	
	switch (type) 	{

	case MAC_Type_Management:
		macmib_.MgmtFrameDropTxa++;
		if (peermacmib)
			peermacmib->MgmtFrameDropTxa++;
		break;

	case MAC_Type_Control:		
		macmib_.CtrlFrameDropTxa++;
		if (peermacmib)
			peermacmib->CtrlFrameDropTxa++;
		break;

	case MAC_Type_Data:
		macmib_.DataFrameDropTxa++;
		if (peermacmib)
			peermacmib->DataFrameDropTxa++;
		break;

	default:
		fprintf(stderr,"Mac802_11mr::incFrameReceives() unknown MAC_type\n");
		assert(0);
		break;		
	}
	
	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_FrameDropTxa);
		mehptr = mehptr->next;
	}
}


void Mac802_11mr::incACKFailed(u_int32_t dst)
{
	macmib_.ACKFailed++;

	MR_MAC_MIB* peermacmib = 0;
	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats((u_int32_t)index_, dst)->macmib;
	if (peermacmib)
		peermacmib->ACKFailed++;       

	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_ACKFailed);
		mehptr = mehptr->next;
	}
}

void Mac802_11mr::incRTSFailed(u_int32_t dst)
{
	macmib_.RTSFailed++;

	MR_MAC_MIB* peermacmib = 0;
	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats((u_int32_t)index_, dst)->macmib;
	if (peermacmib)
		peermacmib->RTSFailed++;       

	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_RTSFailed);
		mehptr = mehptr->next;
	}
}

void Mac802_11mr::incMPDURxSuccessful(u_int32_t src)
{
	macmib_.MPDURxSuccessful++;

	MR_MAC_MIB* peermacmib = 0;
	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats(src, (u_int32_t)index_)->macmib;
	if (peermacmib)
		peermacmib->MPDURxSuccessful++;       

	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_MPDURxSuccessful);
		mehptr = mehptr->next;
	}
}

void Mac802_11mr::incMPDUTxSuccessful(u_int32_t dst)
{
	macmib_.MPDUTxSuccessful++;


	MR_MAC_MIB* peermacmib = 0;
	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats((u_int32_t)index_, dst)->macmib;
	if (peermacmib)
		peermacmib->MPDUTxSuccessful++;       

	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_MPDUTxSuccessful);
		mehptr = mehptr->next;
	}
}

void Mac802_11mr::incMPDUTxOneRetry(u_int32_t dst)
{
	macmib_.MPDUTxOneRetry++;

	MR_MAC_MIB* peermacmib = 0;
	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats((u_int32_t)index_, dst)->macmib;
	if (peermacmib)
		peermacmib->MPDUTxOneRetry++;       

	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_MPDUTxOneRetry);
		mehptr = mehptr->next;
	}
}

void Mac802_11mr::incMPDUTxMultipleRetries(u_int32_t dst)
{
	macmib_.MPDUTxMultipleRetries++;

	MR_MAC_MIB* peermacmib = 0;
	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats((u_int32_t)index_, dst)->macmib;
	if (peermacmib)
		peermacmib->MPDUTxMultipleRetries++;       

	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_MPDUTxMultipleRetries);
		mehptr = mehptr->next;
	}
}

void Mac802_11mr::incMPDUTxFailed(u_int32_t dst)
{
	macmib_.MPDUTxFailed++;

	MR_MAC_MIB* peermacmib = 0;
	if (peerstatsdb_) 
		peermacmib = peerstatsdb_->getPeerStats((u_int32_t)index_, dst)->macmib;
	if (peermacmib)
		peermacmib->MPDUTxFailed++;       

	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_MPDUTxFailed);
		mehptr = mehptr->next;
	}
}


void Mac802_11mr::incidleSlots(int step)
{
	assert(step>=0);
	macmib_.idleSlots += step;

	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_idleSlots);
		mehptr = mehptr->next;
	}
}

void Mac802_11mr::incidle2Slots(int step)
{
	assert(step>=0);
// 	cout << NOW
// 	     << " node " << index_ 
// 	     << " idle2Slots " << macmib_.idle2Slots 
// 	     << " step " << step
// 	     << endl;
	macmib_.idle2Slots += step;
	
	Mac80211EventHandler* mehptr = mehlist_;
	while(mehptr) {
		mehptr->counterHandle(ID_idle2Slots);
		mehptr = mehptr->next;
	}
}



void Mac802_11mr::updateIdleTime(double nextIdleTime, int mode)
{
	assert(nextIdleTime > 0);
	double now = Scheduler::instance().clock();
	nextIdleTime += now;



	if (macmib_.startIdle2Time < now) {
		// channel has been idle up to now
		incidle2Slots(int ((now - macmib_.startIdle2Time)/phymib_.getSlotTime()));
		macmib_.startIdle2Time_min = now;		
	}

	switch (mode) {

	case UIT_FORCED:
		macmib_.startIdle2Time_min = std::max(nextIdleTime, macmib_.startIdle2Time_min);
		macmib_.startIdle2Time = macmib_.startIdle2Time_min;
		break;
				
	case UIT_NORMAL:
		if (nextIdleTime > macmib_.startIdle2Time) {
			macmib_.startIdle2Time_min   = nextIdleTime;
			macmib_.startIdle2Time       = nextIdleTime;
		}
		break;

	case UIT_WEAK:
		macmib_.startIdle2Time = nextIdleTime;
		break;
		
	default:
		assert(0);
		break;
	}

}




void Mac802_11mr::updateSnr(u_int32_t src, u_int32_t dst, double snr)
{
	if (dst == (u_int32_t)index_) {
		if (peerstatsdb_) {
			PeerStats* ps = peerstatsdb_->getPeerStats(src, (u_int32_t)index_);
			assert(ps);
			ps->updateSnr(snr);
		}
	}
	if (src == bss_id()) {
		phymib_.setAPSnr(snr);
	}
}


void Mac802_11mr::updateSnir(u_int32_t src, u_int32_t dst, double snir)
{
	if (dst == (u_int32_t)index_) {
		if (peerstatsdb_) {
			PeerStats* ps = peerstatsdb_->getPeerStats(src, (u_int32_t)index_);
			assert(ps);
			ps->updateSnir(snir);
		}
	}
}


void Mac802_11mr::updateInterf(u_int32_t src, u_int32_t dst, double interf)
{
	if (dst == (u_int32_t)index_) {
		if (peerstatsdb_) {
			PeerStats* ps = peerstatsdb_->getPeerStats(src, (u_int32_t)index_);
			assert(ps);
			ps->updateInterf(interf);
		}
	}
}



double Mac802_11mr::getSnr(u_int32_t addr)
{
	/* we assume it is better to exit if the requested stats
	   are not available, since returning a bogus value might
	   corrupt some algorithm */
	assert(peerstatsdb_);
	PeerStats* ps = peerstatsdb_->getPeerStats(addr, (u_int32_t)index_);
	assert(ps);
	return ps->getSnr();
}

double Mac802_11mr::getSnir(u_int32_t addr)
{
	/* we assume it is better to exit if the requested stats
	   are not available, since returning a bogus value might
	   corrupt some algorithm */
	assert(peerstatsdb_);
	PeerStats* ps = peerstatsdb_->getPeerStats(addr, (u_int32_t)index_);
	assert(ps);
	return ps->getSnir();
}

double Mac802_11mr::getInterf(u_int32_t addr)
{
	/* we assume it is better to exit if the requested stats
	   are not available, since returning a bogus value might
	   corrupt some algorithm */
	assert(peerstatsdb_);
	PeerStats* ps = peerstatsdb_->getPeerStats(addr, (u_int32_t)index_);
	assert(ps);
	return ps->getInterf();
}
