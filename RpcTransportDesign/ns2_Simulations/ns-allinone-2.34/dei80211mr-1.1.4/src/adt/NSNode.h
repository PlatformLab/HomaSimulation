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


#include "Position.h"


		/** 
		 * Impements the position interface
		 * 
		 *
		 * @author Simone Merlin
		 * @see Position 
		 *		  
		 */



class NSNode : public CPosition {

private:
	NSNode* prev; /**< a pointer to the next node in the list */

	NSNode* next; /**< a pointer to the prewvious node in the list */

	Object* elem; /**< a pointer to the element in the node */
public:
//Costruttore
	 	
		/** 
		 * Constructor: creates a new node and insert in the list. Links to the 
		 * prevous and next nodes are specified
		 *
		 * @param newPrev  the position pointer of the previuos node, 
		 * @param newNext  the position pointer of the next node 
		 * @param elem  te object to be embedded
		 *		  
		 */
	NSNode (NSNode* newPrev,NSNode* newNext,Object* elem) ;

	 NSNode() ;
 	 ~NSNode() ;
//Metodi d'interfaccia
public:
		/** 
		 * returns a pointer to the element in the node
		 *
		 * @return a pointer to the element, 
		 */
	Object* element();
//Metodi di lettura
		/** 
		 * returns a pointer to the next node
		 *
		 * @return a pointer to the next node, 
		 */
	NSNode* getNext();
	
		/** 
		 * returns a pointer to the previous node
		 *
		 * @return a pointer to the previous  node, 
		 */
	NSNode* getPrev();

//Metodi scrittori
		/** 
		 * set the position of the next node
		 *
		 * @param newnext apointer to the position of the next node 
		 */
	void setNext(NSNode* newnext);
	
		/** 
		 * set the position of the previous node
		 *
		 * @param newprew a pointer toi the  position of the previous node 
		 */
	void setPrev(NSNode* newprev);

		/** 
		 * set the element
		 *
		 * @param newElem a pointer to the elemet to be inserted
		 */
	void setElement(Object* newElem);
};
