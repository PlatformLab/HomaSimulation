/*
 * Copyright (c) 2001 University of Southern California.
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

#ifndef nam_queuehandle_h
#define nam_queuehandle_h


#include <tclcl.h>
#include "animation.h"


class QueueHandle : public Animation, public TclObject {
public:
  QueueHandle(const char * type, int id, double _size);
  QueueHandle(Edge * edge);

  inline int number() {return number_;}
  virtual int classid() const { return ClassQueueHandleID; }
  inline const char * name() const {return (ns_type_);}
  virtual void reset(double);

  void attachTo(Edge * edge);
  void clearEdge();
  Edge * getEdge() {return edge_;}

  inline double x() const {return x_;}
  inline double y() const {return y_;}
  inline double width() const {return width_;}
  inline double height() const {return height_;}
 
  void setWidth(double w) {width_ = w;}
  void setHeight(double h) {height_ = h;}


  void place();
  const char* info() const;

  void setType(const char * type);
  void setLimit(int limit) {limit_ = limit;}

  void setSecondsPerByte(double spb) {secsPerByte_ = spb;}

  void setMaxQueue(int max) {maxqueue_ = max;}
  void setBuckets(int buckets) {buckets_ = buckets;}

  void setSharedBufferSize(int b) {blimit_ = b;}
  void setQuantum(int quantum) {quantum_ = quantum;}
  void setMask(bool mask) {mask_ = mask;}

  void setBytes(bool bytes) {bytes_ = bytes;}
  void setQueueInBytes(bool qib) {queue_in_bytes_ = qib;}
  void setThreshold (double min ) {thresh_ = min;}
  void setMaximumThreshold(double max ) {maxthresh_ = max;}
  void setMeanPacketSize(int mean ) {mean_pktsize_ = mean;}
  void setQueueWeight(double weight ) {q_weight_ = weight;}
  void setWaitInterval(bool wait ) {wait_ = wait;}
  void setLinterm(double linterm) {linterm_ = linterm;}
  void setBitMarking(bool bitmarking) {setbit_ = bitmarking;}
  void setREDDropTail(bool droptail) {drop_tail_ = droptail;}

  virtual double distance(double x, double y) const;
  void color(const char* name);
  virtual void size(double s);
  inline double size() const {return (size_);}

  int inside(double, float, float) const;
  virtual void update_bb();

  virtual void draw(View * view, double now);

  int writeNsScript(FILE *file);

  const char * property();
  const char * getProperties(char * type); 
  int command(int argc, const char * const * argv);


private:
  void setDefaults(); 

public:
  QueueHandle * next_queue_handle_;  // Used by editornetmodel to track
  Edge * edge_;


protected:
  int number_;

  double width_;
  double height_;
  double x_, y_;
  double angle_;
  double size_;
  char * color_;

  char * type_;
  char * ns_type_;

  // DropTail
  int limit_;  // max queue size

  // FairQueue (FQ)
  double secsPerByte_;

  //StocasticFairQueue (SFQ)
  int maxqueue_;
  int buckets_;

  // DeficitRoundRobin (DRR)
  // buckets_ - the total number of bucket to used for hashing of flows
  int blimit_;  // shared buffer size in bytes
  int quantum_; // how much each flow can send during it's turn (bytes)
  bool mask_; // if true then a flow is determined by having the same node
             // ids (traffic betweeen nodes is one flow) otherwise flows 
             // have both the same node ids and the same port ids
 
  // Random Early Detection gatways (RED)
  bool bytes_; // Set to true to enable "byte-mode" RED where the size 
               // of arriving packets affect the likelihood of marking
               // (dropping) packets
  bool queue_in_bytes_; // set to true to measure the average queue size 
                        // in bytes rather than packets
  double thresh_; // minimum threshold for the average queue size in packets
  double maxthresh_; // maximum threshhold for the average queue size in packets
  int mean_pktsize_; // a rough estimate of the average packet size in bytes
  double q_weight_; // used in exponential average queue size for
                    // calculating the average queue size
  bool wait_; // set to true to maintain an interval between dropped packets
  double linterm_; // maximum drop probability = 1/linterm
  bool setbit_; // Set to true to mark packets by setting the congestion
                // indication bit in packet headers rather than drop packets
  bool drop_tail_; // set to true to use drop tail rather than random drop
                   // when the queue overflows or the average queue size
                   // exceeds maxthresh_
           
  
  

};



#endif
