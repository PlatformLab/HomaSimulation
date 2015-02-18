/*
 * Copyright (c) 1993 Regents of the University of California.
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
 * This code derived from InterViews library which is covered
 * by the copyright below.
 */
/*
 * Copyright (c) 1987, 1988, 1989, 1990, 1991 Stanford University
 * Copyright (c) 1991 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Stanford and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Stanford and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 *
 * IN NO EVENT SHALL STANFORD OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */

/*
 * Implementation of transformation matrix operations.
 */

#include "sincos.h"
#include "transform.h"

static const double RADPERDEG = M_PI/180.0;

Transform::Transform()
{
	clear();
}

void Transform::clear()
{
	identity_ = 1;
	mat00 = mat11 = 1;
	mat01 = mat10 = mat20 = mat21 = 0;
}

Transform::Transform(const Transform& t)
{
	mat00 = t.mat00;	mat01 = t.mat01;
	mat10 = t.mat10;	mat11 = t.mat11;
	mat20 = t.mat20;	mat21 = t.mat21;
	update();
}

Transform::Transform(float a00, float a01, float a10,
		     float a11, float a20, float a21)
{
	mat00 = a00;	mat01 = a01;
	mat10 = a10;	mat11 = a11;
	mat20 = a20;	mat21 = a21;
	update();
}

int Transform::operator ==(const Transform& t) const
{
	if (identity_)
		return t.identity_;

	if (t.identity_)
		return (0);

	return (mat00 == t.mat00 && mat01 == t.mat01 &&
		mat10 == t.mat10 && mat11 == t.mat11 &&
		mat20 == t.mat20 && mat21 == t.mat21);
}

int Transform::operator !=(const Transform& t) const
{
	if (identity_)
		return (!t.identity_);

	if (t.identity_)
		return (1);

	return (mat00 != t.mat00 || mat01 != t.mat01 ||
		mat10 != t.mat10 || mat11 != t.mat11 ||
		mat20 != t.mat20 || mat21 != t.mat21);
}

Transform& Transform::operator =(const Transform& t)
{
	mat00 = t.mat00;
	mat01 = t.mat01;
	mat10 = t.mat10;
	mat11 = t.mat11;
	mat20 = t.mat20;
	mat21 = t.mat21;
	update();
	return (*this);
}

void Transform::update()
{
	identity_ = (
		     mat00 == 1 && mat11 == 1 &&
		     mat01 == 0 && mat10 == 0 && mat20 == 0 && mat21 == 0
		     );
}

void Transform::translate(float dx, float dy)
{
	mat20 += dx;
	mat21 += dy;
	update();
}

void Transform::scale(float sx, float sy)
{
	mat00 *= sx;
	mat01 *= sy;
	mat10 *= sx;
	mat11 *= sy;
	mat20 *= sx;
	mat21 *= sy;
	update();
}

void Transform::skew(float sx, float sy)
{
	mat01 += mat00*sy;
	mat10 += mat11*sx;
    update();
}

void Transform::rotate(float angle)
{
	float tmp1, tmp2, m00, m01, m10, m11, m20, m21;

	angle *= RADPERDEG;
	tmp1 = cos(angle);
	tmp2 = sin(angle);
    
	m00 = mat00*tmp1;
	m01 = mat01*tmp2;
	m10 = mat10*tmp1;
	m11 = mat11*tmp2;
	m20 = mat20*tmp1;
	m21 = mat21*tmp2;

	mat01 = mat00*tmp2 + mat01*tmp1;
	mat11 = mat10*tmp2 + mat11*tmp1;
	mat21 = mat20*tmp2 + mat21*tmp1;
	mat00 = m00 - m01;
	mat10 = m10 - m11;
	mat20 = m20 - m21;
	update();
}

void Transform::premultiply(const Transform& t)
{
	float tmp1 = mat00;
	float tmp2 = mat10;
	
	mat00  = t.mat00*tmp1 + t.mat01*tmp2;
	mat10  = t.mat10*tmp1 + t.mat11*tmp2;
	mat20 += t.mat20*tmp1 + t.mat21*tmp2;
	
	tmp1 = mat01;
	tmp2 = mat11;
	
	mat01  = t.mat00*tmp1 + t.mat01*tmp2;
	mat11  = t.mat10*tmp1 + t.mat11*tmp2;
	mat21 += t.mat20*tmp1 + t.mat21*tmp2;
	update();
}

void Transform::postmultiply(const Transform& t)
{
	float tmp = mat00*t.mat01 + mat01*t.mat11;
	mat00 = mat00*t.mat00 + mat01*t.mat10;
	mat01 = tmp;
	
	tmp = mat10*t.mat01 + mat11*t.mat11;
	mat10 = mat10*t.mat00 + mat11*t.mat10;
	mat11 = tmp;
	
	tmp = mat20*t.mat01 + mat21*t.mat11;
	mat20 = mat20*t.mat00 + mat21*t.mat10;
	mat21 = tmp;
	
	mat20 += t.mat20;
	mat21 += t.mat21;
	update();
}    

void Transform::invert()
{
	float d = det();
	float t00 = mat00;
	float t20 = mat20;
	
	mat20 = (mat10*mat21 - mat11*mat20)/d;
	mat21 = (mat01*t20 - mat00*mat21)/d;
	mat00 = mat11/d;
	mat11 = t00/d;
	mat10 = -mat10/d;
	mat01 = -mat01/d;
	update();
}

void Transform::map(float& x, float& y) const
{
	float tx = x;
	x = tx*mat00 + y*mat10 + mat20;
	y = tx*mat01 + y*mat11 + mat21;
}

void Transform::map(float x, float y, float& tx, float& ty) const
{
	tx = x*mat00 + y*mat10 + mat20;
	ty = x*mat01 + y*mat11 + mat21;
}

void Transform::imap(float& tx, float& ty) const
{
	float d = det();
	float a = (tx - mat20) / d;
	float b = (ty - mat21) / d;
	
	tx = a*mat11 - b*mat10;
	ty = b*mat00 - a*mat01;
}

void Transform::imap(float tx, float ty, float& x, float& y) const
{
	float d = det();
	float a = (tx - mat20) / d;
	float b = (ty - mat21) / d;
	
	x = a*mat11 - b*mat10;
	y = b*mat00 - a*mat01;
}
