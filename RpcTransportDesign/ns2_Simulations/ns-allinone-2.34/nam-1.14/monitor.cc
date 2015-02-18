class Node;
#include "config.h"
#include "stdio.h"
#include "monitor.h"
#include "animation.h"
#include "netview.h"
#include "paint.h"

Monitor::Monitor(int mon_num, Animation *a, double size) :
  mon_num_(mon_num), anim_(a), size_(size)
{
  paint_=Paint::instance()->thin();
  mon_state_=a->monitor_state();
  sprintf(label_, "%d", mon_num);
}

Monitor::~Monitor()
{
  if (mon_state_!=NULL)
    delete(mon_state_);
}

void Monitor::update(double now, char *result, int len)
{
  if (anim_!=NULL)
    anim_->monitor(this, now, result, len);
  else
    sprintf(result, "Not visible");
}

void Monitor::size(double size) {
  size_=size;
}

/*XXX there must be a cleaner way to do this*/
/*we need coordinates from the animation itself, and information about
  the size of the whole model from the netmodel.  Thus we have two draw
  methods, one from the animation that sets up data and one from the
  netmodel that actually does the drawing*/

void Monitor::draw(View */*nv*/, float x, float y)
{
  x_=x; 
  y_=y;
}

void Monitor::draw_monitor(View *nv, float ymin, float /*ymax*/) const
{
  if (anim_==NULL)
    return;
  float delta = 0.5 * size_;
  nv->line(x_,y_,x_,ymin-delta,paint_);
  nv->rect(x_-delta, ymin-delta, x_+delta, ymin-(size_+delta), paint_);
  nv->string(x_, ymin-size_, size_, label_, ANCHOR_CENTER);
}

void Monitor::delete_monitor_object(Animation *m)
{
  /*need to check this because we can sometimes hand a monitor over
    to a new animation just before the old animation calls this*/
  if (anim_==m)
    anim_=NULL;
}
