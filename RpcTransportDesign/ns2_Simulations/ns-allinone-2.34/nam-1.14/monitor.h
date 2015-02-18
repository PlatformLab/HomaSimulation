#ifndef nam_monitor_h
#define nam_monitor_h

class Animation;
class NetView;
class View;
class Node;

struct MonPacket {
  int id;
};

struct MonRoute {
  int src;
  int group;
  Node *node;
};

/*MonState is used when an Animation deletes itself, but it is likely
  that another animation will be created soon that should inherit the
  monitor*/
#define MON_PACKET 1
#define MON_ROUTE 2
struct MonState {
  int type;
  union {
    MonPacket pkt;
    MonRoute route;
  };
};

class Monitor {
public:
  Monitor(int mon, Animation *a, double size);
  ~Monitor();
  void update(double now, char *result, int len);
  inline Monitor *next() const { return next_; }  
  void next(Monitor *next) { next_=next; }
  Animation *anim() const { return anim_; }
  void anim(Animation *a) { anim_=a;}
  void draw (View *nv, float x, float y);
  void size(double size);
  void draw_monitor(View *nv, float ymin, float ymax) const;

  int monitor_number() const {return mon_num_;}
  void delete_monitor_object(Animation *m);
  struct MonState *mon_state_;
protected:
  Monitor* next_;
  int mon_num_;
  Animation *anim_;
  int paint_;
  float x_;
  float y_;
  double size_;
  char label_[20];
};


#endif
