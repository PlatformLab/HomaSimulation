/*
 * Copyright (c) 1997-2001 University of Southern California.
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
 *      This product includes software developed by the Information Sciences
 *      Institute of the University of Southern California.
 * 4. Neither the name of the University nor of the Institute may be used
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
 */

#ifndef parser_h
#define parser_h

#include <stdio.h>
//#include "trace.h"
struct TraceEvent;

class Attribute {
public:
  char flag;
  char * value_type;
  void * value_ptr;
  void * other_ptr;  // Used for backwards compatibility hacks
  char * default_value;
  bool required;
  bool dual_use;
  bool seen;
  char * label;
  Attribute * next;

public:
  // value_ptr is the location to store a read value in the TraceEvent record
  Attribute(char _flag, void * _value_ptr, char * _value_type, char * _default_value, 
            bool _required, char * _label);

  void dualUse(void * _value_ptr, char * _value_type, char * _label);

  char * parseValue(char * run, char * line);
  void setDefaultValue();

private:
  double determineAngle(char * buffer);  // Used to extract the layout angle for "orientation"
                                         // Value type
};


class TraceSyntax {
public:
  char type;
  char * label;
  Attribute * attributes;
  int total_required_attributes;
  int number_of_attributes;
  TraceSyntax * next;

private:
  Attribute * last_attribute;
  
public:
  TraceSyntax(char event_id, char * _label);
  Attribute * addAttribute(char flag, void * value_ptr, char * type,
                           char * default_value, bool required,
                           char * label);
  Attribute * addAttribute(char flag, void * value_ptr, char * type,
                           char * default_value, bool required,
                           char * label, void * other_ptr);
  void setDefaultAttributeValues();
};

class ParseTable {
public:
  //<zheng: +++>
  static bool nam4wpan;
  static int wpan_bradius;
  //</zheng: +++>
  TraceSyntax * syntax_list;
private:
  TraceSyntax * syntax_list_tail;
  TraceEvent * pending;

public:
  ParseTable(TraceEvent * _pending); // pending is where to put value when
                                    // nam trace line is parsed
  int print(FILE * stream);
  int printLatex(FILE * stream);
  bool parseLine(char * line);
  TraceSyntax * addTraceSyntax(char id, char * label);
};

char * eatSpaces(char * marker);
char * advanceToSpace(char * marker);
char * advanceToNextFlag(char * marker);
char * advanceToEndofLine(char * marker);
char * extractString(char * original, char * output);
#endif

