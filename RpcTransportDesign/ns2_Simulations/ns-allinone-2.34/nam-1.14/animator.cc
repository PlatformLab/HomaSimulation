// $Header: /cvsroot/nsnam/nam-1/animator.cc,v 1.1 1998/07/02 00:53:38 haoboy Exp $ 

#include "animator.h"

class NetworkAnimatorClass : TclClass {
public: 
	NetworkAnimatorClass() : TclClass("Animator") {}
	TclObject* create(int, const char*const*) {
		return (new NetworkAnimator());
	}
} networkanimator_class;

// We put it here in C++ because we need to keep a pointer to it 
// in NetModel, etc.
int NetworkAnimator::command(int argc, const char *const* argv) 
{
	// In case we want to put something here.
	return TclObject::command(argc, argv);
}
