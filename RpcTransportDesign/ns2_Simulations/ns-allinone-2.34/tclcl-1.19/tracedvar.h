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
 *	This product includes software developed by the Daedalus Research
 *	Group at the University of California Berkeley.
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
 * Contributed by Giao Nguyen, http://daedalus.cs.berkeley.edu/~gnguyen
 */

#ifndef tracedvar_h
#define tracedvar_h

/*
 * If TCLCL_USE_STREAM is defined, we pick up
 * the ability to set the formatting precision/width
 * and use streams.
 * This is not compiled in by default since ns doesn't use streams.
 */

class ostrstream;
class TclObject;

class TracedVar {
public:
	TracedVar();
	virtual ~TracedVar() {}
	virtual char* value(char* buf, int buflen) = 0;
	inline const char* name() { return (name_); }
	inline void name(const char* name) { name_ = name; }
	inline TclObject* owner() { return owner_; }
	inline void owner(TclObject* o) { owner_ = o; }
	inline TclObject* tracer() { return tracer_; }
	inline void tracer(TclObject* o) { tracer_ = o; }

#ifdef TCLCL_USE_STREAM
	inline void width(int n) { iosWidth_ = n; }
	inline short iosMask() { return iosMask_; }
	inline void setf(short val) { iosMask_ |= val; }
	inline void setf(short val, short mask) {
		iosMask_ = (iosMask_ & ~mask) | (val & mask);
	}
#endif /* TCLCL_USE_STREAM */
protected:
	TracedVar(const char* name);
#ifdef TCLCL_USE_STREAM
	inline void format(ostrstream& os);
#endif /* TCLCL_USE_STREAM */

	const char* name_;	// name of the variable
	TclObject* owner_;	// the object that owns this variable
	TclObject* tracer_;	// callback when the variable is changed
#ifdef TCLCL_USE_STREAM
	unsigned char iosWidth_;	// ios format width
	unsigned char iosPrecision_;	// ios format precision
	short iosMask_;		// ios flag for setf
#endif /* TCLCL_USE_STREAM */
public:
	TracedVar* next_;	// to make a link list
};


class TracedInt : public TracedVar {
public:
	TracedInt() : TracedVar() {}
	TracedInt(int v) : TracedVar(), val_(v) {}
	virtual ~TracedInt() {}
	// implicit conversion operators
	inline operator int() { return val_; }
	// unary operators
	inline int operator++() { assign(val_ + 1); return val_; }
	inline int operator--() { assign(val_ - 1); return val_; }
	inline int operator++(int) {
		int v = val_;
		assign(v + 1);
		return v;
	}
	inline int operator--(int) {
		int v = val_;
		assign(v - 1);
		return v;
	}
	// assignment operators
	inline TracedInt& operator=(const TracedInt& v) {
		assign(v.val_);
		return (*this);
	}
	inline int operator=(int v) { assign(v); return val_; }
	inline int operator+=(int v) { return operator=(val_ + v); }
	inline int operator-=(int v) { return operator=(val_ - v); }
	inline int operator*=(int v) { return operator=(val_ * v); }
	inline int operator/=(int v) { return operator=(val_ / v); }
	inline int operator%=(int v) { return operator=(val_ % v); }
	inline int operator<<=(int v) { return operator=(val_ << v); }
	inline int operator>>=(int v) { return operator=(val_ >> v); }
	inline int operator&=(int v) { return operator=(val_ & v); }
	inline int operator|=(int v) { return operator=(val_ | v); }
	inline int operator^=(int v) { return operator=(val_ ^ v); }

	virtual char* value(char* buf, int buflen);
protected:
	virtual void assign(const int newval);
	int val_;		// value stored by this trace variable
};


class TracedDouble : public TracedVar {
public:
	TracedDouble() : TracedVar() {}
	TracedDouble(double v) : TracedVar(), val_(v) {
#ifdef TCLCL_USE_STREAM
		iosPrecision_ = 6;
#endif /* TCLCL_USE_STREAM */
	}
	virtual ~TracedDouble() {}
	inline operator double() { return val_; }
	inline double operator++() { assign(val_ + 1); return val_; }
	inline double operator--() { assign(val_ - 1); return val_; }
	inline double operator++(int) { 
		double v = val_;
		assign(v + 1);
		return v;
	}
	inline double operator--(int) {
		double v = val_;
		assign(v - 1);
		return v;
	}
	inline TracedDouble& operator=(const TracedDouble& v) {
		assign(v.val_);
		return (*this);
	}
	inline double operator=(double v) { assign(v); return val_; }
	inline double operator+=(double v) { return operator=(val_ + v); }
	inline double operator-=(double v) { return operator=(val_ - v); }
	inline double operator*=(double v) { return operator=(val_ * v); }
	inline double operator/=(double v) { return operator=(val_ / v); }

#ifdef TCLCL_USE_STREAM
	inline void precision(int n) { iosPrecision_ = n; }
#endif /* TCLCL_USE_STREAM */
	virtual char* value(char* buf, int buflen);
protected:
	virtual void assign(const double newval);
	double val_;		// value stored by this trace variable
};

#endif
