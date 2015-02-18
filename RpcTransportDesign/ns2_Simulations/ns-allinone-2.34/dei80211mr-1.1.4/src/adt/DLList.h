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


#include "NSNode.h"

		/**
		* Implements a double linked list. To each element in the list it can be associated an Object
		*
		* @author Simone Merlin
		*	
		* @see Object, CPosition, NSNode
		*/



class DLList{
public:
	 int numElement;
	 NSNode* head;
	 NSNode* tail;

//Costruttore

	 DLList();
	~DLList();

//Metodi privati

		/**
		* Check if the position is head or tail 
		*
		* @param p position to check
		*	
		* @return 0 if p is head or tail, otherwise return (NSNode) p 
		*/
	NSNode* checkPosition(CPosition* p );

//METODI DI INTERFACCIA

//Metodi di Container

		/**
		* Return the number of nodes in the list 
		*	 
		* @return the number of nodes in the list 
		*/
 	int size();

//Metodi di PositionalContainer
		/**
		* Replace the old object at position p with a new object 
		*	 
		* @param p position
		* @param elem object to be inserted
		*
		*/
	Object* replace (CPosition* p,Object* elem);
 		
		/**
		* Cehck if the list is empty 
		*	 
		* @return 1 if is empty, 0 otherwise
		*/
	bool isEmpty();
    	

		/**
		* Swap two nodes of the list
		*	 
		* @param p position  1 to swap
		* @param p position  2 to swap
		*/
	void swap (CPosition* p, CPosition* q);
// Metodi di PositionalSequence


		/**
		* Returns the first element in the list
		*	 
		* @return a pointer to the node in the first position
		*/
	 CPosition* first();
	
		/**
		* Returns the node in the list, which is located before the specified one
		*	 
		* @param a pointer to the reference node
		* @return a pointer to the previous node in the list
		*/
	CPosition* before (CPosition* p);


		/**
		* Insert a new node in the first position
		*	 
		* @param elem the element to be embedded in the new node 
		* @return a pointer to newly created node
		*/
	CPosition* insertFirst(Object* elem);

		/**
		* Insert a new node in the position before the specified one
		*	 
		* @param p a pointer yo the reference node position 
		* @param elem a pointer to the element to be embedded in the new node 
		* @return a pointer to newly created node
		*/
	CPosition* insertBefore(CPosition* p,Object* elem);
	

		/**
		* Insert a new node in the position after the specified one
		*	 
		* @param p a pointer yo the reference node position 
		* @param elem a pointer to the element to be embedded in the new node 
		* @return a pointer to newly created node
		*/
	CPosition* insertAfter(CPosition* p,Object* elem);
	

		/**
		* Insert a new node in the last position
		*	 
		* @param element a pointer to the element to be embedded in the new node 
		* @return a pointer to newly created node
		*/
	CPosition* insertLast (Object* element) ;
		
		/**
		* Returns the last element in the list
		*	 
		* @return a pointer to the node in the first position
		*/
	CPosition* last();


		/**
		* Returns the node in the list, which is located after the specified one
		*	 
		* @param a pointer to the reference node
		* @return a pointer to the previous node in the list
		*/
	CPosition* after (CPosition* p);
	
		/**
		* remove a node from the list
		*	 
		* @param p pointer to the node which has to be deleted
		* @return the object that was embedded in the deleted node 		
		*/
	Object* remove (CPosition* p);
	
		/**
		* remove the node which is located before the specified position
		*	 
		* @param p pointer to the reference position
		* @return the object that was embedded in the deleted node 		
		*/
	Object* removeBefore (CPosition* p);


		/**
		* remove the node which is located after the specified position
		*	 
		* @param p pointer to the reference position
		* @return the object that was embedded in the deleted node 		
		*/
	Object* removeAfter (CPosition* p);

		/**
		* remove the first node
		*	 
		* @return the object that was embedded in the deleted node 		
		*/
	Object* removeFirst() ;

		/**
		* remove the last node
		*	 
		* @return the object that was embedded in the deleted node 		
		*/
	Object* removeLast();
};


