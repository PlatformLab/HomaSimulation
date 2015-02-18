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

//					CLASSE DLList

// Realizza una sequenza posizionale implementata per mezzo di
// una lista a collegamento doppio.
// Implementa PositionalSequence e quindi PositionalContainer e Container.

// NOTE
// funzionante , eccezioni gestite

#include "DLList.h"
#include "stdio.h"
#include <iostream>
using namespace std;

//Costruttore

 DLList::DLList() {
		numElement = 0;
		head = new NSNode();
		tail = new NSNode();
		head->setNext(tail);
		tail->setPrev(head);
	}

//Metodi privati

 DLList::~DLList() {
	CPosition* p;
	CPosition* t; 
	t= head;
	while (t != tail){
		p = after(t);
		delete t;
		}
	}



  NSNode* DLList::checkPosition(CPosition* p ){
		if(p==head) {//cerr<< "Invalid CPosition";
		return 0;}
		if(p==tail) {//cerr<< "Invalid CPosition";
		return 0;
		}
		NSNode* temp = (NSNode*)p;
		return temp;

	}

//METODI DI INTERFACCIA



 	int  DLList::size(){
		return numElement;
	}
	bool DLList::isEmpty(){

	bool l = (numElement == 0);


		return l;
	}

	Object* DLList:: replace (CPosition* p,Object* elem)
{
 		NSNode* x = checkPosition(p);
		if(x!=0){
		Object* old = x->element();
		x->setElement(elem);
		return old;} else return 0;
	}

    void  DLList::swap (CPosition* p, CPosition* q)
	{
		  NSNode* pp = checkPosition(p);
    	  	  NSNode* qq = checkPosition(q);
		if (pp!= 0 and qq!= 0){
		  Object* tmp ;
		  tmp = pp->element();
		  pp->setElement(qq->element());
		  qq->setElement(tmp);
		  }
		  // else cerr<< "Posizioni errate";
	}


	CPosition* DLList:: first() {
		if (isEmpty()){ //cerr<< "Container vuoto";
		return 0;
		}
		else return head->getNext();
	}

	CPosition*  DLList::before (CPosition* p) {
		NSNode* x = checkPosition(p);
		if (x!=0){
		NSNode* prev = x->getPrev();
  		if (prev == head ){
		//cerr<< "Raggiunta testa";
		return 0;
		}
		return prev;}
		else return 0;
	}

	CPosition*  DLList::insertFirst(Object* elem){
		numElement++;
		NSNode* newNode = new NSNode(head, head->getNext(), elem);
		head->getNext()->setPrev(newNode);
		head->setNext(newNode);
		return newNode;
	}

	CPosition*  DLList::insertBefore(CPosition* p,Object* elem){
		NSNode* x = checkPosition (p);

		if (x!=0){

			if ( p == first()) return insertFirst(elem);
			else return insertAfter(before(p), elem);
		}
		else return 0;
	}

	CPosition*  DLList::insertAfter(CPosition* p,Object* elem){
		NSNode* x = checkPosition (p);

		if (x!=0){
		numElement++;
		NSNode* newNode = new NSNode(x,x->getNext(), elem);
		x->getNext()->setPrev(newNode);
		x->setNext(newNode);
		return newNode;}
		else return 0;
	}

	 CPosition*  DLList::insertLast (Object* element) {
		numElement++;
		NSNode* newnode = new NSNode(tail->getPrev(),tail,element);
		tail->getPrev()->setNext(newnode);
		tail->setPrev(newnode);
		return newnode;
	}

	 CPosition* DLList::last(){
			if (isEmpty()){
			//cerr<< "Container vuoto";
			return 0;
			}
			else return (tail->getPrev());
	}

	 CPosition* DLList::after (CPosition* p)
	{
		NSNode* pp = checkPosition(p);
		if (pp!= 0){
		if (pp->getNext()== tail){
		//cerr<< "Superati Limiti";
		return 0;
		}
		else
		return pp->getNext();
		}
		else return 0;
	}

	 Object*  DLList::remove (CPosition* p){
		NSNode* pp = checkPosition (p);
		if (pp!=0){
		NSNode* prev = pp->getPrev();
		NSNode* next = pp->getNext();
		prev->setNext(next);
		next->setPrev(prev);
		numElement--;
		return pp->element();
		}
		else return 0;
	}

	Object* DLList::removeBefore (CPosition* p)
	{
		NSNode* pp = checkPosition (p);
		if(pp!=0){
			NSNode* prev = pp->getPrev();
			if (prev == head){
				//cerr<< "Superati Limiti";
				return 0 ;
			}else{
			NSNode* prevprev = prev->getPrev();
			prevprev->setNext(pp);
			pp->setPrev(prevprev);
			numElement--;
			return prev->element();
			}
			}
			else return 0;

	}

	 Object* DLList::removeAfter (CPosition* p){
		NSNode* pp = checkPosition (p);
		if (pp!= 0){
			NSNode* next = pp->getNext();
			if (pp == tail)
			{//cerr<< "Superati Limiti";
			 return 0;
			}
			else{
			NSNode* nextnext = next->getNext();
			nextnext->setPrev(pp);
			pp->setNext(nextnext);
			numElement--;
			return next->element();
			}
			}
			else return 0;
	}

	 Object* DLList::removeFirst() {
		if (numElement == 0 ){
		// cerr<< "Container vuoto";
		return 0;
		}
		else{
		NSNode* ret = head->getNext();
		NSNode* nextnext = ret->getNext();
		head->setNext(nextnext);
		nextnext->setPrev(head);
		Object* dato=0;
		numElement--;
		dato=ret->element();

		return dato;
		}
	}

	Object* DLList:: removeLast() {
		if (numElement == 0 ){
		//cerr<< "Container vuoto";
		return 0;}
		else{
		NSNode* ret =tail->getPrev();
		NSNode* prevprev = ret->getPrev();
		tail->setPrev(prevprev);
		prevprev->setNext(tail);
		Object* dato=0;
		numElement--;

		dato=ret->element();

		return dato;}
	}

