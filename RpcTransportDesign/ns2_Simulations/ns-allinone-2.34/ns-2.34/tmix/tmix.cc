/* -*-  Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */

/* 
 * Copyright 2007, Old Dominion University
 * Copyright 2007, University of North Carolina at Chapel Hill
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright 
 * notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright 
 * notice, this list of conditions and the following disclaimer in the 
 * documentation and/or other materials provided with the distribution.
 *    3. The name of the author may not be used to endorse or promote 
 * products derived from this software without specific prior written 
 * permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * M.C. Weigle, P. Adurthi, F. Hernandez-Campos, K. Jeffay, and F.D. Smith, 
 * Tmix: A Tool for Generating Realistic Application Workloads in ns-2, 
 * ACM Computer Communication Review, July 2006, Vol 36, No 3, pp. 67-76.
 *
 * Contact: Michele Weigle (mweigle@cs.odu.edu)
 * 
 * For more information on Tmix and to obtain Tmix tools and 
 * connection vectors, see http://netlab.cs.unc.edu/Tmix
 */

#include <tclcl.h>
#include "lib/bsd-list.h"
#include "tcp-full.h"
#include "tmix.h"

/*:::::::::::::::::::::::::: ADU class :::::::::::::::::::::::::::*/

void ADU::print()
{
	if (size_ != FIN) {
		fprintf (stderr,"\t%lu B (%g s after send / %g s after recv)\n",
			 size_, get_send_wait_sec(), get_recv_wait_sec());
	}
	else {
		fprintf (stderr, "\tFIN (%g s after send / %g s after recv)\n",
			 get_send_wait_sec(), get_recv_wait_sec());
	}
}

/*:::::::::::::::::::::::: CONNECTION VECTOR class :::::::::::::::::::::::::*/

ConnVector::~ConnVector()
{
	for (vector<ADU*>::iterator i = init_ADU_.begin();
	  i != init_ADU_.end(); ++i) {
		delete *i;
	}
	for (vector<ADU*>::iterator i = acc_ADU_.begin();
	  i != acc_ADU_.end(); ++i) {
		delete *i;
	}
	init_ADU_.clear();
	acc_ADU_.clear();
}

void ConnVector::add_ADU (ADU* adu, bool direction)
{
	if (direction == INITIATOR) {
		init_ADU_.push_back (adu);
	}
	else if (direction == ACCEPTOR) {
		acc_ADU_.push_back (adu);
	}
}

void ConnVector::print()
{
	int i;

	fprintf (stderr, "CV %ld> %f %s (%d:%d) %d init, %d acc\n", global_id_,
		 start_time_, (type_==SEQ)?"SEQ":"CONC", init_win_, acc_win_,
		 init_ADU_count_, acc_ADU_count_);
	
	fprintf (stderr, "  INIT:\n");
	for (i=0; i<(int) init_ADU_.size(); i++) {
		init_ADU_[i]->print();
	}

	fprintf (stderr, "  ACC:\n");
	for (i=0; i<(int) acc_ADU_.size(); i++) {
		acc_ADU_[i]->print();
	}
}

/*::::::::::::::::::::::::: TMIX class :::::::::::::::::::::::::::::::::*/

static class TmixClass : public TclClass {
public:
	TmixClass() : TclClass("Tmix") {}
	TclObject* create(int, const char*const*) {
		return (new Tmix);
	}
} class_Tmix;

Tmix::Tmix() :
	TclObject(), timer_(this), next_init_ind_(0), 
	next_acc_ind_(0), total_nodes_(0), current_node_(0), outfp_(NULL),
	cvfp_(NULL), ID_(-1), run_(0), debug_(0),pkt_size_(1460),step_size_(1000),
       	warmup_(0),active_connections_(0), total_connections_(0), total_apps_(0),
	running_(false)
{
	connections_.clear();
	line[0] = '#';
	strcpy (tcptype_, "Reno");

	for (int i=0; i<MAX_NODES; i++) {
		acceptor_[i] = NULL;
	}
	for (int i=0; i<MAX_NODES; i++) {
		initiator_[i] = NULL;
	}
}

Tmix::~Tmix()
{
	Tcl& tcl = Tcl::instance();

	/* output stats */
	if (debug_ > 0) {
		fprintf (stderr, "total connections: %lu / active: %lu  ", 
			 total_connections_, active_connections_);
		fprintf (stderr, "(apps in pool: %d  active: %d)\n", 
			 (int) appPool_.size(), (int) appActive_.size());
	}
	
	/* cancel timer */
	timer_.force_cancel();

	/* delete active apps in the pool */
	map<string, TmixApp*>::iterator iter;
        if(!appActive_.empty()) {
		for (iter = appActive_.begin(); 
		     iter != appActive_.end(); iter++) {
			iter->second->stop();
			tcl.evalf ("delete %s", iter->second->name());
			appActive_.erase (iter);
		}
	}

	/* delete apps in pool */
	TmixApp* app;
	while (!appPool_.empty()) {
		app = appPool_.front();
		app->stop();
		tcl.evalf ("delete %s", app->name());
		appPool_.pop();
	}

	/* delete agents in the pool */
	FullTcpAgent* tcp;
	while (!tcpPool_.empty()) {
		tcp = tcpPool_.front();
		tcl.evalf ("delete %s", tcp->name());
 		tcpPool_.pop();
	}

	/* close output file */
	if (outfp_)
		fclose(outfp_);

	/* close input file, if not already closed */
        if (cvfp_)
		fclose (cvfp_);
}

FullTcpAgent* Tmix::picktcp()
{
	FullTcpAgent* a;
	Tcl& tcl = Tcl::instance();

	if (tcpPool_.empty()) {
		tcl.evalf ("%s alloc-tcp %s", name(), tcptype_);
		a = (FullTcpAgent*) lookup_obj (tcl.result());
		if (a == NULL) {
			fprintf (stderr, "Failed to allocate a TCP agent\n");
			abort();
		}
		if (debug_ > 1) {
			fprintf (stderr, 
				 "\tflow %lu created new TCPAgent %s\n",
				 total_connections_, a->name());
		}
	} else {
		a = tcpPool_.front(); 	/* grab top of the queue */
		tcpPool_.pop();         /* remove top from queue */

		if (debug_ > 1) {
			fprintf (stderr, "\tflow %lu got TCPAgent %s", 
				 total_connections_, a->name());
			fprintf (stderr, " from pool (%d in pool)\n",
				 (int) tcpPool_.size());
		}
	}

	return a;
}

TmixApp* Tmix::pickApp()
{
	TmixApp* a;

	if (appPool_.empty()) {
		Tcl& tcl = Tcl::instance();
		tcl.evalf ("%s alloc-app", name());
		a = (TmixApp*) lookup_obj (tcl.result());
		if (a == NULL) {
			fprintf (stderr, 
				 "Failed to allocate a Tmix app\n");
			abort();
		}
		if (debug_ > 1)
			fprintf (stderr, "\tflow %lu created new ",
				 total_connections_);
	} else {
		a = appPool_.front();   /* grab top of the queue */
		appPool_.pop();         /* remove top from queue */
		if (debug_ > 1)
			fprintf (stderr, "\tflow %lu got ", 
				 total_connections_);
	}

	/* initialize app */
	total_apps_++;
	a->set_id(total_apps_);
	a->set_mgr(this);

	if (debug_ > 1)
		fprintf (stderr, "App %s/%lu (%d in pool, %d active)\n",
			 a->name(), a->get_id(), (int) appPool_.size(), 
			 (int) appActive_.size()+1);

	return a;
}


void Tmix::recycle(FullTcpAgent* agent)
{
	if (agent == NULL) {
		fprintf (stderr, "Tmix::recycle> agent is null\n");
		return;
	}

	/* reinitialize FullTcp agent */
	agent->reset();

	/* add to the inactive agent pool */
	tcpPool_.push (agent);

	if (debug_ > 2) {
		fprintf (stderr, "\tTCPAgent %s moved to pool ", 
			 agent->name());
		fprintf (stderr, "(%d in pool)\n", (int) tcpPool_.size());
	}
}

void Tmix::recycle(TmixApp* app)
{
	if (app == NULL)
		return;

	/* find the app in the active pool */
	map<string, TmixApp*>::iterator iter = 
		appActive_.find(app->get_agent_name());
	if (iter == appActive_.end()) 
		return;

	/* remove the app from the active pool */
	appActive_.erase(iter);

	/* insert the app into the inactive pool */
	appPool_.push (app);

	if (debug_ > 2) {
		fprintf (stderr, "\tApp %s (%lu) moved to pool ", 
			 app->name(), app->get_id());
		fprintf (stderr, "(%d in pool, %d active)\n", 
			 (int) appPool_.size(), (int) appActive_.size());
	}

	/* recycle app */
	app->recycle();
}

void Tmix::incr_next_active()
{
	ConnVector* cv;
	int i = 0;

	list<ConnVector*>::iterator iter = next_active_;

	iter++;
	if (iter == connections_.end()) {
		while (!feof (cvfp_) && i < (int) step_size_) {
			/* all connections are started and there are still 
			 * connection vectors in the file, so read a set */
			cv = read_one_cvec();
			if (cv != NULL) {
				connections_.push_back(cv);
				if (debug_ > 3) {
 fprintf (stderr, "Node %s> read one cvec (%ld, %f s) at %f (cur total: %d)\n",
	  name(), cv->get_ID(), cv->get_start_time(), now(), 
	  (int) connections_.size());
				}
			}
			else {
				fprintf (stderr, "cv is null!\n");
			}
			i++;
		}
	}
	next_active_++;
}

void Tmix::setup_connection ()
/*
 * Setup a new connection, including creation of Agents and Apps
 */
{
	Tcl& tcl = Tcl::instance();
	ConnVector* cv = get_current_cvec();

	/* incr count of connections */
	active_connections_++;
	total_connections_++;

	if (debug_ > 1) {
		fprintf (stderr, 
			 "\nTmix %s> new flow %lu  total active: %lu at %f\n",
			 name(), total_connections_, active_connections_, 
			 now());
	}
        
	/* pick tcp agent for initiator and acceptor */
	FullTcpAgent* init_tcp = picktcp();
	FullTcpAgent* acc_tcp = picktcp();

	/* rotate through nodes assigning connections */
	current_node_++;
	if (current_node_ >= total_nodes_)
		current_node_ = 0;

	/* attach agents to nodes (acceptor_ init_) */
	tcl.evalf ("%s attach %s", acceptor_[current_node_]->name(), 
		   acc_tcp->name());
	tcl.evalf ("%s attach %s", initiator_[current_node_]->name(), 
		   init_tcp->name());

	/* set TCP options */
	tcl.evalf ("%s setup-tcp %s %d %d", name(), acc_tcp->name(), 
		   total_connections_, cv->get_acc_win());
	tcl.evalf ("%s setup-tcp %s %d %d", name(), init_tcp->name(), 
		   total_connections_, cv->get_init_win());

	/* setup connection between initiator and acceptor */
	tcl.evalf ("set ns [Simulator instance]");
	tcl.evalf ("$ns connect %s %s", init_tcp->name(), acc_tcp->name());

	/* create TmixApps and put in active list */
	TmixApp* init_app = pickApp();
	appActive_[init_tcp->name()] = init_app;

	TmixApp* acc_app = pickApp();
	appActive_[acc_tcp->name()] = acc_app;

	/* attach TCPs to TmixApps */
	init_tcp->attachApp((Application*) init_app);
	init_app->set_agent(init_tcp);
	acc_tcp->attachApp((Application*) acc_app);
	acc_app->set_agent(acc_tcp);

	/* associate these peers with each other */
	init_app->set_peer(acc_app);
        acc_app->set_peer(init_app);

	init_app->set_cv(cv);
	acc_app->set_cv(cv);

	init_app->set_type(INITIATOR);
	acc_app->set_type(ACCEPTOR);
        
	/* start TmixApps */
	init_app->start();
	acc_app->start();
}

int fpeek(FILE *stream)
{
    int c;
    c = fgetc(stream);
    ungetc(c,stream);
    return c;
}

#include <iostream>
using namespace std;
ConnVector*
Tmix::read_one_cvec() {
  static bool read = false;
  static int cv_file_type = 0;
  if (!read) {
    read = true;
    char local_line[CVEC_LINE_MAX];

    do {
      fgets(local_line, CVEC_LINE_MAX, cvfp_);
    } while (local_line[0] == '#');
    if (local_line[1] == 'E' || local_line[1] == 'O') {
      cv_file_type = CV_V1;
    }
    else {
      cv_file_type = CV_V2;
    }
    fclose(cvfp_);
    cvfp_ = fopen(cvfn_.c_str(), "r");
    if (!cvfp_) {
      cerr << "Error opening connection vector file" << endl;
    }
  }
  if (cv_file_type == CV_V1) {
    return read_one_cvec_v1();
  }
  else if (cv_file_type == CV_V2) {
    return read_one_cvec_v2();
  }
  return 0;
}

	#define A	0
	#define B	1
	#define TA	2
	#define TB	3
void Tmix::read_one_cvec_v1_helper(ConnVector * cv, ADU * adu, int last_state,
	unsigned long last_time_value, unsigned long last_initiator_time_value,
	unsigned long last_acceptor_time_value, bool pending_initiator, bool pending_acceptor) {
			if (last_time_value != 0 ) {
				adu = new ADU(last_time_value, 0, 0);
				if (last_state == TA)
					cv->add_ADU(adu, INITIATOR);
				else if (last_state == TB)
					cv->add_ADU(adu, ACCEPTOR);
				else
					delete adu;
				adu = NULL;
			}
			if (pending_initiator == true) {
				adu = new ADU(last_initiator_time_value, 0, 0);
				cv->add_ADU(adu, INITIATOR);
				adu = NULL;
			}
			if (pending_acceptor == true) {
				adu = new ADU(last_acceptor_time_value, 0, 0);
				cv->add_ADU(adu, ACCEPTOR);
				adu = NULL;
			}
}

ConnVector*
Tmix::read_one_cvec_v1() {
	/*
	 * Need to remember the last time we got...
	 * if the last time we got was the same type
	 * as the current time we are reading
	 * then we set send_wait and not receive_wat
	 */
		ConnVector* cv=NULL;
		
	int init_win, acc_win;
	unsigned long long int start_time;
	int init_ADU_count, acc_ADU_count;
	int global_id;
	int int_junk;
	float float_junk;
		
		int last_state, indexA, indexB, last_time;
		ADU* adu = NULL;

	        last_time = 0;
		unsigned long last_time_value = 0;
		unsigned long last_initiator_time_value = 0;
		unsigned long last_acceptor_time_value = 0;
		bool pending_initiator = false;
		bool pending_acceptor = false;
		last_state = TB;
		indexA = indexB = 0;
		do{
			if(line == NULL || line[0] == '#' || !strcmp(line,"\n")){
				;
			}else if(line[0] == 'S' || line[0] == 'C') { //SEQ or CONC header
				if(cv!=NULL) {
					read_one_cvec_v1_helper(cv, adu, last_state, last_time_value,
						last_initiator_time_value, last_acceptor_time_value, pending_initiator,
						pending_acceptor);
					return cv;
				}
				last_state = TB;
				indexA = indexB = 0;
				
				// == connection vector construction == //
				if(line[0] == 'S'){
					if(sscanf(line, "SEQ %llu %u %u %u",
								&(start_time), &(init_ADU_count), &(int_junk), &(global_id)) != 4){
						fprintf(stderr, "Parse error on: %s\n", line);
						cv = NULL; // check the logic on this...
						goto err;
					}
					cv = new ConnVector(global_id, (double)start_time/1000000.0, SEQ);
				}else if(line[0] == 'C'){
					if(sscanf(line, "CONC %llu %u %u %u %u",
								&(start_time), &(init_ADU_count), &(acc_ADU_count), &(int_junk),
								&(global_id)) != 5){
						fprintf(stderr, "Parse error on: %s\n", line);
						cv = NULL; // check the logic on this...
						goto err;
					}
					cv = new ConnVector(global_id, (double)start_time/1000000.0, CONC, init_ADU_count, acc_ADU_count);
				}
				// == these are set thru setters now == //
/*				cv->rwin_init = -1;
				cv->rwin_recv = -1;
				cv->delay = -1;
				cv->loss_init = -1;
				cv->loss_recv = -1;*/
			}else{
				if(cv == NULL){
					fprintf(stderr, "Missing SEQ|CONC header for: %s\n", line);
					goto err;
				}
				if(line[0] == 'w'){ //Window size
					// == don't read directyl in, but to local var first, then assign
					if(sscanf(line, "w %d %d", &(init_win), &(acc_win)) != 2){
						fprintf(stderr, "Parse error on: %s\n", line);
						goto err;
					}
					cv->set_init_win(init_win, pkt_size_);
					cv->set_acc_win(acc_win, pkt_size_);
				}else if(line[0] == 'r'){ //Per flow delay
					if(sscanf(line, "r %d", &(int_junk)) != 1){
						fprintf(stderr, "Parse error on: %s\n", line);
						goto err;
					}
				}else if(line[0] == 'l'){ //Per flow loss
					if(sscanf(line, "l %f %f", &(float_junk), &(float_junk)) != 2){
						fprintf(stderr, "Parse error on: %s\n", line);
						goto err;
					}
				}else{
					char str[10];
					int tmp;
					if(sscanf(line, "%s %d", str, &tmp) != 2){
						fprintf(stderr, "Parse error on: %s\n", line);
						goto err;
					}
					// seq has recv waits
					if(cv->get_type() == SEQ){ // SEQ related ADU units
						if(line[0] == '>'){
							if(last_state == A || last_state == B){
								fprintf(stderr, "Got ADU A after ADU %d\n", last_state);
								goto err;
							}
							// ====== //
							if(last_state == TA){
								adu = new ADU(last_time_value, 0, tmp);
								cv->add_ADU(adu, INITIATOR);
								adu = NULL;
								if (cv->get_type() == SEQ && tmp != FIN) {
									cv->incr_init_ADU_count();
								}
							}
							else if (last_state == TB) {
								adu = new ADU(0, last_time_value, tmp);
								cv->add_ADU(adu, INITIATOR);
								adu = NULL;
								if (cv->get_type() == SEQ && tmp != FIN) {
									cv->incr_init_ADU_count();
								}
							}
							// ====== //
							last_state = A;
						}else if(line[0] == '<'){
							if(last_state == A || last_state == B){
								fprintf(stderr, "Got ADU B after ADU %d\n", last_state);
								goto err;
							}
							// ====== //
							if(last_state == TB){
								adu = new ADU(last_time_value, 0, tmp);
								cv->add_ADU(adu, ACCEPTOR);
								adu = NULL;
								if (cv->get_type() == SEQ && tmp != FIN) {
									cv->incr_acc_ADU_count();
								}
							}
							else if (last_state == TA) {
								adu = new ADU(0, last_time_value, tmp);
								cv->add_ADU(adu, ACCEPTOR);
								adu = NULL;
								if (cv->get_type() == SEQ && tmp != FIN) {
									cv->incr_acc_ADU_count();
								}
							}
							// ====== //
							last_state = B;
						}else if(line[0] == 't'){
							if(last_state == TA || last_state == TB){
								fprintf(stderr, "Got ADU TA/B after ADU %d\n", last_state);
								goto err;
							}
							if(last_state == A){
								// ======= //
								last_time_value = tmp;
								if (tmp == 0) last_time_value = 1;
								last_time = TA;
								// ======= //
								last_state = TA;
							}else if(last_state == B){
								// ======= //
								last_time_value = tmp;
								if (tmp == 0) last_time_value = 1;
								last_time = TB;
								// ======= //
								last_state = TB;
							}
						}
						// conc has send waits...
					}else if(cv->get_type() == CONC){ // CONC related ADU units
						if(line[0] == 'c' && line[1] == '>'){
							if(last_state == A){
								fprintf(stderr, "Got ADU c> after c>\n");
								goto err;
							}

							// ========= //
							if (last_initiator_time_value != 0) {
								// add the ADU w/ the last time value
								adu = new ADU(last_initiator_time_value, 0, tmp);
								cv->add_ADU(adu, INITIATOR);
								adu = NULL;
								pending_initiator = false;
							}
							else {
								// add a 0 0 adu
								adu = new ADU(0, 0, tmp);
								cv->add_ADU(adu, INITIATOR);
								adu = NULL;
								pending_initiator = false;
							}
							// ========= //

							last_state = A;
						}else if(line[0] == 'c' && line[1] == '<'){
							if(last_state == B){
								fprintf(stderr, "Got ADU c< after c<\n");
								goto err;
							}

							// ========= //
							if (last_acceptor_time_value != 0) {
								// add the ADU w/ the last time value
								adu = new ADU(last_acceptor_time_value, 0, tmp);
								cv->add_ADU(adu, ACCEPTOR);
								adu = NULL;
								pending_acceptor = false;
							}
							else {
								// add a 0 0 adu
								adu = new ADU(0, 0, tmp);
								cv->add_ADU(adu, ACCEPTOR);
								adu = NULL;
								pending_acceptor = false;
							}
							// ========= //

							last_state = B;
						}else if(line[0] == 't' && line[1] == '>'){
							if(last_state == TA){
								fprintf(stderr, "Got ADU t> after t>\n");
								goto err;
							}

							// ==== addition/modification ==== //
							last_initiator_time_value = tmp;
							if (tmp == 0) last_initiator_time_value = 1;
							pending_initiator = true;
							// ==== end ==== //

							last_state = TA;
						}else if(line[0] == 't' && line[1] == '<'){
							if(last_state == TB){
								fprintf(stderr, "Got ADU t< after t<\n");
								goto err;
							}

							// ==== addition/modification ==== //
							last_acceptor_time_value = tmp;
							if (tmp == 0) last_acceptor_time_value = 1;
							pending_acceptor = true;
							// ==== end ==== //

							last_state = TB;
						}
					}
				}
			}
		}while(fgets(line, CVEC_LINE_MAX, cvfp_)!=NULL);
		if (cv != NULL) {
			read_one_cvec_v1_helper(cv, adu, last_state, last_time_value,
				last_initiator_time_value, last_acceptor_time_value, pending_initiator,
				pending_acceptor);
			return cv;
		}
		return NULL;
	
	err:
		fprintf(stderr,"cvecid=%lu\n", cv->get_ID());
		if (cv != NULL)
			delete cv;
		return NULL;
}
#undef A
#undef B
#undef TA
#undef TB

ConnVector*
Tmix::read_one_cvec_v2()
{
	char sym;       /* first token in line - a symbol */
	/* items to read */
	unsigned long start, id, send, recv, size;
	int numinit, numacc, initwin, accwin;
	ConnVector* cv;
	ADU* adu;
	bool started_one = false;

	while (!feof (cvfp_)) {
		/* look at the first character in the line */
		sym = fpeek (cvfp_);

		if (sym == '#') {
			/* skip comments */
			fscanf (cvfp_, "%*[^\n]\n");
			continue;
		}

		/* break if we've already read one cvec */
		if (started_one && (sym == 'S' || sym == 'C')) {
			break;
		}
		started_one = true;

		if (sym == 'S') {
			/* start of new SEQ connection vector */
			fscanf (cvfp_, "%c %lu %*d %*d %lu\n", &sym, &start, 
				&id);
			cv = (ConnVector*) new ConnVector (id, 
 							   (double) 
							   start/1000000.0, 
							   SEQ); 
		}
		else if (sym == 'C') {
			/* start of new CONC connection vector */
			fscanf (cvfp_, "%c %lu %d %d %*d %lu\n", &sym,
				&start, &numinit, &numacc, &id);
			cv = (ConnVector*) new ConnVector (id, 
							   (double)
							   start/1000000.0, 
							   CONC, numinit, 
							   numacc); 
		}
		else if (sym == 'w') {
			/* window size */
			fscanf (cvfp_, "%c %d %d\n", &sym, &initwin, &accwin);
			cv->set_init_win (initwin, pkt_size_);
			cv->set_acc_win (accwin, pkt_size_);
		}
		else if (sym == 'I') {
			/* new initiator ADU */
			fscanf (cvfp_, "%c %lu %lu %lu\n", &sym, &send, &recv, 
				&size);
			adu = (ADU*) new ADU (send, recv, size);
			cv->add_ADU (adu, INITIATOR);
			if (cv->get_type() == SEQ && size != FIN) {
				cv->incr_init_ADU_count();
			}
		}
		else if (sym == 'A') {
			/* new acceptor ADU */
			fscanf (cvfp_, "%c %lu %lu %lu\n", &sym, &send, &recv, 
				&size);
			adu = (ADU*) new ADU (send, recv, size);
			cv->add_ADU (adu, ACCEPTOR);
			if (cv->get_type() == SEQ && size != FIN) {
				cv->incr_acc_ADU_count();
			}
		}
		else {
			/* skip the line */
			fscanf (cvfp_, "%*[^\n]\n");
		}
	}

	/* return the cv ptr */
	return cv;
}

void Tmix::start()
{            
	/* make sure that there are an equal number of acceptor nodes
           and initiator nodes */
	if (next_init_ind_ != next_acc_ind_) {
		fprintf (stderr, "Error: %d initiators and %d acceptors", 
			 next_init_ind_, next_acc_ind_);
	}
	total_nodes_ = next_init_ind_;
	running_  = true;

	/* read from the connection vector file */
	ConnVector* cv;
	int i=0;
	while (!feof (cvfp_) && i < (int) step_size_) {
		/* read in step_size_ ConnVectors and add to list */
		cv = read_one_cvec();
		if (cv != NULL) {
			connections_.push_back(cv);
		}
		else {
			fprintf (stderr, "cv is null!\n");
		}
		i++;
	}

	if (debug_ > 3) {
		fprintf (stderr, "Tmix %s> %d connections\n", 
			 name(), (int) connections_.size());
	}

	/* Start scheduling connections */

	/* initialize the iterator to the next active connection */
	init_next_active();

	/* schedule the first connection */
	timer_.resched(get_next_start());
}

void Tmix::stop()
{
	running_ = false;
}

int Tmix::command(int argc, const char*const* argv) {
	if (argc == 2) {
		if (strcmp (argv[1], "start") == 0) {
			start();
			return (TCL_OK);
		}
		else if (strcmp (argv[1], "stop") == 0) {
			stop();
			return (TCL_OK);
		}
		else if (strcmp (argv[1], "active-connections") == 0) {
			if (outfp_) {
				fprintf (outfp_, "%lu ", active_connections_);
				fflush (outfp_);
			}
			else
				fprintf (stderr, "%lu ", active_connections_);
			return (TCL_OK);
		}
		else if (strcmp (argv[1], "total-connections") == 0) {
			if (outfp_) {
				fprintf (outfp_, "%lu ", total_connections_);
				fflush (outfp_);
			}
			else
				fprintf (stderr, "%lu ", total_connections_);
			return (TCL_OK);
		}
	}
	else if (argc == 3) {
		if ((!strcmp (argv[1], "set-init")) 
		    || (!strcmp (argv[1], "init"))) {
			if (next_init_ind_ >= MAX_NODES) {
				return (TCL_ERROR);
			}
			initiator_[next_init_ind_] = (Node*) 
				lookup_obj(argv[2]);
			if (initiator_[next_init_ind_] == NULL) {
				return (TCL_ERROR);
			}
			next_init_ind_++;
			return (TCL_OK);			
		}
		else if ((!strcmp (argv[1], "set-acc")) || 
			 (!strcmp (argv[1], "acc"))) {
			if (next_acc_ind_ >= MAX_NODES) {
				return (TCL_ERROR);
			}
			acceptor_[next_acc_ind_] = (Node*) 
				lookup_obj(argv[2]);
			if (acceptor_[next_acc_ind_] == NULL)
				return (TCL_ERROR);
			next_acc_ind_++;
			return (TCL_OK);			
                    
		}
		else if (strcmp (argv[1], "set-outfile") == 0) {
			outfp_ = fopen (argv[2], "w");
			if (outfp_)
				return (TCL_OK);
			else 
				return (TCL_ERROR);
		}
		else if (strcmp (argv[1], "set-cvfile") == 0) {  
			cvfp_ = fopen (argv[2], "r");
			cvfn_ = argv[2];
			if (cvfp_)
				return (TCL_OK);
			else 
				return (TCL_ERROR);
		}
		else if (strcmp (argv[1], "set-ID") == 0) {
			ID_ = (int) atoi (argv[2]);
			return (TCL_OK);
		}
		else if (strcmp (argv[1], "set-run") == 0) {
			run_ = (int) atoi (argv[2]);
			return (TCL_OK);
		}
		else if (strcmp (argv[1], "set-debug") == 0) {
			debug_ = (int) atoi (argv[2]);
			return (TCL_OK);
		}
		else if (strcmp (argv[1], "set-warmup") == 0) {
			warmup_ = (int) atoi (argv[2]);
			return (TCL_OK);
		}
		else if (!strcmp (argv[1], "set-TCP")) {
			strcpy (tcptype_, argv[2]);
			return (TCL_OK);
		}
		else if(strcmp(argv[1],"set-pkt-size")==0) {
			pkt_size_ = atoi(argv[2]);
			return(TCL_OK);
		}
		else if(strcmp(argv[1],"set-step-size")==0) {
			step_size_=atol(argv[2]);
			return(TCL_OK);
		}
		else if (strcmp (argv[1], "recycle") == 0) {
			FullTcpAgent* tcp = (FullTcpAgent*) 
				lookup_obj(argv[2]);
			
			/* Wait to recycle until peer is done */

			/* find app associated with this agent */
			map<string, TmixApp*>::iterator iter = 
				appActive_.find(tcp->name());
			if (iter != appActive_.end()) {
				TmixApp* app = iter->second;
				TmixApp* peer_app = app->get_peer();
				app->stop();   /* indicate app is done */
				if (!app->get_running() && 
				  !peer_app->get_running()) {
					FullTcpAgent* peer_tcp = 
						(FullTcpAgent*) lookup_obj 
						(peer_app->get_agent_name());
					/* remove apps from active pools 
					   and put in inactive pools */
					if (debug_ > 1)
						fprintf (stderr, 
						 "App (%s)> DONE at %f\n", 
							 app->id_str(), now());

					/* delete the ConnVector associated
					   with the apps */
					connections_.remove(app->get_cv());
					delete app->get_cv();

					/* recycle everything */
					recycle ((FullTcpAgent*) tcp);
					recycle ((FullTcpAgent*) peer_tcp);
					recycle (app);
					recycle (peer_app);
				}
				else {
					return(TCL_OK); 
				}
			}
                             
			active_connections_--;  
			return (TCL_OK);
		}
	}
	return TclObject::command(argc, argv);
}


/*::::::::::::::::::::::::: TIMER HANDLER classes :::::::::::::::::::::::::::*/

void TmixTimer::expire(Event* = 0) 
{

}

void TmixTimer::handle(Event* e)
{
	double next_start;

	TimerHandler::handle(e);

	/* if stopped, wait for all active connections to finish */
	if (!mgr_->running()) {
		if (mgr_->get_active() != 0) {
			resched (1);              // check back in 1 second...
		}
		return;
	}

	/* setup new connection */
	mgr_->setup_connection();

    	/* increment ptr to next active connection */
	mgr_->incr_next_active();

	if (mgr_->scheduled_all()) {
		/* we've just scheduled the last connection */
		mgr_->stop();
		return;
	}
	
	/* schedule time for next connection */
	next_start = mgr_->get_next_start() - mgr_->now();
	if (next_start < 0) {
		/* if we're behind, don't schedule a negative time */
		fprintf (stderr, "Tmix (%s) resched time reset from %f to 0\n",
			 mgr_->name(), next_start);
		next_start = 0;
	}
	if (mgr_->debug() > 1) {
		fprintf (stderr, "\tnext connection scheduled in %f s...\n", 
			 next_start);
	}
	resched (next_start);
}


void TmixAppTimer::expire(Event* = 0)
{
}

void TmixAppTimer::handle(Event* e)
{
	TimerHandler::handle(e);  /* resets the timer status to timer_idle */
	t_->timeout();
}

/*:::::::::::::::::::::::: TMIX APPLICATION class ::::::::::::::::::::*/

static class TmixAppClass : public TclClass {
public:
	TmixAppClass() : TclClass("Application/Tmix") {}
	TclObject* create(int, const char*const*) {
		return (new TmixApp);
	}
} class_app_tmix;

TmixApp::~TmixApp()
{
	Tcl& tcl = Tcl::instance();
	if (agent_ != NULL) {
		tcl.evalf ("delete %s", agent_->name());
	}
}

char* TmixApp::id_str()
{
	static char appname[50];
	sprintf (appname, " %s / %lu / %lu / %d.%d ", name(), id_, 
		 get_global_id(), agent_->addr(), agent_->port());
	return appname;
}

bool TmixApp::ADU_empty() 
{
	if (type_ == INITIATOR) {
		return (cv_->get_init_ADU().empty());
	}
	else {
		return (cv_->get_acc_ADU().empty());
	}
}

bool TmixApp::end_of_ADU() 
{
	if (type_ == INITIATOR) {
		return ((int) cv_->get_init_ADU().size() == ADU_ind_);
	}
	else {
		return ((int) cv_->get_acc_ADU().size() == ADU_ind_);
	}
}

ADU* TmixApp::get_current_ADU()
{
	if (end_of_ADU() || ADU_empty()) {
		sent_last_ADU_ = true;
		return NULL;
	}
	
	if (type_ == INITIATOR) {
		return (cv_->get_init_ADU()[ADU_ind_]);
	}
	else {
		return (cv_->get_acc_ADU()[ADU_ind_]);
	}
}

bool TmixApp::send_first()
{
	ADU* adu = get_current_ADU();
	if (!adu)
		return false;

	return (adu->get_recv_wait_sec() == 0);
}

void TmixApp::start()
{
	/* initialize variables */
	running_ = true;
	ADU_ind_ = 0;

	get_agent()->listen();
	if (mgr_->debug() > 3) {
		fprintf (stderr, "App (%s)> listening\n", id_str());
	}

	/* if we send data first (before waiting for recv, send now */
	if (send_first()) {
		timer_.resched(0);      // send data now
	}
}

void TmixApp::stop()
{
	running_ = false;
}

void TmixApp::recycle()
{
	id_ = 0;
	ADU_ind_ = 0;
	cv_ = NULL;
        running_ = false;
	total_bytes_ = 0;
	expected_bytes_ = 0;
	peer_ = NULL;
        mgr_ = NULL;
	agent_ = NULL;
	timer_.force_cancel();
	sent_last_ADU_ = false;
	waiting_to_send_ = false;
}


void TmixApp::timeout()
{ 
	/* look at the next thing to send */
	unsigned long size;
	double send_wait;
	ADU* adu = get_current_ADU();
	if (!adu)
		return;

	waiting_to_send_ = false;

	size = adu->get_size();

	if (size == FIN && peer_->sent_last_ADU()) {
		/* time to close the connection */
		agent_->sendmsg (1, "MSG_EOF"); /* close() is not equivalent */
		if (mgr_->debug() > 3) {
			fprintf (stderr,"App (%s)> closing connection at %f\n",
				 id_str(), mgr_->now());
		}
		return;
	} 

	if (size == FIN && !peer_->sent_last_ADU()) {
		/* let the peer know that we're done and wait 
		   for them to close the connection */
		if (mgr_->debug() > 3) {
			fprintf (stderr, 
				 "App (%s)> last ADU (skipped FIN) at %f\n",
				 id_str(), mgr_->now());
		}
		sent_last_ADU_ = true;
		return;
	}

	/* time to send an ADU */

	/* let the peer know what we're sending */
	peer_->incr_expected_bytes (size);

	if (mgr_->debug() > 3) {
		fprintf (stderr, "App (%s)> sent %lu B (%d expected) at %f\n", 
			 id_str(), size, peer_->get_expected_bytes(),
			 mgr_->now());
	}
	
	/* need to know if this is the last ADU to send */
	ADU_ind_++;     		/* move to the next ADU */
	if (end_of_ADU()) {
		/* that was the last ADU */
		sent_last_ADU_ = true;
		if (mgr_->debug() > 3) {
			fprintf (stderr, "App (%s)> sent last ADU", id_str());
		}
		if (peer_->sent_last_ADU()) {
			/* peer is also done, close connection */
			agent_->sendmsg (size, "MSG_EOF"); 
			if (mgr_->debug() > 3) {
				fprintf (stderr,", closing connection at %f\n",
					 mgr_->now());
			}
		}
		else {
			/* wait for peer to close connection */
			agent_->sendmsg (size);
			if (mgr_->debug() > 3) {
				fprintf (stderr, "\n");
			}
		}
		return;
	}

	/* we still have more to send */
	agent_->sendmsg (size);
	
	adu = get_current_ADU();
	if (!adu)
		return;
	
	send_wait = adu->get_send_wait_sec();

	if (send_wait == 0) {
		/* nothing to send yet */
		return;
	}

	/* set timer to send next ADU */
	waiting_to_send_ = true;
	timer_.resched (send_wait);
	if (mgr_->debug() > 3) {
		fprintf (stderr, "App (%s)> resched %f s at %f\n", id_str(), 
			 send_wait, mgr_->now());
	}
}

void TmixApp::recv(int bytes)
{
	double recv_wait;
	ADU* adu;

	/* we've received a packet */
	if (mgr_->debug() > 4)
		fprintf (stderr, "App (%s)> received %d B at %f\n",
			 id_str(), bytes, mgr_->now());

	total_bytes_ += bytes;
	if (total_bytes_ >= expected_bytes_) {
		/* finished receiving ADU from peer */
		if (mgr_->debug() > 3)
			fprintf (stderr, 
				 "App (%s)> received total %d B at %f\n",
				 id_str(), expected_bytes_, mgr_->now());

		total_bytes_ -= expected_bytes_;
		expected_bytes_ = 0;

		if (mgr_->debug() > 3 && total_bytes_ > 0) {
			fprintf (stderr, 
				 "App (%s)> %d bytes still left\n",
				 id_str(), total_bytes_);
		}

		adu = get_current_ADU();
		if (!adu)
			return;

		recv_wait = adu->get_recv_wait_sec();
		if (recv_wait == 0) {
			/* no action needed because we're not waiting for 
			 * the peer to finish */
			return;
		}
	
		if (peer_->waiting_to_send()) {
			/* no action needed because we're waiting for
			 * the peer to send another ADU */
			return;
		}
		
		/* schedule next send */
		timer_.resched (recv_wait);
		if (mgr_->debug() > 3) {
			fprintf (stderr, "App (%s)> resched %f s at %f\n", 
				 id_str(), recv_wait, mgr_->now());
		}
	}
}
