/*6:*/
#line 95 "gb_flip.w"

#define gb_next_rand() (*gb_fptr>=0?*gb_fptr--:gb_flip_cycle())
extern long*gb_fptr;
extern long gb_flip_cycle();

/*:6*//*11:*/
#line 222 "gb_flip.w"

extern void gb_init_rand();

/*:11*//*13:*/
#line 254 "gb_flip.w"

extern long gb_unif_rand();

/*:13*/
