/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
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
 * Ported 2006 from ns-2.29 
 * by Federico Maguolo, Nicola Baldo and Simone Merlin 
 * (SIGNET lab, University of Padova, Department of Information Engineering)
 * 
 */


#include "papropagation.h"

// methods for free space model
static class PAFreeSpaceClass: public TclClass {
public:
	PAFreeSpaceClass() : TclClass("Propagation/FreeSpace/PowerAware") {}
	TclObject* create(int, const char*const*) {
		return (new PAFreeSpace);
	}
} class_pafreespace;


double PAFreeSpace::Pr(Packet* , PacketStamp *t, PacketStamp *r,   double L, double lambda)
{
  //double L = ifp->getL();		// system loss
  //double lambda = ifp->getLambda();   // wavelength

	double Xt, Yt, Zt;		// location of transmitter
	double Xr, Yr, Zr;		// location of receiver

	t->getNode()->getLoc(&Xt, &Yt, &Zt);
	r->getNode()->getLoc(&Xr, &Yr, &Zr);

	// Is antenna position relative to node position?
	Xr += r->getAntenna()->getX();
	Yr += r->getAntenna()->getY();
	Zr += r->getAntenna()->getZ();
	Xt += t->getAntenna()->getX();
	Yt += t->getAntenna()->getY();
	Zt += t->getAntenna()->getZ();

	double dX = Xr - Xt;
	double dY = Yr - Yt;
	double dZ = Zr - Zt;
	double d = sqrt(dX * dX + dY * dY + dZ * dZ);
	//printf("DISTANZA %f %f %f %f %f%\n", d, Xr, Yr, Xt, Yt);  
	// get antenna gain
	double Gt = t->getAntenna()->getTxGain(dX, dY, dZ, lambda);
	double Gr = r->getAntenna()->getRxGain(dX, dY, dZ, lambda);

	// calculate receiving power at distance
	double Pr = Friis(t->getTxPr(), Gt, Gr, lambda, L, d);
	//printf("%lf: d: %lf, Pr: %e\n", Scheduler::instance().clock(), d, Pr);

	return Pr;
}



double
PAFreeSpace::getDist(double Pr, double Pt, double Gt, double Gr, double hr, double ht, double L, double lambda)
{
        return sqrt((Pt * Gt * Gr * lambda * lambda) / (L * Pr)) /
                (4 * PI);
}

