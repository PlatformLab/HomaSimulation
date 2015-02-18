#ifndef nam_animator_h
#define nam_animator_h

#include <tclcl.h>

class NetworkAnimator : public TclObject {
public:
	NetworkAnimator() : TclObject() {}
	virtual int command(int argc, const char *const* argv);
};

#endif // nam_animator_h
