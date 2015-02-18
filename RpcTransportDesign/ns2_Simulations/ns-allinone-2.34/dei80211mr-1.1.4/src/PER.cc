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

#include "mac.h"
#include "mac-802_11mr.h"
#include "PER.h"
#include <math.h>
#include <float.h>
#include <rng.h>

#define ABS(x) (((x)>=0) ? (x) : (-x) )

//Completion for the ADT


/// Container for the length	
class leng : public Object {
	public:
	int len;
	DLList* snr_list;

	leng(int len_){
		len=len_;
		snr_list= new DLList();
	}
	
	~leng() {
		delete snr_list;	
	}
	
};


/// Container for SNR and PER
class Snr : public Object {
	public:
	double snr;
	double per;
	
	Snr(double snr_, double per_){
		snr= snr_;
		per= per_;
	}

};


/**
 * TCL wrapper for the PER class
 * 
 */
static class PERClass : public TclClass {
public:
	PERClass() : TclClass("PER") {}
	TclObject* create(int, const char*const*) {
		return (new PER());
	}
} class_per;


PER::PER()
{
	for(int i = 0 ; i < NumPhyModes ; i++)
		PHYmod[i] = new DLList();

#ifndef COMPILING_TEST_PROGRAM
	bind("debug_", &debug_);
	bind("noise_", &noise_);
#endif

}

PER::~PER(){
 	for(int i = 0 ; i < NumPhyModes ; i++)
		delete PHYmod[i];
}



int PER::command(int argc, const char*const* argv){

	if(debug_>2) printf("Called PER::command argc=%i\n",argc);

	if (argc == 2) {	
		if (strcmp(argv[1], "print") == 0) {
		print_data();
		 return TCL_OK;
	}
	} else if (argc == 6) {

		// Command to add PER entries to PER table
		if (strcmp(argv[1], "add") == 0) {
                       
			PhyMode pm = str2PhyMode(argv[2]);

			if (pm==ModeUnknown) return(TCL_ERROR);

			int len =   atoi(argv[3]);
			double snr = atof(argv[4]);
			double  per = atof(argv[5]);

			if(debug_>2)  printf("%s=%f ------- %s=%f\n",argv[4],snr,argv[5],per);
			
			set_per(pm,len,snr,per);
			return TCL_OK;		   
		}
	}
}



int PER::get_err(PowerProfile* pp, Packet* pkt, double* snrptr, double* snirptr, double* interfptr){


	if(debug_>3)
		{

			printf("PER::get_err(): dumping everything\n");
			
			for (int i=0; i< 12; i++)
				{
					if(PHYmod[i]!=0)
						{
							NSNode* nl  = (NSNode*)(PHYmod[i]->first());
							while(nl !=0  )
								{
									cout << ((leng*)nl->element())->len << "\n";
									DLList* sl = ((leng*)nl->element())->snr_list;
									NSNode* ns =(NSNode*)sl->first();
									while( ns !=0 )
										{												
											cout << ((Snr*)ns->element())->snr << "  " ;
											cout << ((Snr*)ns->element())->per << "\n" ;
											ns  = (NSNode*)(sl->after(ns));
										}
									nl  = (NSNode*)(PHYmod[i]->after(nl));
								}
							
							
							
						}
				}
		}

	/* Hack needed not to link the whole NS with the test program */ 
#ifdef COMPILING_TEST_PROGRAM         
	double pkt_pow = 0 ;
	int len = 0;
	PhyMode pm=Mode1Mb;
	double duration = 1;
#else        
	struct hdr_cmn *hdr = HDR_CMN(pkt);
	struct MultiRateHdr *hdm = HDR_MULTIRATE(pkt);
	double pkt_pow = pkt->txinfo_.RxPr;
	double duration = hdr->txtime(); 
	PhyMode pm = hdm->mode(); 
	int len = hdr->size(); 
#endif

	double avgpower = avg(pp,duration);
	double interf = avgpower-pkt_pow;
	double snr1=DBL_MAX; 
	double snr2;
	double per1;
	double per2;


	if (ABS(noise_) > DBL_MIN)
		{
			snr1 = 10*log10(pkt_pow/(noise_));
			per1 = get_per( pm, snr1, len);
		}
	else
		per1 = 0;

	if (ABS(noise_ + interf) > DBL_MIN)
		{
			snr2 = 10*log10(pkt_pow/(noise_ + interf));
			per2 = get_per( pm, snr2, len);
		}
	else
		per2=0;

	/* Return SNR */
	if (snrptr!=NULL)
		*snrptr = snr1;

	/* Return SNIR */
	if (snirptr!=NULL)
		*snirptr = snr2;

	/* Return interfering power */
	if (interfptr!=NULL)
		*interfptr = interf;

	//	double x = (double) rand()/(double)RAND_MAX;
	double x = RNG::defaultrng()->uniform_double();

	//if ((debug_)&&(interf>1e-15))
	if ((debug_))
		{
			cout << "PER::get_err() at " << Scheduler::instance().clock() << "s\n" ;
			cout << "PER::get_err() Average received power      "<< avgpower << " W\n"; 
			cout << "PER::get_err() Signal power                "<< pkt_pow << " W\n";
			cout << "PER::get_err() Noise power                 "<< noise_ << " W\n";
			cout << "PER::get_err() Interference power          "<< interf<< " W\n";
			cout << "PER::get_err() SNR (noise only)            "<< snr1<< " dB\n";
			cout << "PER::get_err() SNIR (noise + interference) "<< snr2<< " dB\n";
			cout << "PER::get_err() PER due to noise only "<< per1 << "\n"; 
			cout << "PER::get_err() PER due to noise and interference "<< per2<< "\n";
			cout << "PER::get_err() Rand "<< x<< "\n";
		}
	
 	if (x<per1) 
		return PKT_ERROR_NOISE; 	
	else if (x<per2) 
		return PKT_ERROR_INTERFERENCE ; 
	else 
		return PKT_OK; 

}




int PER::get_per(PhyMode pm, double tsnr, double tlen, per_set* ps_){
	
	DLList* snrlist;
	NSNode* pl1 =0;
	NSNode* pl2 =0;
	NSNode* ps1 =0;
	NSNode* ps2 =0;
	NSNode* ps3 =0;
	NSNode* ps4 =0;
	
	//cerco la lunghezza
	pl1 = (NSNode*)PHYmod[pm]->first();
	 
	if(pl1 == 0) {
	  cerr << "No PER entry found for PHYMode " << PhyMode2str(pm) << endl;
		return 1;
	}
	
	while (pl1 != PHYmod[pm]->last() && ((leng*)pl1->element())->len < tlen ){
		pl1=(NSNode*)PHYmod[pm]->after(pl1);
		}
	//pl1 o e' il primo maggiore o uguale di tlen o e' l'ultimo
	
	//prendo quello precedente (potrebbe essere nullo)
	if (((leng*)pl1->element())->len > tlen)
		pl2 = (NSNode*)PHYmod[pm]->before(pl1);
	
	
	snrlist = ((leng*)pl1->element())->snr_list;
	//cerco snr nella prima lunghezza
	ps1 = (NSNode*)snrlist->first();
	
	if(ps1 == 0) {
		cerr<<"Tabella non inizializzata 2";
		return 1;
	}
	while ( ps1 != snrlist->last()  && ((Snr*)ps1->element())->snr < tsnr  ) {
		ps1=(NSNode*)PHYmod[pm]->after(ps1);
	}
	//ps1 o e' maggiore di tsnr o e' l'ultimo
	
	//prendo quello precedente
	if (((Snr*)ps1->element())->snr > tsnr)
		ps2 = (NSNode*)snrlist->before(ps1);
	
	
	//se le lunghezze erano due diverse cerco snr anche nella seconda
	if ((pl1 != pl2) & (pl2 != 0)){
	//cerr << "dicversi";
		snrlist = ((leng*)pl2->element())->snr_list;
	//cerco
		ps3 = (NSNode*)snrlist->first();
	
		if(ps3 == 0) {
		cerr<<"Tabella non inizializzata 3";
		return 1;
		}
	
		while (not (((Snr*)ps3->element())->snr >= tsnr or ps3 == snrlist->last() ) ){
			ps3=(NSNode*)PHYmod[pm]->after(ps3);
		}
	//prendo anche quello precedente
		if (((Snr*)ps3->element())->snr > tsnr)
		ps4 = (NSNode*)snrlist->before(ps3);
	
	}
	
	
	//settaggio delle variabili di uscita in base ai risultati della ricerca. 
	//I valori /1 indicano che no e
	if (ps1){
	//cerr << ps_;
	((Snr*)ps1->element())->snr;
	
	ps_->snr[0] = ((Snr*)ps1->element())->snr;
	ps_->per[0] = ((Snr*)ps1->element())->per;
	ps_->len[0] = ((leng*)pl1->element())->len;
	}else{
	ps_->snr[0] = -1;
	ps_->per[0] = -1;
	ps_->len[0] = -1;
	}
	
	
	if (ps2){
	ps_->snr[1] = ((Snr*)ps2->element())->snr;
	ps_->per[1] = ((Snr*)ps2->element())->per;
	ps_->len[1] = ((leng*)pl1->element())->len;
	}else{
	ps_->snr[1] = -1;
	ps_->per[1] = -1;
	ps_->len[1] = -1;
	}
	
	if (ps3){
	ps_->snr[2] = ((Snr*)ps3->element())->snr;
	ps_->per[2] = ((Snr*)ps3->element())->per;
	ps_->len[2] = ((leng*)pl2->element())->len;
	}else{
	ps_->snr[2] = -1;
	ps_->per[2] = -1;
	ps_->len[2] = -1;
	}
	
	if (ps4){
	ps_->snr[3] = ((Snr*)ps4->element())->snr;
	ps_->per[3] = ((Snr*)ps4->element())->per;
	ps_->len[3] = ((leng*)pl2->element())->len;
	}else{
	ps_->snr[3] = -1;
	ps_->per[3] = -1;
	ps_->len[3] = -1;
	}

	if (debug_ > 2) {
		int i;
		printf("Pointers: %x %x %x %x\n",ps1,ps2,ps3,ps4);
		fprintf(stderr,"4 PER table entries: \n");
		for(i=0; i<4; i++)
			fprintf(stderr,"\t %d %f %f  \n", ps_->len[i], ps_->snr[i], ps_->per[i] );

	}

}


void PER::set_per(PhyMode phy, int l , double snr, double per)
{

	NSNode* nl  = (NSNode*)(PHYmod[phy]->first());

	if (nl==0){
		leng* le = new leng(l);
		PHYmod[phy]->insertFirst(le);
		le->snr_list->insertFirst(new Snr(snr,per));


	}else{
		while(nl !=0 && l>((leng*)nl->element())->len)
			nl  = (NSNode*)(PHYmod[phy]->after(nl));

		if (nl==0){
			leng* le =  new leng(l);
			PHYmod[phy]->insertLast(le);
			le->snr_list->insertFirst(new Snr(snr,per));

		}else
			{

				if (((leng*)nl->element())->len==l){
					DLList* sl = ((leng*)nl->element())->snr_list;
					NSNode* ns =(NSNode*)sl->first();
					while( ns !=0 && ((Snr*)ns->element())->snr<snr )
						ns  = (NSNode*)(sl->after(ns));
					if (ns==0){
						sl->insertLast(new Snr(snr,per));
					}
					else{
						sl->insertBefore(ns,new Snr(snr,per));
					}

				}else{

					leng* le =  new leng(l);
					PHYmod[phy]->insertBefore(nl,le);
					le->snr_list->insertFirst(new Snr(snr,per));

				}
			}
	}
}




double PER::get_per(PhyMode pm, double tsnr, double tlen){

	struct per_set ps_;

	get_per( pm,  tsnr, tlen, &ps_);


	int len1 = -1;
	int len2 = -1;
	double per1 = -1;
	double per2 = -1;
	double per = -1;


	//not found entries are set equal  to the clesest one

	//cerr <<  ps_.len[0] << ps_.len[1] << ps_.len[2] << ps_.len[3];

	if(ps_.len[0]==-1){
		ps_.len[0]=ps_.len[1];
		ps_.snr[0]=ps_.snr[1];
		ps_.per[0]=ps_.per[1];
	}

	if(ps_.len[1]==-1){
		ps_.len[1]=ps_.len[0];
		ps_.snr[1]=ps_.snr[0];
		ps_.per[1]=ps_.per[0];
	}

	if(ps_.len[2]==-1){
		ps_.len[2]=ps_.len[3];
		ps_.snr[2]=ps_.snr[3];
		ps_.per[2]=ps_.per[3];
	}

	if(ps_.len[3]==-1){
		ps_.len[3]=ps_.len[2];
		ps_.snr[3]=ps_.snr[2];
		ps_.per[3]=ps_.per[2];
	}

	if (debug_ > 2) {
		int i;
		fprintf(stderr,"4 PER table entries: \n");
		for(i=0; i<4; i++)
			fprintf(stderr,"\t %d %f %f  \n", ps_.len[i], ps_.snr[i], ps_.per[i] );

	}


	// intepolation: 1) linear interpolation at the varying of the SNR for each length
	//		2) linear interpolation among lenghts 

	if (ps_.snr[1] != ps_.snr[0])
		per1 = ps_.per[0] +  (ps_.per[1]- ps_.per[0])/(ps_.snr[1]- ps_.snr[0])*(tsnr- ps_.snr[0]);
	else 
		per1 = ps_.per[1];

	if (ps_.snr[3] != ps_.snr[2])
		per2 = ps_.per[2] +  (ps_.per[3]- ps_.per[2])/(ps_.snr[3]- ps_.snr[2])*(tsnr- ps_.snr[2]);
	else
		per2 = ps_.per[2];

	if (per1!=-1 && per2!=-1){
		if (ps_.len[3] != ps_.len[1])
			per = per1 +  (per2- per1)/(ps_.len[3]- ps_.len[1])*(tlen-ps_.len[1]);
		else
			per = per1;
	}
	else 
		if (per1!=-1){
			per = per1;
		}else 
			if (per2!=-1){
				per = per2;
			}else {cerr << "No PER available";}
	return per;
}



//compute the mean average power given by the time power profile.

double PER::avg(PowerProfile* pp,double duration){

  if (debug_>1)  printf("PER::avg()  starting calculations\n");	
	

	double avg_pow = 0 ;
	for (int i=0; i< pp->getNumElements()-1; i++){
		double a =  pp->getElement(i).power;
		//double b = pp->getElement(i+1).power;
		//cerr << "pot ist" << a << " "<< b << "---\n"; 		
		double dt = (pp->getElement(i+1).time -pp->getElement(i).time);
		avg_pow = avg_pow + a * dt;

		if (debug_>1)  printf("PER::avg()  power=%2.10f \t dt=%f\n",a,dt);
	}

	double a = pp->getElement(pp->getNumElements()-1).power;
	double dt = (duration -pp->getElement(pp->getNumElements()-1).time);
	
	//	avg_pow = avg_pow + pp->getElement(pp->getNumElements()-1).power * (duration -pp->getElement(pp->getNumElements()-1).time);
	avg_pow = avg_pow + a * dt;

	if (debug_>1)  printf("PER::avg()  power=%2.10f \t dt=%f duration=%f\n",a,dt,duration);

//cerr << "average0" << pp->getElement(1).time - pp->getElement(0).time;

	avg_pow = avg_pow / duration;

	if(debug_>1) printf("PER::avg() finished calculations: at %f average power=%2.10f\n", Scheduler::instance().clock(),avg_pow);

	return avg_pow;
}









int PER::get_snr(PhyMode pm, double tper, double tlen, per_set* ps_){
	
	DLList* snrlist;
	NSNode* pl1 =0;
	NSNode* pl2 =0;
	NSNode* ps1 =0;
	NSNode* ps2 =0;
	NSNode* ps3 =0;
	NSNode* ps4 =0;
	
	//cerco la lunghezza
	pl1 = (NSNode*)PHYmod[pm]->first();
	
	if(pl1 == 0) {
		cerr<<"Tabella non inizializzata 1";
		return 1;
	}
	
	while (pl1 != PHYmod[pm]->last() && ((leng*)pl1->element())->len < tlen ){
		pl1=(NSNode*)PHYmod[pm]->after(pl1);
		}
	//pl1 o e' il primo maggiore o uguale di tlen o e' l'ultimo
	
	//prendo quello precedente (potrebbe essere nullo)
	if (((leng*)pl1->element())->len > tlen)
		pl2 = (NSNode*)PHYmod[pm]->before(pl1);
	
	
	snrlist = ((leng*)pl1->element())->snr_list;
	//cerco snr nella prima lunghezza
	ps1 = (NSNode*)snrlist->first();
	
	if(ps1 == 0) {
		cerr<<"Tabella non inizializzata 2";
		return 1;
	}
	while ( ps1 != snrlist->last()  && ((Snr*)ps1->element())->per > tper  ) {
		ps1=(NSNode*)PHYmod[pm]->after(ps1);
	}
	//ps1 o e' minore di tper o e' l'ultimo
	
	//prendo quello precedente
	if (((Snr*)ps1->element())->per < tper)
		ps2 = (NSNode*)snrlist->before(ps1);
	
	
	//se le lunghezze erano due diverse cerco snr anche nella seconda
	if ((pl1 != pl2) & (pl2 != 0)){
	//cerr << "dicversi";
		snrlist = ((leng*)pl2->element())->snr_list;
	//cerco
		ps3 = (NSNode*)snrlist->first();
	
		if(ps3 == 0) {
		cerr<<"Tabella non inizializzata 3";
		return 1;
		}
	
		while (not (((Snr*)ps3->element())->per <= tper or ps3 == snrlist->last() ) ){
			ps3=(NSNode*)PHYmod[pm]->after(ps3);
		}
	//prendo anche quello precedente
		if (((Snr*)ps3->element())->per < tper)
		ps4 = (NSNode*)snrlist->before(ps3);
	
	}
	
	
	//settaggio delle variabili di uscita in base ai risultati della ricerca. 
	//I valori /1 indicano che no e
	if (ps1){
	//cerr << ps_;
	((Snr*)ps1->element())->snr;
	
	ps_->snr[0] = ((Snr*)ps1->element())->snr;
	ps_->per[0] = ((Snr*)ps1->element())->per;
	ps_->len[0] = ((leng*)pl1->element())->len;
	}else{
	ps_->snr[0] = -123456789;
	ps_->per[0] = -1;
	ps_->len[0] = -1;
	}
	
	
	if (ps2){
	ps_->snr[1] = ((Snr*)ps2->element())->snr;
	ps_->per[1] = ((Snr*)ps2->element())->per;
	ps_->len[1] = ((leng*)pl1->element())->len;
	}else{
	ps_->snr[1] = -123456789;
	ps_->per[1] = -1;
	ps_->len[1] = -1;
	}
	
	if (ps3){
	ps_->snr[2] = ((Snr*)ps3->element())->snr;
	ps_->per[2] = ((Snr*)ps3->element())->per;
	ps_->len[2] = ((leng*)pl2->element())->len;
	}else{
	ps_->snr[2] = -123456789;
	ps_->per[2] = -1;
	ps_->len[2] = -1;
	}
	
	if (ps4){
	ps_->snr[3] = ((Snr*)ps4->element())->snr;
	ps_->per[3] = ((Snr*)ps4->element())->per;
	ps_->len[3] = ((leng*)pl2->element())->len;
	}else{
	ps_->snr[3] = -123456789;
	ps_->per[3] = -1;
	ps_->len[3] = -1;
	}

}




double PER::get_snr(PhyMode pm, double tper, double tlen){

	struct per_set ps_;

	get_snr( pm,  tper, tlen, &ps_);


	int len1 = -1;
	int len2 = -1;
	double snr1 = -123456789;
	double snr2 = -123456789;
	double snr = -123456789;


	//not found entries are set equal  to the clesest one

	//cerr <<  ps_.len[0] << ps_.len[1] << ps_.len[2] << ps_.len[3];

	if(ps_.len[0]==-1){
		ps_.len[0]=ps_.len[1];
		ps_.snr[0]=ps_.snr[1];
		ps_.per[0]=ps_.per[1];
	}

	if(ps_.len[1]==-1){
		ps_.len[1]=ps_.len[0];
		ps_.snr[1]=ps_.snr[0];
		ps_.per[1]=ps_.per[0];
	}

	if(ps_.len[2]==-1){
		ps_.len[2]=ps_.len[3];
		ps_.snr[2]=ps_.snr[3];
		ps_.per[2]=ps_.per[3];
	}

	if(ps_.len[3]==-1){
		ps_.len[3]=ps_.len[2];
		ps_.snr[3]=ps_.snr[2];
		ps_.per[3]=ps_.per[2];
	}


	// intepolation: 1) linear interpolation at the varying of the PER for each length
	//		2) linear interpolation among lenghts 
	
	if (ps_.snr[0] == ps_.snr[1])
		snr1 = ps_.snr[0];
	else
		snr1 = ps_.snr[0] +  (ps_.snr[1]- ps_.snr[0])/(ps_.per[1]- ps_.per[0])*(tper- ps_.per[0]);

	if (ps_.snr[2] == ps_.snr[3])
		snr2 = ps_.snr[2];
	else
		snr2 = ps_.snr[2] +  (ps_.snr[3]- ps_.snr[2])/(ps_.per[3]- ps_.per[2])*(tper- ps_.per[2]);
	
	if (snr1!=-123456789 && snr2!=-123456789){
		if (ps_.len[3] != ps_.len[1])
			snr = snr1 +  (snr2- snr1)/(ps_.len[3]- ps_.len[1])*(tlen-ps_.len[1]);
		else 
			snr = snr1;
	}
	else 
		if (snr1!=-123456789){
			snr = snr1;
		}else 
			if (snr2!=-123456789){
				snr = snr2;
			}else {cerr << "No SNR available " << snr << " " << pm << " " << tper <<" " << tlen << "/n";}
	return snr;
}


void PER::print_data(){

for (int i = 0 ; i<= Mode54Mb; i++) {
for (int l = 128 ; l<= 1024; l=l*2) {
for (double per = 0 ; per<1; per=per+0.1) {

printf("%d %d %f %f \n",i,l,per,get_snr((PhyMode)i, per , l));

}
}
}


}



