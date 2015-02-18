/* $Id: eval.c,v 1.1 1996/10/04 13:33:28 ewz Exp $
 * 
 * eval.c -- evaluation routines.
 *
 */

#include <stdio.h>
#include <malloc.h>
#include "gb_graph.h"
#include "gb_dijk.h"
#include "eval.h"
#include <assert.h>

int finddegdist(Graph *g, int** degdist)
{
    Vertex *v;
    Arc *a;
    int max = 0, *deg, idx, i;

    deg = (int *)malloc((g->n)*sizeof(int));
    for (v = g->vertices,i=0; i<g->n; i++,v++) {
	deg[i] = 0;
	for (a = v->arcs; a; a = a->next)
		deg[i]++;
	if (deg[i] > max) max = deg[i];
    }
    *degdist = (int *)calloc(max+1,sizeof(int));
    for (v = g->vertices,i=0; i < g->n; i++, v++) {
	(*degdist)[deg[i]]++;
    }
    return max;
}

#define first(g)  g->vertices
#define BIGINT 0x7fffffff
#define MAXBINS	100
#define BINCONST  10		/* number of bins in pathlen dist = n*BINCONST */

static int *depth;
static int min, max;
static float avg;

void
dopaths(Graph *g, enum Field f0, enum Field f1, int *rmin, int *rmax, float *ravg)
{
    Vertex *u, *v;
    Arc *a;
    int idx, i,j, sum;

    depth = (int *)calloc(g->n,sizeof(int)); 

    /* note that bins really should be calculated using scale 	*/
    /*	 e.g., bins = c*scale, where c is some constant		*/
    /* scale can be extracted from g->id, but I don't want 	*/
    /*	 to bother with that right now				*/
    /*
     *    bins = BINCONST*g->n;
     *    pathlendist = (int *)calloc(bins+1,sizeof(int)); 
     *    maxpathlen = 0;
     */

    max = 0;
    min = BIGINT;
    sum = 0;
    for (v = first(g),i=0; i< g->n; i++,v++) {
        twofield_sptree(g,v,f0,f1);	/* sets mdist for each node */
        depth[i] = 0;	/* paranoia */
        for (u = first(g),j=0; j<g->n; j++,u++) { 
	    if (u->mdist > depth[i]) depth[i] = u->mdist;

	    /* if (u->dist > maxpathlen) maxpathlen = u->dist; */
	    /* pathlendist[bins] counts all lengths >= bins */
	    /* if (u->dist > bins) pathlendist[bins]++; */
	    /* else pathlendist[u->dist]++; */
	} 
	sum += depth[i];
	if (depth[i] < min) min = depth[i];
        if (depth[i] > max) max = depth[i];
    }
    avg = (float)sum/g->n;
    *rmax = max;
    *rmin = min;
    *ravg = avg;
}

/* convert depth data into a distribution */
/* assumes dopaths has already been called, and that min, max, depth */
/*     have valid data */
void
dodepthdist(Graph *g, int** ddist)
{
int num, i;

  num = max - min + 1;
  *ddist = (int *)calloc(num,sizeof(int));
  for (i = 0; i < g->n; i++) 
    (*ddist)[depth[i] - min]++;
}


#define rank z.I
#define parent y.V
#define untagged x.A
#define link w.V
#define min v.V

#define idx(g,v) v-g->vertices

/* count bicomponents. */
/* verbose=0 means no printouts, just return the number. */
int bicomp(Graph *g,int verbose)
{
    register Vertex *v;
    long            nn;
    Vertex          dummy;
    Vertex         *active_stack;
    Vertex         *vv;
    Vertex         *artic_pt = NULL;

    int count = 0;	

    for (v = g->vertices; v < g->vertices + g->n; v++) {
	v->rank = 0;
	v->untagged = v->arcs;
        v->link = NULL;
    }
    nn = 0;
    active_stack = NULL;
    dummy.rank = 0;

    for (vv = g->vertices; vv < g->vertices + g->n; vv++)
	if (vv->rank == 0) {
	    v = vv;
	    v->parent = &dummy;
	    v->rank = ++nn;
	    v->link = active_stack;
	    active_stack = v;
	    v->min = v->parent;
	    do {
		register Vertex *u;
		register Arc   *a = v->untagged;
		if (a) {
		    u = a->tip;
		    v->untagged = a->next;
		    if (u->rank) {
			if (u->rank < v->min->rank)
			    v->min = u;
		    } else {
			u->parent = v;
			v = u;
			v->rank = ++nn;
			v->link = active_stack;
			active_stack = v;
			v->min = v->parent;
		    }
		} else {
		    u = v->parent;
		    if (v->min == u)	/* 19: */
			if (u == &dummy) {
			    if (verbose) {
			    if (artic_pt)
				printf(" and %d (this ends a connected component of the graph)\n", idx(g, artic_pt));
			    else
				printf("Isolated vertex %d\n", idx(g, v));
			    }
			    active_stack = artic_pt = NULL;
			} else {
			    register Vertex *t;

			    if (artic_pt && verbose)
				printf(" and articulation point %d\n", 
				idx(g, artic_pt));
			    t = active_stack;
			    active_stack = v->link;
			    if (verbose) printf("Bicomponent %d", idx(g,v));
			    count++;
			    if (verbose) {
			    if (t == v)
				putchar('\n');
			    else {
				printf(" also includes:\n");
				while (t != v) {
				    printf(" %d (from %d; ..to %d)\n",
				       idx(g,t),idx(g,t->parent),idx(g,t->min));
				    t = t->link;
				}
			    }
			    }
			    artic_pt = u;
			}
		    else if (v->min->rank < u->min->rank)
			u->min = v->min;
		    v = u;
		}
	    }

	    while (v != &dummy);
	}
    return count;
}


void twofield_sptree(g,u,f0,f1)
Graph*g;
Vertex*u;
enum Field f0,f1;
{
	Vertex *v, *t;
	Arc *r;
  	int newdist;

	for (t = g->vertices; t < g->vertices + g->n; t++) {
		t->backlink = NULL;
		t->dist = BIGINT;
		t->mdist = 0;
	}
	u->backlink = u;
	u->dist = 0;
	u->mdist = 0;
	t = u;
	(*init_queue)(0L);	/* queue contains only unknown v's */
	while (t != NULL) {
	/* invariant: u is known => u->mdist and u->dist are correct */
	    for (r = t->arcs; r; r=r->next) {
		v = r->tip;
		/* newdist = t->dist + r->len; */
		switch (f0) {
		case Len: newdist = t->dist + r->len; break;
		case A: newdist = t->dist + r->a.I; break;
		case B: newdist = t->dist + r->b.I; break;
		case Hops: newdist = t->dist + 1;
		}
		
		if (v->backlink) { /* v was seen but not known */
		    if (newdist < v->dist) {
			v->backlink = t;
			/* v->hops = t->hops + 1; */
			switch (f1) {
			case Len: v->mdist = t->mdist + r->len; break;
			case A: v->mdist = t->mdist + r->a.I; break;
			case B: v->mdist = t->mdist + r->b.I; break;
			case Hops: v->mdist = t->mdist + 1;
			}
			(*requeue)(v,newdist);
		    }
	   	}
		else { /* newly seen */
		    v->backlink = t;
		    /* v->hops = t->hops + 1; */
		    switch (f1) {
			case Len: v->mdist = t->mdist + r->len; break;
			case A: v->mdist = t->mdist + r->a.I; break;
			case B: v->mdist = t->mdist + r->b.I; break;
			case Hops: v->mdist = t->mdist + 1;
			}
		    (*enqueue)(v,newdist);
		}
	    }
	    t = (*del_min)();
        }

}


