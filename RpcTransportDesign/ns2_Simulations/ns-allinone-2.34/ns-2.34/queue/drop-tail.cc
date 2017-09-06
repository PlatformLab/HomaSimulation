/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1994 Regents of the University of California.
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
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/queue/drop-tail.cc,v 1.17 2004/10/28 23:35:37 haldar Exp $ (LBL)";
#endif

#include "drop-tail.h"

static class DropTailClass : public TclClass {
 public:
	DropTailClass() : TclClass("Queue/DropTail") {}
	TclObject* create(int, const char*const*) {
		return (new DropTail);
	}
} class_drop_tail;

void DropTail::reset()
{
	Queue::reset();
}

int
DropTail::command(int argc, const char*const* argv)
{
	if (argc==2) {
		if (strcmp(argv[1], "printstats") == 0) {
			print_summarystats();
			return (TCL_OK);
		}
 		if (strcmp(argv[1], "shrink-queue") == 0) {
 			shrink_queue();
 			return (TCL_OK);
 		}
	}
	if (argc == 3) {
		if (!strcmp(argv[1], "packetqueue-attach")) {
			delete q_;
			if (!(q_ = (PacketQueue*) TclObject::lookup(argv[2])))
				return (TCL_ERROR);
			else {
				pq_ = q_;
				return (TCL_OK);
			}
		}
	}
	return Queue::command(argc, argv);
}

/*
 * drop-tail
 */
void DropTail::enque(Packet* p)
{
	if (summarystats) {
                Queue::updateStats(qib_?q_->byteLength():q_->length());
	}
	//printf("qlim_ = %d, qib_ = %d, mean_pktsize_ = %d\n", qlim_, qib_, mean_pktsize_);
	int qlimBytes = qlim_ * mean_pktsize_;
	if ((!qib_ && (q_->length() + 1) >= qlim_) ||
  	(qib_ && (q_->byteLength() + hdr_cmn::access(p)->size()) >= qlimBytes)){
		// if the queue would overflow if we added this packet...
		if (drop_front_) { /* remove from head of queue */
			q_->enque(p);
			Packet *pp = q_->deque();
			drop(pp);
		}
		else if (drop_prio_) {
			Packet *max_pp = p;
			int max_prio = 0;

			q_->enque(p);
			q_->resetIterator();
			for (Packet *pp = q_->getNext(); pp != 0; pp = q_->getNext()) {
				if (!qib_ || ( q_->byteLength() - hdr_cmn::access(pp)->size() < qlimBytes)) {
					hdr_ip* h = hdr_ip::access(pp);
					int prio = h->prio();
					if (prio >= max_prio) {
						max_pp = pp;
						max_prio = prio;
					}
				}
			}
			q_->remove(max_pp);
			drop(max_pp);
		}
		else if (drop_smart_) {
			Packet *max_pp = p;
			int max_count = 0;

			q_->enque(p);
			q_->resetIterator();
			for (Packet *pp = q_->getNext(); pp != 0; pp = q_->getNext()) {
				hdr_ip* h = hdr_ip::access(pp);
				FlowKey fkey;
				fkey.src = h->saddr();
				fkey.dst = h->daddr();
				fkey.fid = h->flowid();

				char* fkey_buf = (char*) &fkey;
				int length = sizeof(fkey);
				string fkey_string(fkey_buf, length);

				std::tr1::hash<string> string_hasher;
				size_t signature = string_hasher(fkey_string);

				if (sq_counts_.find(signature) != sq_counts_.end()) {
					int count = sq_counts_[signature];
					if (count > max_count) {
						max_count = count;
						max_pp = pp;
					}
				}
			}
			q_->remove(max_pp);
			/*hdr_ip* h = hdr_ip::access(p);
		       	FlowKey fkey;
	       		fkey.src = h->saddr();
       			fkey.dst = h->daddr();
       			fkey.fid = h->flowid();

		        char* fkey_buf = (char*) &fkey;
	       		int length = sizeof(fkey);
	       		string fkey_string(fkey_buf, length);

	       		std::tr1::hash<string> string_hasher;
	       		size_t p_sig = string_hasher(fkey_string);
	       		h = hdr_ip::access(max_pp);
	       		fkey.src = h->saddr();
	       		fkey.dst = h->daddr();
	       		fkey.fid = h->flowid();

	       		string fkey_string2(fkey_buf, length);
	       		size_t maxpp_sig = string_hasher(fkey_string2);

			 printf("%s, enqueued %d, dropped %d instead\n", this->name(), p_sig, maxpp_sig);*/
			drop(max_pp);
		} else {
			drop(p);
		}
	} else {
		q_->enque(p);
	}
}

//AG if queue size changes, we drop excessive packets...
void DropTail::shrink_queue()
{
        int qlimBytes = qlim_ * mean_pktsize_;
	if (debug_)
		printf("shrink-queue: time %5.2f qlen %d, qlim %d\n",
 			Scheduler::instance().clock(),
			q_->length(), qlim_);
        while ((!qib_ && q_->length() > qlim_) ||
            (qib_ && q_->byteLength() > qlimBytes)) {
                if (drop_front_) { /* remove from head of queue */
                        Packet *pp = q_->deque();
                        drop(pp);
                } else {
                        Packet *pp = q_->tail();
                        q_->remove(pp);
                        drop(pp);
                }
        }
}

Packet* DropTail::deque()
{
        if (summarystats && &Scheduler::instance() != NULL) {
                Queue::updateStats(qib_?q_->byteLength():q_->length());
        }
	//printf("drop_smart_ = %d, sq_limit = %d \n", drop_smart_, sq_limit_);

	/*Shuang: deque the packet with the highest priority */
	if (deque_prio_) {
			q_->resetIterator();
			Packet *p = q_->getNext();
			int highest_prio_;
			if (p != 0)
				highest_prio_ = hdr_ip::access(p)->prio();
			else
				return 0;
			for (Packet *pp = q_->getNext(); pp != 0; pp = q_->getNext()) {
					hdr_ip* h = hdr_ip::access(pp);
					int prio = h->prio();
					//deque from the head
					if (prio < highest_prio_) {
						p = pp;
						highest_prio_ = prio;
				}
			}
			if (keep_order_) {
				q_->resetIterator();
				hdr_ip* hp = hdr_ip::access(p);
				for (Packet *pp = q_->getNext(); pp != p; pp = q_->getNext()) {
					hdr_ip* h = hdr_ip::access(pp);
					if (h->saddr() == hp->saddr()
					&& h->daddr() == hp->daddr()
					&& h->flowid() == hp->flowid()) {
						p = pp;
						break;
					}
				}
			}

			//if (hdr_ip::access(p)->flowid() == 1) {
		//		q_->resetIterator();
		//		printf("DEQUE: flow %d queue_length %d\n", hdr_ip::access(p)->flowid(), q_->byteLength());
		//		for (Packet *pp = q_->getNext(); pp != 0; pp = q_->getNext()) {
		//			hdr_ip* h = hdr_ip::access(pp);
		//			printf("( %d %d )", h->flowid(), h->prio());
		//		}
		//		printf("\n");
		//		fflush(stdout);
			//}

			q_->remove(p);
			return p;
	} else
	if (drop_smart_) {
		Packet *p = q_->deque();
		if (p) {
			hdr_ip* h = hdr_ip::access(p);
			FlowKey fkey;
			fkey.src = h->saddr();
			fkey.dst = h->daddr();
			fkey.fid = h->flowid();

			char* fkey_buf = (char*) &fkey;
			int length = sizeof(fkey);
			string fkey_string(fkey_buf, length);

			std::tr1::hash<string> string_hasher;
			size_t signature = string_hasher(fkey_string);
			sq_queue_.push(signature);

			if (sq_counts_.find(signature) != sq_counts_.end()) {
				sq_counts_[signature]++;
				//printf("%s packet with signature %d, count = %d, qsize = %d\n", this->name(), signature, sq_counts_[signature], sq_queue_.size());
			}
			else {
				sq_counts_[signature] = 1;
				//printf("%s first packet with signature %d, count = %d, qsize = %d\n", this->name(), signature, sq_counts_[signature], sq_queue_.size());
			}

       			if (sq_queue_.size() > sq_limit_) {
		       		//printf("%s we are full %d %d\n", this->name(), sq_counts_.size(), sq_queue_.size());
	       			size_t temp = sq_queue_.front();				                            	 sq_queue_.pop();
				sq_counts_[temp]--;
				if (sq_counts_[temp] == 0)
					sq_counts_.erase(temp);

				//printf("%s removed front sig = %d, no longer full %d %d\n", this->name(), temp, sq_counts_.size(), sq_queue_.size());

			}
		}
		return p;

    	} else {
		return q_->deque();
	}
}

void DropTail::print_summarystats()
{
	//double now = Scheduler::instance().clock();
        printf("True average queue: %5.3f", true_ave_);
        if (qib_)
                printf(" (in bytes)");
        printf(" time: %5.3f\n", total_time_);
}
