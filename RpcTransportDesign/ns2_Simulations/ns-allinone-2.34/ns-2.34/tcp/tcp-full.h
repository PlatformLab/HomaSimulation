
#ifndef ns_tcp_full_h
#define ns_tcp_full_h

#include "tcp.h"
#include "rq.h"

/*
 * most of these defines are directly from
 * tcp_var.h or tcp_fsm.h in "real" TCP
 */


/*
 * these are used in 'tcp_flags_' member variable
 */

#define TF_ACKNOW       0x0001          /* ack peer immediately */
#define TF_DELACK       0x0002          /* ack, but try to delay it */
#define TF_NODELAY      0x0004          /* don't delay packets to coalesce */
#define TF_NOOPT        0x0008          /* don't use tcp options */
#define TF_SENTFIN      0x0010          /* have sent FIN */
#define	TF_RCVD_TSTMP	0x0100		/* timestamp rcv'd in SYN */
#define	TF_NEEDFIN	0x0800		/* send FIN (implicit state) */

/* these are simulator-specific */
#define	TF_NEEDCLOSE	0x10000		/* perform close on empty */

/*
 * these are used in state_ member variable
 */

#define TCPS_CLOSED             0       /* closed */
#define TCPS_LISTEN             1       /* listening for connection */
#define TCPS_SYN_SENT           2       /* active, have sent syn */
#define TCPS_SYN_RECEIVED       3       /* have sent and received syn */
#define TCPS_ESTABLISHED        4       /* established */
#define TCPS_CLOSE_WAIT		5	/* rcvd fin, waiting for app close */
#define TCPS_FIN_WAIT_1         6       /* have closed, sent fin */
#define TCPS_CLOSING            7       /* closed xchd FIN; await FIN ACK */
#define TCPS_LAST_ACK           8       /* had fin and close; await FIN ACK */
#define TCPS_FIN_WAIT_2         9       /* have closed, fin is acked */

#define	TCP_NSTATES		10	/* total number of states */

#define TCPS_HAVERCVDFIN(s) ((s) == TCPS_CLOSING || (s) == TCPS_CLOSED || (s) == TCPS_CLOSE_WAIT)
#define	TCPS_HAVERCVDSYN(s) ((s) >= TCPS_SYN_RECEIVED)

#define TCPIP_BASE_PKTSIZE      40      /* base TCP/IP header in real life */
/* these are used to mark packets as to why we xmitted them */
#define REASON_NORMAL   0
#define REASON_TIMEOUT  1
#define REASON_DUPACK   2
#define	REASON_RBP	3	/* if ever implemented */
#define	REASON_SACK	4	/* hole fills in SACK */

/* bits for the tcp_flags field below */
/* from tcp.h in the "real" implementation */
/* RST and URG are not used in the simulator */

#define TH_FIN  0x01        /* FIN: closing a connection */
#define TH_SYN  0x02        /* SYN: starting a connection */
#define TH_PUSH 0x08        /* PUSH: used here to "deliver" data */
#define TH_ACK  0x10        /* ACK: ack number is valid */
#define TH_ECE  0x40        /* ECE: CE echo flag */
#define TH_CWR  0x80        /* CWR: congestion window reduced */


#define PF_TIMEOUT 0x04	    /* protocol defined */
#define	TCP_PAWS_IDLE	(24 * 24 * 60 * 60)	/* 24 days in secs */

class FullTcpAgent;
class DelAckTimer : public TimerHandler {
public:
	DelAckTimer(FullTcpAgent *a) : TimerHandler(), a_(a) { }
protected:
	virtual void expire(Event *);
	FullTcpAgent *a_;
};

class FullTcpAgent : public TcpAgent {
public:
	FullTcpAgent() :
		prio_scheme_(0), prio_num_(0), startseq_(0), last_prio_(0), seq_bound_(0),
		closed_(0), pipe_(-1), rtxbytes_(0), fastrecov_(FALSE),
        	last_send_time_(-1.0), infinite_send_(FALSE), irs_(-1),
        	delack_timer_(this), flags_(0),
        	state_(TCPS_CLOSED), recent_ce_(FALSE),
		  last_state_(TCPS_CLOSED), rq_(rcv_nxt_), last_ack_sent_(-1),
		  informpacer(0), enable_pias_(0), pias_prio_num_(0), pias_debug_(0) { }
		// Mohammad: added informpacer
		//Wei: add enable_pias_

	~FullTcpAgent() { cancel_timers(); rq_.clear(); }
	virtual void recv(Packet *pkt, Handler*);
	virtual void timeout(int tno); 	// tcp_timers() in real code
	virtual void close() { usrclosed(); }
	void advanceby(int);	// over-rides tcp base version
	virtual void advance_bytes(int);	// unique to full-tcp
        virtual void sendmsg(int nbytes, const char *flags = 0);
        virtual int& size() { return maxseg_; } //FullTcp uses maxseg_ for size_
	virtual int command(int argc, const char*const* argv);
       	virtual void reset();       		// reset to a known point
protected:
	virtual void delay_bind_init_all();
	virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);
	/* Shuang: priority dropping */
	virtual int set_prio(int seq, int maxseq);
	virtual int calPrio(int prio);
	virtual int byterm();
	/* Wei: PIAS priority */
	virtual int piasPrio(int bytes_sent);

	int prio_scheme_;
	int prio_num_; //number of priorities; 0: unlimited
	int prio_cap_[7];
    int enable_pias_;   //wei: enable PIAS or not
	int pias_prio_num_;	//wei: number of priorities used by PIAS (no more than 8)
    int pias_thresh_[7];    //wei: demotion thresholds of PIAS
	int pias_debug_;	//wei: debug mode for PIAS
	int startseq_;
	int last_prio_;
	int seq_bound_;
	int prob_cap_;  //change to prob mode after #prob_cap_ timeout
	int prob_count_; //current #timeouts
	bool prob_mode_;
	int last_sqtotal_;
	int cur_sqtotal_;
	int deadline; // time remain in us at the beginning
	double start_time; //start time
	int early_terminated_; //early terminated

	int closed_;
	int ts_option_size_;	// header bytes in a ts option
	int pipe_;		// estimate of pipe occupancy (for Sack)
	int pipectrl_;		// use pipe-style control
	int rtxbytes_;		// retransmitted bytes last recovery
	int open_cwnd_on_pack_;	// open cwnd on a partial ack?
	int segs_per_ack_;  // for window updates
	int spa_thresh_;    // rcv_nxt < spa_thresh? -> 1 seg per ack
	int nodelay_;       // disable sender-side Nagle?
	int fastrecov_;	    // are we in fast recovery?
	int deflate_on_pack_;	// deflate on partial acks (reno:yes)
	int data_on_syn_;   // send data on initial SYN?
	double last_send_time_;	// time of last send


	/* Mohammad: state-variable for robust
	   FCT measurement.
	*/
	int flow_remaining_; /* Number of bytes yet to be received from
			       the current flow (at the receiver). This is
			       set by TCL when starting a flow. Receiver will
			       set immediate ACKs when nothing remains to
			       notify sender of flow completion. */

	/* Mohammad: state-variable to inform
	 * pacer (TBF) of receiving ecnecho for the flow
	 */
	int informpacer;
	//abd

	// Mohammad: if non-zero, set dupack threshold to max(3, dynamic_dupack_ * cwnd)_
	double dynamic_dupack_;

	int close_on_empty_;	// close conn when buffer empty
	int signal_on_empty_;	// signal when buffer is empty
	int reno_fastrecov_;	// do reno-style fast recovery?
	int infinite_send_;	// Always something to send
	int tcprexmtthresh_;    // fast retransmit threshold
	int iss_;       // initial send seq number
	int irs_;	// initial recv'd # (peer's iss)
	int dupseg_fix_;    // fix bug with dup segs and dup acks?
	int dupack_reset_;  // zero dupacks on dataful dup acks?
	int halfclose_;	    // allow simplex closes?
	int nopredict_;	    // disable header predication
	int ecn_syn_;       // Make SYN/ACK packets ECN-Capable?
	int ecn_syn_next_;  // Make next SYN/ACK packet ECN-Capable?
				// This is used for ecn_syn_wait = 2.
	int ecn_syn_wait_;  // 0: Window of one if SYN/ACK pkt is marked.
                            // 1: Wait if SYN/ACK packet is ECN-marked.
                            // 2: Retry if SYN/ACK packet is ECN-marked.
	int dsack_;	    // do DSACK as well as SACK?
	double delack_interval_;
        int debug_;                     // Turn on/off debug output

	int headersize();   // a tcp header w/opts
	int outflags();     // state-specific tcp header flags
	int rcvseqinit(int, int); // how to set rcv_nxt_
	int predict_ok(Packet*); // predicate for recv-side header prediction
	int idle_restart();	// should I restart after idle?
	int fast_retransmit(int);  // do a fast-retransmit on specified seg
	inline double now() { return Scheduler::instance().clock(); }
	virtual void newstate(int ns);

	void bufferempty();   		// called when sender buffer is empty

	void finish();
	void reset_rtx_timer(int);  	// adjust the rtx timer

	virtual void timeout_action();	// what to do on rtx timeout
	virtual void dupack_action();	// what to do on dup acks
	virtual void pack_action(Packet*);	// action on partial acks
	virtual void ack_action(Packet*);	// action on acks
	virtual void send_much(int force, int reason, int maxburst = 0);
	virtual int build_options(hdr_tcp*);	// insert opts, return len
	virtual int reass(Packet*);		// reassemble: pass to ReassemblyQueue
	virtual void process_sack(hdr_tcp*);	// process a SACK
	virtual int send_allowed(int);		// ok to send this seq#?
	virtual int nxt_tseq() {
		return t_seqno_;		// next seq# to send
	}
	virtual void sent(int seq, int amt) {
		if (seq == t_seqno_)
			t_seqno_ += amt;
		pipe_ += amt;
		if (seq < int(maxseq_))
			rtxbytes_ += amt;
	}
	virtual void oldack() {			// what to do on old ack
		dupacks_ = 0;
	}

	virtual void extra_ack() {		// dup ACKs after threshold
		if (reno_fastrecov_)
			cwnd_++;
	}

	virtual void sendpacket(int seq, int ack, int flags, int dlen, int why, Packet *p=0);
	void connect();     		// do active open
	void listen();      		// do passive open
	void usrclosed();   		// user requested a close
	virtual int need_send();    		// send ACK/win-update now?
	virtual int foutput(int seqno, int reason = 0); // output 1 packet
	void newack(Packet* pkt);	// process an ACK
	int pack(Packet* pkt);		// is this a partial ack?
	void dooptions(Packet*);	// process option(s)
	DelAckTimer delack_timer_;	// other timers in tcp.h
	void cancel_timers();		// cancel all timers
	void prpkt(Packet*);		// print packet (debugging helper)
	char *flagstr(int);		// print header flags as symbols
	char *statestr(int);		// print states as symbols


	/*
	* the following are part of a tcpcb in "real" RFC793 TCP
	*/
	int maxseg_;        /* MSS */
	int flags_;     /* controls next output() call */
	int state_;     /* enumerated type: FSM state */
	int recent_ce_;	/* last ce bit we saw */
	int ce_transition_; /* Mohammad: was there a transition in
			       recent_ce by last ACK. for DCTCP receiver
			       state machine. */
	int last_state_; /* FSM state at last pkt recv */
	int rcv_nxt_;       /* next sequence number expected */

	ReassemblyQueue rq_;    /* TCP reassembly queue */
	/*
	* the following are part of a tcpcb in "real" RFC1323 TCP
	*/
	int last_ack_sent_; /* ackno field from last segment we sent */

	double recent_;		// ts on SYN written by peer
	double recent_age_;	// my time when recent_ was set

	/*
	 * setting iw, specific to tcp-full, called
	 * by TcpAgent::reset()
	 */
	void set_initial_window();
};

class NewRenoFullTcpAgent : public FullTcpAgent {

public:
	NewRenoFullTcpAgent();
protected:
	int	save_maxburst_;		// saved value of maxburst_
	int	recov_maxburst_;	// maxburst lim during recovery
	void pack_action(Packet*);
	void ack_action(Packet*);
};

class TahoeFullTcpAgent : public FullTcpAgent {
protected:
	void dupack_action();
};

class SackFullTcpAgent : public FullTcpAgent {
public:
	SackFullTcpAgent() :
		sq_(sack_min_), sack_min_(-1), h_seqno_(-1) { }
	~SackFullTcpAgent() { rq_.clear(); }
protected:
	virtual void delay_bind_init_all();
	virtual int delay_bind_dispatch(const char *varName, const char *localName, TclObject *tracer);

	virtual void pack_action(Packet*);
	virtual void ack_action(Packet*);
	virtual void dupack_action();
	virtual void process_sack(hdr_tcp*);
	virtual void timeout_action();
	virtual int set_prio(int seq, int maxseq);
	virtual int nxt_tseq();
	virtual int hdrsize(int nblks);
	virtual int send_allowed(int);
	virtual void sent(int seq, int amt) {
		if (seq == h_seqno_)
			h_seqno_ += amt;
		FullTcpAgent::sent(seq, amt);
	}
	virtual int byterm();

	int build_options(hdr_tcp*);	// insert opts, return len
	int clear_on_timeout_;	// clear sender's SACK queue on RTX timeout?
	int sack_option_size_;	// base # bytes for sack opt (no blks)
	int sack_block_size_;	// # bytes in a sack block (def: 8)
	int max_sack_blocks_;	// max # sack blocks to send
	int sack_rtx_bthresh_;	// hole-fill byte threshold
	int sack_rtx_cthresh_;	// hole-fill counter threshold
	int sack_rtx_threshmode_;	// hole-fill mode setting


	void	reset();
	//XXX not implemented?
	//void	sendpacket(int seqno, int ackno, int pflags, int datalen, int reason, Packet *p=0);

	ReassemblyQueue sq_;	// SACK queue, used by sender
	int sack_min_;		// first seq# in sack queue, initializes sq_
	int h_seqno_;		// next seq# to hole-fill
};

class MinTcpAgent : public SackFullTcpAgent {
public:
   virtual void timeout_action();
   virtual double rtt_timeout();
//   virtual void advance_bytes(int nb);
};

class DDTcpAgent : public SackFullTcpAgent {

	virtual void slowdown(int how);			/* reduce cwnd/ssthresh */
	virtual int byterm();
	virtual int foutput(int seqno, int reason = 0); // output 1 packet
	virtual int need_send();    		// send ACK/win-update now?

};

#endif
