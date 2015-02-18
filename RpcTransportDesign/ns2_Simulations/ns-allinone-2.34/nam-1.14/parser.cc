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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "parser.h"
#include "trace.h"

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
Attribute::Attribute(char _flag, void * _value_ptr, char * _value_type,
                     char * _default_value, bool _required, char * _label) {
  flag = _flag;
  value_ptr = _value_ptr;
  other_ptr = NULL;
  value_type = _value_type;
  default_value = _default_value;
  required = _required;
  seen = false;
  label = _label;
  next = NULL;  
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
char * 
Attribute::parseValue(char * run, char * line) {
  char buffer[MAXVALUE];
  double angle;

  if (!strcmp(value_type, "int")) {
    *((int *) value_ptr) = atoi(run);
    run = advanceToSpace(run);

  } else if (!strcmp(value_type, "double")) {
    *((double *) value_ptr) = atof(run);
    run = advanceToSpace(run);

  } else if (!strcmp(value_type, "string") || 
             !strcmp(value_type, "hex")) {
    run = extractString(run,(char *) value_ptr);

  } else if (!strcmp(value_type, "string l")) {
    run = extractString(run, buffer);
    // Check for use as length in link event
    if (other_ptr && isdigit(buffer[0])) {
      fprintf(stderr, "Nam syntax has changed: %sPlease use -h instead of -l for length.\n\n",line);
      *((double *) other_ptr) = atof(buffer);
    } else {
      strcpy((char *) value_ptr, buffer);
    }

  } else if (!strcmp(value_type, "color")) {
    // Checking for angle to be bacwards compatible
    // with the -o flag standing for orientation
    run = extractString(run, buffer);
    angle = determineAngle(buffer);
    if (angle == -1.0) {
     // We must have a color
     strcpy((char *) value_ptr, buffer); 
    } else { 
      if (other_ptr) {
        fprintf(stderr, "Nam syntax has changed: %sPlease use -O instead of -o for orientation.\n\n",line);
        *((double *) other_ptr) = angle;
      }
    }
  } else if (!strcmp(value_type, "time")) {
    if (*run == '*') {
      *((double *) value_ptr) = 0.0;
      run++;
    } else {
      *((double *) value_ptr) = atof(run);
      run = advanceToSpace(run);
    }

  } else if (!strcmp(value_type, "comment")) {
    if (*run == '{') {
      run++;
      // Grab first value (Source)
      ((PacketAttr *)value_ptr)->esrc = atoi(run);

      // Grab second value (Destination)
      run = advanceToSpace(run); 
      run = eatSpaces(run);
      ((PacketAttr *) value_ptr)->edst = atoi(run);

      // Eat the rest of the comment
      while (*run != '}' && *run != '\n') {
        run++;
      }

      // Eat last curly brace
      if (*run == '}') {
        run++;
      }

    } else {
      fprintf(stderr, "Error in comment field: %s\n", line);
    }

  } else if (!strcmp(value_type, "flag")) {
    if (*run == '-' || *run == '\0' || isspace(*run)) {
      // For cases where the flag is -x followed by another attribute  
      // or on the end of the line.  If -x is followed by another attribute
      // run should point to - in -n 1
      // For example -x -n 1 or -x\0 or -n 1 -x\n
      *((int *) value_ptr) = 1;
    } else if (isdigit(*run)) {
      // For intialization cases or when you get -x 2 
      *((int *) value_ptr) = atoi(run);
      run = advanceToSpace(run); 
    } else {
      // The syntax is really messed up so report an
      // error and skip over the value
      fprintf(stderr, "Invalid value for flag attribute type in line: %s\n",line);
      run = advanceToSpace(run);
    }

  } else if (!strcmp(value_type, "char")) {
    *((char *) value_ptr) = *run;
    run = advanceToSpace(run);

  } else if (!strcmp(value_type, "orientation")) {
    run = extractString(run, buffer);
    angle = determineAngle(buffer);
    if (angle == -1.0) {
     angle = 0.0;
    } 
    *((double *) value_ptr) = angle;
    
  } else if (!strcmp(value_type, "packet source")) {
    if (*run == '*') {
      *((int *) value_ptr) = -1;
      run++;
    } else {
      *((int *) value_ptr) = atoi(run);
      run = advanceToSpace(run);
    }
  } else if (!strcmp(value_type, "shape")) {
    run = extractString(run, buffer);
    // Check for being used as y_vel_ in old nam
    // syntax
    if (isdigit(buffer[0]) || (buffer[0] == '-' && isdigit(buffer[1]))) {
      if (other_ptr) {
        fprintf(stderr, "Nam syntax has changed: %sPlease use -V instead of -v for y velocity \n\n",line);
        *((double *) other_ptr) = atof(buffer);
      }
    } else {
      strcpy((char *) value_ptr, buffer); 
    }
  } else if (!strcmp(value_type, "tcl expression")) {
    //*((char **) value_ptr) = run;
    *((int *) value_ptr) = run - line;
    
    //Advance to next flag or end of line
    run = advanceToEndofLine(run);
  }
  return run;
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
void
Attribute::setDefaultValue() {
  parseValue(default_value, default_value);
}

//----------------------------------------------------------------------
//  double
//  Attribute::determineAngle(char * buffer)
//      - Determines angle based on up-right, up, down, down-left, etc
//        constants.  Also will try to extract a numeric angle from buffer
//        if unable to determine angle -1 is returned
//----------------------------------------------------------------------
double
Attribute::determineAngle(char * buffer) {
  double angle;
  if (isalpha(*buffer) || *buffer == '"' || *buffer == '#') {
    // Set angle based on constants
    if (!strcmp(buffer, "right")) {
      angle = 0.0;
    } else if (!strcmp(buffer, "up-right") || !strcmp(buffer, "right-up")) {
      angle = 0.25;
    } else if (!strcmp(buffer, "up")) {
      angle = 0.50;
    } else if (!strcmp(buffer, "up-left") || !strcmp(buffer, "left-up")) {
      angle = 0.75;
    } else if (!strcmp(buffer, "left")) {
      angle = 1.0;
    } else if (!strcmp(buffer, "left-down") || !strcmp(buffer, "down-left")) {
      angle = 1.25;
    } else if (!strcmp(buffer, "down")) {
      angle = 1.50;
    } else if (!strcmp(buffer, "down-right") || !strcmp(buffer, "right-down")) {
      angle = 1.75;
    } else {
     angle = -1.0;
    } 
  } else {
    angle = atof(buffer);
  }
  return angle;
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
TraceSyntax::TraceSyntax(char event_id, char * _label) {
  type = event_id;
  label = _label;
  attributes = NULL;
  last_attribute = NULL;
  total_required_attributes = 0;
  number_of_attributes = 0;
  next = NULL;
}


//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
Attribute * 
TraceSyntax::addAttribute(char flag, void * value_ptr, char * type,
                          char * default_value, bool required,
                          char * label) {
  Attribute * attribute = new Attribute(flag, value_ptr, type,
                                        default_value, required, label);

  if (attribute) {
    // Add attribute to list of attributes for this ParseTraceSyntax
    if (!attributes) {
      attributes = attribute;
      last_attribute = attribute;
    } else {
      last_attribute->next = attribute;
      last_attribute = attribute;
    }

    number_of_attributes++;
    if (required) {
      total_required_attributes++;
    }
  }
  return attribute;
}

//----------------------------------------------------------------------
//  Attribute * 
//  TraceSyntax::addAttribute(char flag, void * value_ptr, char * type,
//                            char * default_value, bool required,
//                            char * label, void * other_ptr) 
//
//   - other_ptr is a pointer to an alternate location to put data.
//     It is used to enable backwards compatability with the old
//     nam syntax.
//----------------------------------------------------------------------
Attribute * 
TraceSyntax::addAttribute(char flag, void * value_ptr, char * type,
                          char * default_value, bool required,
                          char * label, void * other_ptr) {
  Attribute * attribute;

  attribute = addAttribute(flag, value_ptr, type, default_value, required, label);
  if (attribute) {
   attribute->other_ptr = other_ptr;
  }

  return attribute; 
}


//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
void
TraceSyntax::setDefaultAttributeValues() {
  Attribute * attribute = attributes;
  while (attribute) {
    attribute->setDefaultValue();
    attribute->seen = false;
    attribute = attribute->next;
  }
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
//<zheng: +++>
bool ParseTable::nam4wpan = false;
int ParseTable::wpan_bradius = -1;
//</zheng: +++>
ParseTable::ParseTable(TraceEvent * _pending) {
  TraceSyntax * ts;
  
  syntax_list = NULL;
  syntax_list_tail = NULL;
  pending = _pending;


  // Comment
  ts = addTraceSyntax('#', "comment -- this line is ignored");

  // Time sync dummy event
  ts = addTraceSyntax('T', "Dummy event to be used in time synchronization");
  ts->addAttribute('t', &pending->time, "time", "-1.0", true, "time");

  // ---- Node ----
  // Wireless node stuff seems to be wierd.
  // A lot of the code dealing with wireless bases decisions on number of attributes
  // and not a wireless attribute flag. 
  ts = addTraceSyntax('n', "node");
  ts->addAttribute('t', &pending->time, "time", "-1.0", true, "time");
  ts->addAttribute('s', &pending->ne.src, "int", "-1", true, "node id"); 
  ts->addAttribute('u', &pending->ne.x_vel_, "double", "0.0", false, "x velocity");
  ts->addAttribute('U', &pending->ne.x_vel_, "double", "0.0", false, "x velocity");
  ts->addAttribute('V', &pending->ne.y_vel_, "double", "0.0", false, "y velocity");
  // old nam syntax allowed for -v to also mean y velocity
  ts->addAttribute('v', &pending->ne.mark.shape, "shape", "circle", false, "shape (circle, box, hexagon)", &pending->ne.y_vel_);
  ts->addAttribute('c', &pending->ne.node.color, "color", "black", false, "color"); 
  ts->addAttribute('z', &pending->ne.size, "double", "10.0", false, "size of node"); 
  ts->addAttribute('a', &pending->ne.node.addr, "int", "0", false, "address"); 
  ts->addAttribute('x', &pending->ne.x, "double", "0.0", false, "x location"); 
  ts->addAttribute('y', &pending->ne.y, "double", "0.0", false, "y location"); 
  ts->addAttribute('Z', &pending->ne.z, "double", "0.0", false, "z location (not supported)"); 
  ts->addAttribute('i', &pending->ne.node.lcolor, "color", "black", false, "label color");

  ts->addAttribute('b', &pending->ne.node.dlabel, "string", "", false, "label"); // Initialization
  ts->addAttribute('l', &pending->ne.node.dlabel, "string", "", false, "label");

  ts->addAttribute('o', &pending->ne.node.oldColor, "color", "gray", false, "previous color");
  ts->addAttribute('S', &pending->ne.node.state, "string", "UP", false, "state (UP, DOWN, COLOR)");
  ts->addAttribute('L', &pending->ne.node.odlabel, "string", "", false, "previous label");
  ts->addAttribute('p', &pending->ne.node.direction, "string", "", false, "label location");
  ts->addAttribute('P', &pending->ne.node.odirection, "string", "", false, "previous label location");
  ts->addAttribute('i', &pending->ne.node.lcolor, "color", "black", false, "inside label color");
  ts->addAttribute('I', &pending->ne.node.olcolor, "color", "black", false, "previous inside label color");
  ts->addAttribute('e', &pending->ne.node.dcolor, "color", "black", false, "label color");
  ts->addAttribute('E', &pending->ne.node.odcolor, "color", "black", false, "previous label color");
  ts->addAttribute('T', &pending->ne.stoptime, "double", "0.0", false, "duration of movement");
  ts->addAttribute('w', &pending->ne.wireless, "flag", "0", false, "wireless node");

  // ---- Link ----
  ts = addTraceSyntax('l', "link");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->le.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->le.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('r', &pending->le.link.rate, "double", "1.0", false, "transmission rate"); 
  ts->addAttribute('D', &pending->le.link.delay, "double", "1.0", false, "delay"); 
  ts->addAttribute('h', &pending->le.link.length, "double", "1.0", false, "length"); 
  ts->addAttribute('O', &pending->le.link.angle, "orientation", "1.0", false, "orientation"); 
  ts->addAttribute('b', &pending->le.link.dlabel, "string", "", false, "label");

  ts->addAttribute('c', &pending->le.link.color, "color", "black", false, "color");
  // old nam syntax allowed for 'o' to be color or orientation
  ts->addAttribute('o', &pending->le.link.oldColor, "color", "gray", false, "previous color", &pending->le.link.angle);  
  ts->addAttribute('S', &pending->le.link.state, "string", "UP", true, "state (UP, DOWN)"); 
  // old nam syntax allowed for 'l' to be length or label
  ts->addAttribute('l', &pending->le.link.dlabel, "string l", "", false, "label", &pending->le.link.length); 
  ts->addAttribute('L', &pending->le.link.odlabel, "string", "", false, "previous label"); 
  ts->addAttribute('e', &pending->le.link.dcolor, "color", "", false, "label color"); 
  ts->addAttribute('E', &pending->le.link.odcolor, "color", "", false, "previous label color"); 
  
  // ---- Enqueue Packet ----
  ts = addTraceSyntax('+', "enqueue packet");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->pe.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->pe.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('e', &pending->pe.pkt.size, "int", "1", false, "extent"); 
  ts->addAttribute('a', &pending->pe.pkt.attr, "int", "0", false, "packet color attribute id"); 
  ts->addAttribute('i', &pending->pe.pkt.id, "int", "0", false, "id"); 
  ts->addAttribute('l', &pending->pe.pkt.energy, "int", "0", false, "energy"); 
  ts->addAttribute('c', &pending->pe.pkt.convid, "string", "", false, "conversation"); 
  ts->addAttribute('x', &pending->pe.pkt, "comment", "{-1 -1 }", false, "comment"); 
  ts->addAttribute('p', &pending->pe.pkt.type, "string", "", false, "packet type"); 
  ts->addAttribute('k', &pending->pe.pkt.wtype, "string", "", false, "packet type"); 
  ts->addAttribute('y', &pending->pe.namgraph_flags, "comment", "{0 0}", false, "");
  ts->addAttribute('S', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('m', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('f', &pending->pe.namgraph_flags.size, "int", "0", false, "");

   
  // ---- Dequeue Packet ----
  ts = addTraceSyntax('-', "dequeue packet");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->pe.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->pe.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('e', &pending->pe.pkt.size, "int", "1", false, "extent"); 
  ts->addAttribute('a', &pending->pe.pkt.attr, "int", "0", false, "attribute"); 
  ts->addAttribute('i', &pending->pe.pkt.id, "int", "0", false, "id"); 
  ts->addAttribute('l', &pending->pe.pkt.energy, "int", "0", false, "energy"); 
  ts->addAttribute('c', &pending->pe.pkt.convid, "string", "", false, "conversation"); 
  ts->addAttribute('x', &pending->pe.pkt, "comment", "{-1 -1 }", false, "comment"); 
  ts->addAttribute('p', &pending->pe.pkt.type, "string", "", false, "packet type"); 
  ts->addAttribute('k', &pending->pe.pkt.wtype, "string", "", false, "packet type"); 
  ts->addAttribute('y', &pending->pe.namgraph_flags, "comment", "{0 0}", false, "");
  ts->addAttribute('S', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('m', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('f', &pending->pe.namgraph_flags.size, "int", "0", false, "");

  // ---- Hop ----
  ts = addTraceSyntax('h', "hop");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->pe.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->pe.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('e', &pending->pe.pkt.size, "int", "1", false, "extent"); 
  ts->addAttribute('a', &pending->pe.pkt.attr, "int", "0", false, "attribute"); 
  ts->addAttribute('i', &pending->pe.pkt.id, "int", "0", false, "id"); 
  ts->addAttribute('l', &pending->pe.pkt.energy, "int", "0", false, "energy"); 
  ts->addAttribute('c', &pending->pe.pkt.convid, "string", "", false, "conversation"); 
  ts->addAttribute('x', &pending->pe.pkt, "comment", "{-1 -1 }", false, "comment"); 
  ts->addAttribute('p', &pending->pe.pkt.type, "string", "", false, "packet type"); 
  ts->addAttribute('k', &pending->pe.pkt.wtype, "string", "", false, "packet type"); 
  ts->addAttribute('y', &pending->pe.namgraph_flags, "comment", "{0 0}", false, "");
  ts->addAttribute('S', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('m', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('f', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('R', &pending->pe.pkt.wBcastRadius, "double", "0", false, "wireless broadcast radius");
  ts->addAttribute('D', &pending->pe.pkt.wBcastDuration, "double", "0", false, "wireless broadcast duration");
   
  // ---- Receive ----
  ts = addTraceSyntax('r', "receive");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->pe.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->pe.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('e', &pending->pe.pkt.size, "int", "1", false, "extent"); 
  ts->addAttribute('a', &pending->pe.pkt.attr, "int", "0", false, "attribute"); 
  ts->addAttribute('i', &pending->pe.pkt.id, "int", "0", false, "id"); 
  ts->addAttribute('l', &pending->pe.pkt.energy, "int", "0", false, "energy"); 
  ts->addAttribute('c', &pending->pe.pkt.convid, "string", "", false, "conversation"); 
  ts->addAttribute('x', &pending->pe.pkt, "comment", "{-1 -1 }", false, "comment"); 
  ts->addAttribute('p', &pending->pe.pkt.type, "string", "", false, "packet type"); 
  ts->addAttribute('k', &pending->pe.pkt.wtype, "string", "", false, "packet type"); 
  ts->addAttribute('y', &pending->pe.namgraph_flags, "comment", "{0 0}", false, "");
  ts->addAttribute('S', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('m', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('f', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('R', &pending->pe.pkt.wBcastRadius, "double", "0", false, "wireless broadcast radius");
  ts->addAttribute('D', &pending->pe.pkt.wBcastDuration, "double", "0", false, "wireless broadcast duration");
   
  // ---- Drop Line ----
  ts = addTraceSyntax('d', "drop line");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->pe.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->pe.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('e', &pending->pe.pkt.size, "int", "1", false, "extent"); 
  ts->addAttribute('a', &pending->pe.pkt.attr, "int", "0", false, "attribute"); 
  ts->addAttribute('i', &pending->pe.pkt.id, "int", "0", false, "id"); 
  ts->addAttribute('l', &pending->pe.pkt.energy, "int", "0", false, "energy"); 
  ts->addAttribute('c', &pending->pe.pkt.convid, "string", "", false, "conversation"); 
  ts->addAttribute('x', &pending->pe.pkt, "comment", "{-1 -1 }", false, "comment"); 
  ts->addAttribute('p', &pending->pe.pkt.type, "string", "", false, "packet type"); 
  ts->addAttribute('k', &pending->pe.pkt.wtype, "string", "", false, "packet type"); 
  ts->addAttribute('y', &pending->pe.namgraph_flags, "comment", "{0 0}", false, "");
  ts->addAttribute('S', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('m', &pending->pe.namgraph_flags.size, "int", "0", false, "");
  ts->addAttribute('f', &pending->pe.namgraph_flags.size, "int", "0", false, "");
   
  // ---- Session Enqueue ----
  ts = addTraceSyntax('E', "session enqueue");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->pe.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->pe.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('e', &pending->pe.pkt.size, "int", "1", false, "extent"); 
  ts->addAttribute('a', &pending->pe.pkt.attr, "int", "0", false, "attribute"); 
  ts->addAttribute('i', &pending->pe.pkt.id, "int", "0", false, "id"); 
  ts->addAttribute('l', &pending->pe.pkt.energy, "int", "0", false, "energy"); 
  ts->addAttribute('c', &pending->pe.pkt.convid, "string", "", false, "conversation"); 
  ts->addAttribute('x', &pending->pe.pkt, "comment", "{-1 -1 }", false, "comment"); 
  ts->addAttribute('p', &pending->pe.pkt.type, "string", "", false, "packet type"); 
  ts->addAttribute('k', &pending->pe.pkt.wtype, "string", "", false, "packet type"); 
   
  // ---- session dequeue ----
  ts = addTraceSyntax('D', "session dequeue");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->pe.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->pe.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('e', &pending->pe.pkt.size, "int", "1", false, "extent"); 
  ts->addAttribute('a', &pending->pe.pkt.attr, "int", "0", false, "attribute"); 
  ts->addAttribute('i', &pending->pe.pkt.id, "int", "0", false, "id"); 
  ts->addAttribute('l', &pending->pe.pkt.energy, "int", "0", false, "energy"); 
  ts->addAttribute('c', &pending->pe.pkt.convid, "string", "", false, "conversation"); 
  ts->addAttribute('x', &pending->pe.pkt, "comment", "{-1 -1 }", false, "comment"); 
  ts->addAttribute('p', &pending->pe.pkt.type, "string", "", false, "packet type"); 
  ts->addAttribute('k', &pending->pe.pkt.wtype, "string", "", false, "packet type"); 
   
  // ---- Session Drop ----
  ts = addTraceSyntax('P', "session drop");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->pe.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->pe.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('e', &pending->pe.pkt.size, "int", "1", false, "extent"); 
  ts->addAttribute('a', &pending->pe.pkt.attr, "int", "0", false, "attribute"); 
  ts->addAttribute('i', &pending->pe.pkt.id, "int", "0", false, "id"); 
  ts->addAttribute('l', &pending->pe.pkt.energy, "int", "0", false, "energy"); 
  ts->addAttribute('c', &pending->pe.pkt.convid, "string", "", false, "conversation"); 
  ts->addAttribute('x', &pending->pe.pkt, "comment", "{-1 -1 }", false, "comment"); 
  ts->addAttribute('p', &pending->pe.pkt.type, "string", "", false, "packet type"); 
  ts->addAttribute('k', &pending->pe.pkt.wtype, "string", "", false, "packet type"); 

  // ---- Agent Event ----
  ts = addTraceSyntax('a', "agent");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->ae.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->ae.dst, "int", "-1", false, "destination id"); 
  ts->addAttribute('x', &pending->ae.agent.expired, "flag", "0", false, "remove agent"); 
//  ts->addAttribute('X', &pending->ae.agent.expired, "flag", "0", false, "remove agent"); 
  ts->addAttribute('n', &pending->ae.agent.name, "string", "", false, "agent name"); 
   

  // ---- Feature ----
  ts = addTraceSyntax('f', "feature");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->fe.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->fe.dst, "int", "-1", false, "destination id"); 
  ts->addAttribute('x', &pending->fe.feature.expired, "flag", "0", false, "remove feature"); 
  ts->addAttribute('T', &pending->fe.feature.type, "char", "", true, "type"); 
  ts->addAttribute('n', &pending->fe.feature.name, "string", "", true, "name"); 
  ts->addAttribute('a', &pending->fe.feature.agent, "string", "", true, "agent"); 
  ts->addAttribute('v', &pending->fe.feature.value, "string", "", true, "value"); 
  ts->addAttribute('o', &pending->fe.feature.oldvalue, "string", "", false, "previous value"); 
  
  // ---- Group ----
  ts = addTraceSyntax('G', "group");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('n', &pending->ge.grp.name, "string", "", true, "name"); 
  ts->addAttribute('i', &pending->ge.src, "int", "-1", true, "node id"); 
  ts->addAttribute('a', &pending->ge.grp.mbr, "int", "-1", true, "group id"); 
  ts->addAttribute('x', &pending->ge.grp.flag, "flag", "0", false, "remove from group");

  // ---- LanLink ----
  ts = addTraceSyntax('L', "lan link");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->lle.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->lle.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('o', &pending->lle.angle, "orientation", "0.0", false, "orientation"); 
  ts->addAttribute('O', &pending->lle.angle, "orientation", "0.0", false, "orientation"); 

  // ---- Mark Node ----
  ts = addTraceSyntax('m', "mark node");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('n', &pending->me.mark.name, "string", "", true, "name"); 
  ts->addAttribute('s', &pending->me.src, "int", "-1", true, "node id"); 
  ts->addAttribute('c', &pending->me.mark.color, "string", "black", false, "color"); 
  ts->addAttribute('h', &pending->me.mark.shape, "string", "circle", false, "shape (circle, square, hexagon)"); 
  ts->addAttribute('X', &pending->me.mark.expired, "flag", "0", false, "remove mark"); 
  
  // ---- Routing Event ----
  ts = addTraceSyntax('R', "routing event");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->re.src, "int", "-1", true, "source id"); 
  ts->addAttribute('d', &pending->re.dst, "int", "-1", true, "destination id"); 
  ts->addAttribute('g', &pending->re.route.group, "int", "-1", false, "multicast group"); 
  ts->addAttribute('p', &pending->re.route.pktsrc, "packet source", "-1", false, "packet source id or *"); 
  ts->addAttribute('n', &pending->re.route.neg, "flag", "0", false, "negative cache"); 
  ts->addAttribute('x', &pending->re.route.expired, "flag", "0", false, "this route timed out"); 
  ts->addAttribute('T', &pending->re.route.timeout, "double", "0.0", false, "timeout"); 
  ts->addAttribute('m', &pending->re.route.mode, "string", "iif", false, "mode (iif or oif)"); 

  // ---- Execute TCL Expression ----
  ts = addTraceSyntax('v', "execute tcl expression");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('e', &pending->ve, "tcl expression", "\n", true, "tcl script"); 
  // ---- Trace File Version ----
  ts = addTraceSyntax('V', "set trace file version");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('v', &pending->version, "string", "", true, "time"); 
  ts->addAttribute('a', &pending->dummy, "int", "0", true, "time"); 

  // ---- Use Nam Graph ----
  ts = addTraceSyntax('N', "use nam graph");
  
  // ---- Wireless Range ----
  ts = addTraceSyntax('W', "wireless range");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('x', &pending->we.x, "int", "100", true, "X"); 
  ts->addAttribute('y', &pending->we.y, "int", "100", true, "Y"); 
  
  // ---- Energy Status (Future Use) ----
  ts = addTraceSyntax('g', "energy status -- for future use");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 

  // ---- Hierarchical Address Space Configuration ----
  // initialized in tcl
  ts = addTraceSyntax('A', "hierarchical address space configuration -- initilization only");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('n', &pending->hae.hierarchy, "int", "0", false, "hierarchy"); 
  ts->addAttribute('p', &pending->hae.portshift, "int", "0", false, "port shift"); 
  ts->addAttribute('o', &pending->hae.portmask, "hex", "0xffffffff", false, "port mask"); 
  ts->addAttribute('c', &pending->hae.multicast_shift, "int", "0", false, "mulitcast shift"); 
  ts->addAttribute('a', &pending->hae.multicast_mask, "int", "0", false, "multicast mask"); 
  ts->addAttribute('h', &pending->hae.hierarchy, "int", "0", false, "hierarchy"); 
  ts->addAttribute('m', &pending->hae.nodeshift, "int", "0", false, "node shift"); 
  ts->addAttribute('s', &pending->hae.nodemask, "int", "0", false, "node mask"); 

  // ---- Color Table Configuration ----
  // initialized in tcl
  ts = addTraceSyntax('c', "color table configuration -- initialization only");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('i', &pending->ce.id, "int", "1", true, "id"); 
  ts->addAttribute('n', &pending->ce.color, "string", "black", true, "color"); 
  
  // ---- Define Packet Queue ----
  ts = addTraceSyntax('q', "create packet queue -- initialization only");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('s', &pending->qe.src, "int", "0", true, "source id"); 
  ts->addAttribute('d', &pending->qe.dst, "int", "0", true, "destination id"); 
  ts->addAttribute('a', &pending->qe.angle, "orientation", "0.0", true, "orientaion"); 

  // ---- layout lan ----
  ts = addTraceSyntax('X', "layout lan");
  ts->addAttribute('t', &pending->time, "time", "0.0", true, "time"); 
  ts->addAttribute('n', &pending->layoutle.name, "string", "", true, "name"); 
  ts->addAttribute('r', &pending->layoutle.rate, "double", "10.0", true, "rate"); 
  ts->addAttribute('D', &pending->layoutle.delay, "double", "0.0", true, "delay"); 
  ts->addAttribute('o', &pending->layoutle.angle, "orientation", "0.0", false, "orientation"); 
  ts->addAttribute('O', &pending->layoutle.angle, "orientation", "0.0", false, "orientation"); 

}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
int
ParseTable::print(FILE * stream) {
  TraceSyntax *ts;
  Attribute * attribute;
  int i = 0;
  
  ts = syntax_list;
  while (ts) {
    // Output the type
    i += fprintf(stream, "%c = %s\n", ts->type, ts->label);
    attribute = ts->attributes;
    while (attribute) {
      i += fprintf(stream, "     -%c <%s> %s\n",attribute->flag, attribute->value_type, attribute->label); 
      attribute = attribute->next;
    }
    ts = ts->next;
  }

  return i;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
int
ParseTable::printLatex(FILE * stream) {
  TraceSyntax *ts;
  Attribute * attribute;
  int i = 0;
  
  ts = syntax_list;
  while (ts) {
    // Output the type
    i += fprintf(stream, "  \\begin{tabular}{llll}\n");
    if (ts->type == '#') {
      i += fprintf(stream, "  \\%c : & %s & & \\\\\n", ts->type, ts->label);
    } else {
      i += fprintf(stream, "  %c : & %s & & \\\\\n", ts->type, ts->label);
    }

    // Display attribute flags in a latex table
    attribute = ts->attributes;
    if (attribute) {
      // 3 Columns
      while (attribute) {
        if (attribute->flag == '#') {
          i += fprintf(stream, "    &  -\\%c ", attribute->flag); 
        } else {
          i += fprintf(stream, "    &  -%c ", attribute->flag); 
        }
        i += fprintf(stream, "& <%s> ", attribute->value_type); 
        i += fprintf(stream, "& %s \\\\\n", attribute->label); 
        attribute = attribute->next;
      }
    }
    i += fprintf(stream, "  \\end{tabular}\n\n");
    ts = ts->next;
  }
  return i;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
bool
ParseTable::parseLine(char * line) {
  bool success = true;
  char * run = line; // run down the nam event line
  TraceSyntax * ts = syntax_list;
  Attribute * attribute = NULL;
  char attribute_type;

  pending->tt = *line;

  // Skip over comment lines and empty lines
  if (*run == '#' || *run == '\n' || *run == '\r') {
    //<zheng: +++>
    if (strncmp(run,"# nam4wpan #",12) == 0) {
      ParseTable::nam4wpan = true;
    }
    //</zheng: +++>
    return true;
  }
  
  // Find Syntax Type
  while (ts != NULL) {
    if (ts->type == *line) {
      break;
    }
    ts = ts->next;
  }

  if (ts != NULL) {
    // Setup Attribute List to default values
    ts->setDefaultAttributeValues();

    // Skip over Syntax Type id
    run++;
    run = eatSpaces(run);
    
    while (*run != '\n') {
    
      //Check for Attribute Flag 
      if (*run != '-' && ts->type != 'v') {
        // Special Case for tcl expression flag
        //   The old design has the syntax "v -t <time> <expression>"
        while (*run != '-' && *run != '\n' && *run != EOF) {
          run++;
        }
      }

      if (*run == '\n' || *run == EOF) {
        // Something is seriously wrong with this line in the script
        // so bail out with an error
        fprintf(stderr, "Unexpected end of line in: %s", line);
        fprintf(stderr, "Perhaps you are missing a value following the attribute flag.\n");
        if (attribute) {
          fprintf(stderr, "Last parsed attribute flag was -%c\n\n", attribute->flag);
        }
        break;
      } else if (*run != '-' && ts->type == 'v') {
        // Missing -e flag
        // Report the new syntax but setup the attribute
        // as if we are using it
        
        fprintf(stderr, "Nam syntax has changed: %s", line);
        fprintf(stderr, "Please use this format in the future.\n");
        fprintf(stderr, "v -t <time> -e <tcl expression>\n\n");
        attribute_type = 'e';
      } else {
        // Eat the dash and set the current attribute
        run++;
        attribute_type = *run;
        run++;
      }

      //Match in Table
      attribute = ts->attributes;
      while (attribute != NULL) {
        if (attribute_type == attribute->flag) {
          break;
        }
        attribute = attribute->next;
      }

      //Read in Value
      if (attribute) {
        // Eat the flag and whitespace, to end on the value
        run = eatSpaces(run);
        run = attribute->parseValue(run, line);
        attribute->seen = true;

      } else {
        fprintf(stderr, "Unknown Flag -%c in: %s\n", attribute_type, line);
        // Skip to next attribute
        while (*run != '-' && *run != '\n') {
          run++;
        }
      }
      run = eatSpaces(run);
    }

    // Check for required attributes
    attribute = ts->attributes;
    while (attribute != NULL) {
      if (attribute->required && !attribute->seen) {
        success = false;
        fprintf(stderr, "Missing required flag -%c in: %s\n", attribute->flag, line);
      }
      attribute = attribute->next;  
    }
  } else {
    fprintf(stderr, "Unknown nam event id type: %s\n", line);
    success = false;
  }
  return success;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
TraceSyntax * 
ParseTable::addTraceSyntax(char id, char * label) {
  TraceSyntax * ts;
  
  if (!syntax_list) {
    // Create list and add this as the first member
    ts = new TraceSyntax(id, label);
    syntax_list =  ts;
    syntax_list_tail = ts;
  } else {
    // Check for duplicates
    ts = syntax_list;
    while (ts) {
      if (ts->type == id) {
        break;
      }
      ts = ts->next;
    }

    if (!ts) {
      // Add it to the end of the list
      ts = new TraceSyntax(id, label);
      syntax_list_tail->next = ts;
      syntax_list_tail = ts;
    }
  }
  
  return ts;
}


//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
char *
eatSpaces(char * marker) {
  while (*marker == ' ' || *marker == '\t') {
    marker++; 
  }
  return marker;
}
//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
char *
advanceToSpace(char * marker) {
  while (!isspace(*marker)) {
    marker++; 
  }
  return marker;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
char * advanceToNextFlag(char * marker) {
  while (*marker != '-' && *marker != '\n' && *marker != '\0') {
    marker++;
  }
  return marker;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
char * advanceToEndofLine(char * marker) {
  while (*marker != '\n' && *marker != '\0') {
    marker++;
  }
  return marker;
}

//---------------------------------------------------------------------
// char *
// ParseTable::parse_string(char *orig, char * output)
//   - Extracts the string after a flag in a nam animation line.
//   - Removes any double quotes, null terminates the string 
//     and copies it into the output variable.
//   - Returns the location after the extracted string in the original
//     nam animation line.
//   - Requires that the original pointer points to the start of 
//     the string.
//
//   - Strings can be double quoted or have no quotes
//
//     Note: May need to add in \r check for Micro$oft Windows
//---------------------------------------------------------------------
char *
extractString(char * original, char * output) {
  char * run; // run along the string
  int count = 0;  // size of the copied string, should be less than MAXNAME

  run = original;

  // Skip over first double quote, if any
  if (*run == '"') {
    run++;

    // Copy over to output
    while (count < MAXNAME &&
           *run != '"' &&
           *run != '\n' &&
           *run != '\r') {
      output[count] = *run;
      run++;
      count++;
    }
  
    // Skip over last double quote, if any
    if (*run == '"') {
      run++;
    }

  } else {
    // Copy over to output
    while (count < MAXNAME &&
           *run != '"' &&
           *run != ' ' &&
           *run != '\n' &&
           *run != '\r') {
      output[count] = *run;
      run++;
      count++;
    }
  }

  // Null Terminate the String
  if (count < MAXNAME) {
    output[count] = '\0';
    count++;
  } else {
    count--;
    output[count] = '\0';
  }
      
  return run;
}
