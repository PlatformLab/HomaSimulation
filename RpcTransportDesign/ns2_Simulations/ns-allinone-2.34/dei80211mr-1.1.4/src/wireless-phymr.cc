/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- 
 *
 * Copyright (c) 1996 Regents of the University of California.
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
 *	Engineering Group at Lawrence Berkeley Laboratory and the Daedalus
 *	research group at UC Berkeley.
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
 * $Header: /nfs/jade/vint/CVSROOT/ns-2/mac/wireless-phy.cc,v 1.25 2005/08/22 05:08:33 tomh Exp $
 *
 * Ported from CMU/Monarch's code, nov'98 -Padma Haldar.
 * wireless-phy.cc
 *
 * Ported 2006 from ns-2.29 
 * by Federico Maguolo, Nicola Baldo and Simone Merlin 
 * (SIGNET lab, University of Padova, Department of Information Engineering)
 *
 */

#include <math.h>

#include <packet.h>

#include <mobilenode.h>
#include <phy.h>
#include <propagation.h>
#include <modulation.h>
#include <omni-antenna.h>
#include "wireless-phymr.h"
#include <packet.h>
#include <ip.h>
#include <agent.h>
#include <trace.h>

#include "diffusion/diff_header.h"

#include "papropagation.h"

#ifndef MAX
#define MAX(a,b) (((a)<(b))?(b):(a))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

void PASleep_Timer::expire(Event *) {
	a_->UpdateSleepEnergy();
}


/*
	PowerChangeTimer
*/

PowerChangeTimer::PowerChangeTimer(PAWirelessPhy *phy) : phy_(phy), eventList_(0)
{
}

void PowerChangeTimer::addEvent(double instant, double increasePower)
{
	PowerChangeEvent *pe = new PowerChangeEvent;
	pe->instant = instant;
	pe->increasePower = increasePower;
	PowerChangeEvent *prev = 0;
	for(PowerChangeEvent *cur = eventList_; cur; cur = cur->next)
	{
		if(cur->instant >= instant)
		{
			pe->next = cur;
			if(prev)
				prev->next = pe;
			else
				eventList_ = pe;
			resched(eventList_->instant - Scheduler::instance().clock());
			return;
		}
		if(cur)
			prev = cur;
	}
	if(prev)
		prev->next = pe;
	else
		eventList_ = pe;
	pe->next = 0;
	resched(eventList_->instant - Scheduler::instance().clock());
}

void PowerChangeTimer::expire(Event *e)
{
	assert(eventList_);
	PowerChangeEvent *pe = eventList_;
	eventList_ = eventList_->next;
	phy_->increasePower(pe->increasePower);
	delete pe;
	if(eventList_)
		resched(eventList_->instant - Scheduler::instance().clock());
}

/* ======================================================================
   PAWirelessPhy Interface
   ====================================================================== */
static class PAWirelessPhyClass: public TclClass {
public:
        PAWirelessPhyClass() : TclClass("Phy/WirelessPhy/PowerAware") {}
        TclObject* create(int, const char*const*) {
                return (new PAWirelessPhy);
        }
} class_PAWirelessPhy;


PAWirelessPhy::PAWirelessPhy() : Phy(), sleep_timer_(this), status_(IDLE), powerTimer_(this), profile_(0)
{

	bind("CSThresh_", &CSThresh_);
	bind("Pt_", &Pt_);
	bind("freq_", &freq_);
	bind("L_", &L_);
	
	lambda_ = SPEED_OF_LIGHT / freq_;

	node_ = 0;
	ant_ = 0;
	propagation_ = 0;

	//[Nicola] modulation was removed
	//modulation_ = 0;

	// Assume AT&T's Wavelan PCMCIA card -- Chalermek
        //	Pt_ = 8.5872e-4; // For 40m transmission range.
	//      Pt_ = 7.214e-3;  // For 100m transmission range.
	//      Pt_ = 0.2818; // For 250m transmission range.
	//	Pt_ = pow(10, 2.45) * 1e-3;         // 24.5 dbm, ~ 281.8mw
	
	Pt_consume_ = 0.660;  // 1.6 W drained power for transmission
	Pr_consume_ = 0.395;  // 1.2 W drained power for reception

	//	P_idle_ = 0.035; // 1.15 W drained power for idle

	P_idle_ = 0.0;
	P_sleep_ = 0.00;
	P_transition_ = 0.00;
	node_on_=1;
	
	channel_idle_time_ = NOW;
	update_energy_time_ = NOW;
	last_send_time_ = NOW;
	
	sleep_timer_.resched(1.0);

}

int
PAWirelessPhy::command(int argc, const char*const* argv)
{
	TclObject *obj; 

	if (argc==2) {
		if (strcasecmp(argv[1], "NodeOn") == 0) {
			node_on();

			if (em() == NULL) 
				return TCL_OK;
			if (NOW > update_energy_time_) {
				update_energy_time_ = NOW;
			}
			return TCL_OK;
		} else if (strcasecmp(argv[1], "NodeOff") == 0) {
			node_off();

			if (em() == NULL) 
				return TCL_OK;
			if (NOW > update_energy_time_) {
				em()->DecrIdleEnergy(NOW-update_energy_time_,
						     P_idle_);
				update_energy_time_ = NOW;
			}
			return TCL_OK;
		}
	} else if(argc == 3) {
		if (strcasecmp(argv[1], "setTxPower") == 0) {
			Pt_consume_ = atof(argv[2]);
			return TCL_OK;
		} else if (strcasecmp(argv[1], "setRxPower") == 0) {
			Pr_consume_ = atof(argv[2]);
			return TCL_OK;
		} else if (strcasecmp(argv[1], "setIdlePower") == 0) {
			P_idle_ = atof(argv[2]);
			return TCL_OK;
		}else if (strcasecmp(argv[1], "setSleepPower") == 0) {
			P_sleep_ = atof(argv[2]);
			return TCL_OK;
		} else if (strcasecmp(argv[1], "setTransitionPower") == 0) {
			P_transition_ = atof(argv[2]);
			return TCL_OK;
		} else if (strcasecmp(argv[1], "setTransitionTime") == 0) {
			T_transition_ = atof(argv[2]);
			return TCL_OK;
		}else if( (obj = TclObject::lookup(argv[2])) == 0) {
			fprintf(stderr,"PAWirelessPhy: %s lookup of %s failed\n", 
				argv[1], argv[2]);
			return TCL_ERROR;
		}else if (strcmp(argv[1], "propagation") == 0) {
			assert(propagation_ == 0);
			propagation_ = (PAFreeSpace*) obj;
			return TCL_OK;
		} else if (strcasecmp(argv[1], "antenna") == 0) {
			ant_ = (Antenna*) obj;
			return TCL_OK;
		} else if (strcasecmp(argv[1], "node") == 0) {
			assert(node_ == 0);
			node_ = (Node *)obj;
			return TCL_OK;
		} else 	if (strcmp(argv[1], "powerProfile") == 0) {
			profile_ = (PowerProfile *) TclObject::lookup(argv[2]);
			if (profile_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		}
	}
	return Phy::command(argc,argv);
}
 
void 
PAWirelessPhy::sendDown(Packet *p)
{
	/*
	 * Sanity Check
	 */
	assert(initialized());
	
	if (em()) {
		if (Is_node_on() != true ) {
			//node is off here...
			Packet::free(p);
			return;
		}
		if(Is_node_on() == true && Is_sleeping() == true){
			em()-> DecrSleepEnergy(NOW-update_energy_time_,
					       P_sleep_);
			update_energy_time_ = NOW;
			
		}
		
	}
	/*
	 * Decrease node's energy
	 */
	if(em()) {
		if (em()->energy() > 0) {

		    double txtime = hdr_cmn::access(p)->txtime();
		    double start_time = MAX(channel_idle_time_, NOW);
		    double end_time = MAX(channel_idle_time_, NOW+txtime);
		    double actual_txtime = end_time-start_time;

		    if (start_time > update_energy_time_) {
			    em()->DecrIdleEnergy(start_time - 
						 update_energy_time_, P_idle_);
			    update_energy_time_ = start_time;
		    }

		    /* It turns out that MAC sends packet even though, it's
		       receiving some packets.
		    
		    if (txtime-actual_txtime > 0.000001) {
			    fprintf(stderr,"Something may be wrong at MAC\n");
			    fprintf(stderr,"act_tx = %lf, tx = %lf\n", actual_txtime, txtime);
		    }
		    */

		   // Sanity check
		   double temp = MAX(NOW,last_send_time_);

		   /*
		   if (NOW < last_send_time_) {
			   fprintf(stderr,"Argggg !! Overlapping transmission. NOW %lf last %lf temp %lf\n", NOW, last_send_time_, temp);
		   }
		   */
		   
		   double begin_adjust_time = MIN(channel_idle_time_, temp);
		   double finish_adjust_time = MIN(channel_idle_time_, NOW+txtime);
		   double gap_adjust_time = finish_adjust_time - begin_adjust_time;
		   if (gap_adjust_time < 0.0) {
			   fprintf(stderr,"What the heck ! negative gap time.\n");
		   }

		   if ((gap_adjust_time > 0.0) && (status_ == RECV)) {
			   em()->DecrTxEnergy(gap_adjust_time,
					      Pt_consume_-Pr_consume_);
		   }

		   em()->DecrTxEnergy(actual_txtime,Pt_consume_);
//		   if (end_time > channel_idle_time_) {
//			   status_ = SEND;
//		   }
//
		   status_ = IDLE;

		   last_send_time_ = NOW+txtime;
		   channel_idle_time_ = end_time;
		   update_energy_time_ = end_time;

		   if (em()->energy() <= 0) {
			   em()->setenergy(0);
			   ((MobileNode*)node())->log_energy(0);
		   }

		} else {

			// log node energy
			if (em()->energy() > 0) {
				((MobileNode *)node_)->log_energy(1);
			} 
//
			Packet::free(p);
			return;
		}
	}

	/*
	 *  Stamp the packet with the interface arguments
	 */
	p->txinfo_.stamp((MobileNode*)node(), ant_->copy(), Pt_, lambda_);
	
	// Send the packet
	channel_->recv(p, this);
}

int 
PAWirelessPhy::sendUp(Packet *p)
{
	/*
	 * Sanity Check
	 */
	assert(initialized());

	PacketStamp s;
	double Pr;
	int pkt_recvd = 1; // [Nicola] Changed default value, removed spaghetti code below

	Pr = p->txinfo_.getTxPr();
	

	if (em()) {

		// Drop the packet if the node is either OFF, SLEEPING, 
		// or if it has ZERO ENERGY

		if ((Is_node_on()== false) || (Is_sleeping()==true) || (em()->energy() <= 0)) {
			pkt_recvd = 0;
		}
			
	}

	if(propagation_) {
		s.stamp((MobileNode*)node(), ant_, 0, lambda_);
		Pr = propagation_->Pr(p, &p->txinfo_, &s, getL(), getLambda());
		if (Pr < CSThresh_) {
			pkt_recvd = 0;
		}
// ---------------------------------------------
// [Nicola] Removed because we're dealing with 
//	    RX success somewhere else
// --------------------------------------------
// 		if (Pr < RXThresh_) {
// 			/*
// 			 * We can detect, but not successfully receive
// 			 * this packet.
// 			 */
// 			hdr_cmn *hdr = HDR_CMN(p);
// 			hdr->error() = 1;
// #if DEBUG > 3
// 			printf("SM %f.9 _%d_ drop pkt from %d low POWER %e/%e\n",
// 			       Scheduler::instance().clock(), node()->index(),
// 			       p->txinfo_.getNode()->index(),
// 			       Pr,RXThresh);
// #endif
// 		}
	}


// ---------------------
// [Nicola] Removed because:
//  1) the only available modulation module implementation is almost fake
//  2) errors will be handled somewhere else through SNIR

// 	if(modulation_) {
// 		hdr_cmn *hdr = HDR_CMN(p);
// 		hdr->error() = modulation_->BitError(Pr);
// 	}
// ---------------------
	
	/*
	 * The MAC layer must be notified of the packet reception
	 * now - ie; when the first bit has been detected - so that
	 * it can properly do Collision Avoidance / Detection.
	 */
	 
	/*
	Federico  27/04/2006 - I need to gnerate an event when the trasmission ends, in order to decrease the instant power
	*/
	if(debug_>1) printf("%f - WirelessPhy.sendUp - receivePower=%-2.20f\n", Scheduler::instance().clock(),Pr);
	increasePower(Pr);
	powerTimer_.addEvent(Scheduler::instance().clock() + hdr_cmn::access(p)->txtime(), -Pr);	
	
	p->txinfo_.getAntenna()->release();

	/* WILD HACK: The following two variables are a wild hack.
	   They will go away in the next release...
	   They're used by the mac-802_11 object to determine
	   capture.  This will be moved into the net-if family of 
	   objects in the future. */
	p->txinfo_.RxPr = Pr;
	
	// [Nicola] CTPhresh was part of the WILD HACK above, we don't need it any more...
	// p->txinfo_.CPThresh = CPThresh_;

	/*
	 * Decrease energy if packet successfully received
	 */
	if(pkt_recvd && em()) {

		double rcvtime = hdr_cmn::access(p)->txtime();
		// no way to reach here if the energy level < 0
		
		double start_time = MAX(channel_idle_time_, NOW);
		double end_time = MAX(channel_idle_time_, NOW+rcvtime);
		double actual_rcvtime = end_time-start_time;

		if (start_time > update_energy_time_) {
			em()->DecrIdleEnergy(start_time-update_energy_time_,
					     P_idle_);
			update_energy_time_ = start_time;
		}
		
		em()->DecrRcvEnergy(actual_rcvtime,Pr_consume_);
/*
  if (end_time > channel_idle_time_) {
  status_ = RECV;
  }
*/
		channel_idle_time_ = end_time;
		update_energy_time_ = end_time;

		status_ = IDLE;

		/*
		  hdr_diff *dfh = HDR_DIFF(p);
		  printf("Node %d receives (%d, %d, %d) energy %lf.\n",
		  node()->address(), dfh->sender_id.addr_, 
		  dfh->sender_id.port_, dfh->pk_num, node()->energy());
		*/

		// log node energy
		if (em()->energy() > 0) {
		((MobileNode *)node_)->log_energy(1);
        	} 

		if (em()->energy() <= 0) {  
			// saying node died
			em()->setenergy(0);
			((MobileNode*)node())->log_energy(0);
		}
	}
	
	return pkt_recvd;
}

void
PAWirelessPhy::node_on()
{

        node_on_= TRUE;
	status_ = IDLE;

       if (em() == NULL)
 	    return;	
   	if (NOW > update_energy_time_) {
      	    update_energy_time_ = NOW;
   	}
}

void 
PAWirelessPhy::node_off()
{

        node_on_= FALSE;
	status_ = SLEEP;

	if (em() == NULL)
            return;
        if (NOW > update_energy_time_) {
            em()->DecrIdleEnergy(NOW-update_energy_time_,
                                P_idle_);
            update_energy_time_ = NOW;
	}
}

void 
PAWirelessPhy::node_wakeup()
{

	if (status_== IDLE)
		return;

	if (em() == NULL)
            return;

        if ( NOW > update_energy_time_ && (status_== SLEEP) ) {
		//the power consumption when radio goes from SLEEP mode to IDLE mode
		em()->DecrTransitionEnergy(T_transition_,P_transition_);
		
		em()->DecrSleepEnergy(NOW-update_energy_time_,
				      P_sleep_);
		status_ = IDLE;
	        update_energy_time_ = NOW;
		
		// log node energy
		if (em()->energy() > 0) {
			((MobileNode *)node_)->log_energy(1);
	        } else {
			((MobileNode *)node_)->log_energy(0);   
	        }
	}
}

void 
PAWirelessPhy::node_sleep()
{
//
//        node_on_= FALSE;
//
	if (status_== SLEEP)
		return;

	if (em() == NULL)
            return;

        if ( NOW > update_energy_time_ && (status_== IDLE) ) {
	//the power consumption when radio goes from IDLE mode to SLEEP mode
	    em()->DecrTransitionEnergy(T_transition_,P_transition_);

            em()->DecrIdleEnergy(NOW-update_energy_time_,
                                P_idle_);
		status_ = SLEEP;
	        update_energy_time_ = NOW;

	// log node energy
		if (em()->energy() > 0) {
			((MobileNode *)node_)->log_energy(1);
	        } else {
			((MobileNode *)node_)->log_energy(0);   
	        }
	}
}
//
void
PAWirelessPhy::dump(void) const
{
	Phy::dump();
	fprintf(stdout,
		"\tPt: %f, Gt: %f, Gr: %f, lambda: %f, L: %f\n",
		Pt_, ant_->getTxGain(0,0,0,lambda_), ant_->getRxGain(0,0,0,lambda_), lambda_, L_);
	//fprintf(stdout, "\tbandwidth: %f\n", bandwidth_);
	fprintf(stdout, "--------------------------------------------------\n");
}


void PAWirelessPhy::UpdateIdleEnergy()
{
	if (em() == NULL) {
		return;
	}
	if (NOW > update_energy_time_ && (Is_node_on()==TRUE && status_ == IDLE ) ) {
		  em()-> DecrIdleEnergy(NOW-update_energy_time_,
					P_idle_);
		  update_energy_time_ = NOW;
	}

	// log node energy
	if (em()->energy() > 0) {
		((MobileNode *)node_)->log_energy(1);
        } else {
		((MobileNode *)node_)->log_energy(0);   
        }

//	idle_timer_.resched(10.0);
}

double PAWirelessPhy::getDist(double Pr, double Pt, double Gt, double Gr,
			    double hr, double ht, double L, double lambda)
{
	if (propagation_) {
		return propagation_->getDist(Pr, Pt, Gt, Gr, hr, ht, L,
					     lambda);
	}
	return 0;
}

//
void PAWirelessPhy::UpdateSleepEnergy()
{
	if (em() == NULL) {
		return;
	}
	if (NOW > update_energy_time_ && ( Is_node_on()==TRUE  && Is_sleeping() == true) ) {
		  em()-> DecrSleepEnergy(NOW-update_energy_time_,
					P_sleep_);
		  update_energy_time_ = NOW;
		// log node energy
		if (em()->energy() > 0) {
			((MobileNode *)node_)->log_energy(1);
        	} else {
			((MobileNode *)node_)->log_energy(0);   
        	}
	}
	
	//A hack to make states consistent with those of in Energy Model for AF
	int static s=em()->sleep();
	if(em()->sleep()!=s){

		s=em()->sleep();	
		if(s==1)
			node_sleep();
		else
			node_wakeup();			
		printf("\n AF hack %d\n",em()->sleep());	
	}	
	
	sleep_timer_.resched(10.0);
}

void PAWirelessPhy::increasePower(double power)
{
	if(profile_)
	{
		PowerElement el;
		el.power = power;
		el.time = Scheduler::instance().clock();
		if(debug_) printf("%f - WirelessPhy.increasePower - Power=%-2.20f\n", Scheduler::instance().clock(),power);
		profile_->addElement(el);
	}
}
