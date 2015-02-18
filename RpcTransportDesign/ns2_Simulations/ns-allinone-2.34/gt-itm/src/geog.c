/* $Id: geog.c,v 1.1 1996/10/04 13:36:46 calvert Exp $
 *
 * geog.c -- routines to construct various flavors of semi-geometric graphs
 *           using the Stanford GraphBase tools.
 *
 */

#include <stdio.h>
/* #include <sys/types.h>		for NBBY */
/* instead define as foll */
#ifndef NBBY
#define NBBY 8
#endif

#ifndef FBSD
#include <alloca.h>
#endif
#include <assert.h>
#include <string.h>		/* for strchr() */
#include "gb_graph.h"
#include "math.h"
#include "geog.h"
#include "limits.h"

#define STACKMAX	0x400	/* 1K bytes max in stack */

/* Convention for trouble codes:
 *  - GEO adds 1-3,
 *  - HIER adds 4-6,
 *  - TS adds 7-9,
 */

#define GEO_TRBL	1
#define HIER_TRBL	4


#define sub	u.G		/* domain graph vertex expands into	*/

#ifndef u_char
typedef unsigned char u_char;
#endif 

#ifndef u_long
typedef unsigned long u_long;
#endif

static char geogId[]="$Id: geog.c,v 1.1 1996/10/04 13:36:46 calvert Exp $";

double
distance(Vertex *u, Vertex *v)
{
double dx, dy;
double foo;

    dx = (double) u->xpos - v->xpos;
    dy = (double) u->ypos - v->ypos;
    foo = (dx*dx) + (dy*dy);
    return sqrt(foo);
}

/* Distance between two vertices, rounded to the nearest integer. */
long
idist(Vertex *u, Vertex *v)
{
double dx, dy;
double foo;

    dx = (double) u->xpos - v->xpos;
    dy = (double) u->ypos - v->ypos;
    foo = (dx*dx) + (dy*dy);
    return (long) rint(sqrt(foo));
}

int
printparms(char *buf,geo_parms *pp)
{
        sprintf(buf,"{%ld,%d,%d,%.3f,%.3f,%.3f}",
                pp->n, pp->scale,pp->method,
                pp->alpha,pp->beta, pp->gamma);
        /* with Sys V this would be easier */
        return strlen(buf);
}

/* randomize an array of longs, keeping the mean constant */
/* doesn't let anything get below 1, i.e. won't work with negatives */
void
randomize(long* a, long size, long mean, int iters)
{
register i,indx;

   for (i=0; i<size; i++)       /* initialize */
        a[i] = mean;

   if (size < 2) return;	/* no point */

   for (i=0; i<iters; i++) {
        indx = gb_next_rand()%size;
        if (a[indx] > 1) {       /* first decrease one */
            a[indx]--;
            a[gb_next_rand()%size]++;   /* then increase one */
        }
   }
}

/*
 * geo(seed, <param structure>)
 *  Creates a gb_graph of n nodes with edges distributed probabilistically.
 *  These graphs are constructed so as to be easy to display graphically.
 *  0. nodes are placed on a grid "scale" units on a side, at most one
 *     node per grid point.
 *     Each node's (integer) position on the grid is kept in the aux fields
 *     x.I and y.I, which are renamed (in geog.h) to xpos and ypos.
 *      If perturb is nonzero, each node's position will also be
 *      offset by a random amount that is uniformly distributed over the
 *      interval [0,resolution/2*scale].
 *  1. Edge is placed between two nodes by a probabilistic method, which
 *     is determined by the "method" parameter.  Edge is placed with
 *     probability p, where p is calculated by one of the methods below,
 *     using:
 *       alpha, beta, gamma: input parameters,
 *       L is scale * sqrt(2.0): the max distance between two points,
 *       d: the Euclidean distance between two nodes.
 *       e: a random number uniformly distributed in [0,L]
 *
 *     Method 1: (Waxman's RG2, with alpha,beta := beta,alpha)
 *          p = alpha * exp(-e/L*beta)
 *     Method 2: (Waxmans's RG1, with alpha,beta := beta, alpha)
 *          p = alpha * exp(-d/L*beta)
 *     Method 3: (Pure random graph)
 *          p = alpha
 *     Method 4: ("EXP" - another distance varying function)
 *	    p = alpha * exp(-d/(L-d))
 *     Method 5: (Doar-Leslie, with alpha,beta := beta,alpha, ke=gamma) 
 *	    p = (gamma/n) * alpha * exp(-d/(L*beta))
 *     Method 6: (Locality with two regions)
 *          p = alpha     if d <= L*gamma,
 *          p = beta 	  if d > L*gamma 
 *
 *  2. Constraints (checked upon entry)
 *       0.0 <=  alpha	<= 1.0	[alpha is a probability: bad_specs+1]
 *       0.0 <= beta		[beta is nonnegative: bad_specs+3]
 *       0.0 <= gamma		[gamma is nonnegative: bad_specs+2]
 *       n <  scale*scale 	[enough room for nodes: bad_specs+4]
 *
 *  Note carefully the following:
 *   - Output of this routine depends on sequence of values returned by
 *     gb_next_rand().  If the seed parameter is nonzero, it will be
 *     used to seed the random number generator before createing the graph.
 *     if it is zero, whatever random values are next are used.
 *     Thus, successive calls to this function with the same parameters
 *     do not, in general, return the same graph if seed=0.
 *
 *   - The returned graph is NOT guaranteed to be connected.  Checking
 *     connectivity is the responsibility of the caller.
 *
 */

Graph * 
geo(long seed, geo_parms *pp)
{
double p,d,L,radius,r;
u_char *occ;
register int scale;
unsigned nbytes, index, x, y;
u_long units,pertrange;
int mallocd;
register i,j;
Graph *G;
Vertex *up,*vp;
char namestr[128];

    gb_trouble_code = 0;

    if (seed)			/* zero seed means "already init'd */
	gb_init_rand(seed);

    scale = pp->scale;

    L = sqrt(2.0) * scale;

    gb_trouble_code = 0;
    if ((pp->alpha <= 0.0) || (pp->alpha > 1.0)) gb_trouble_code=bad_specs+1;
    if (pp->gamma < 0.0) 		gb_trouble_code=bad_specs+GEO_TRBL;
    if (pp->beta < 0.0)			gb_trouble_code=bad_specs+GEO_TRBL;
    if (pp->n > (scale * scale))	gb_trouble_code=bad_specs+GEO_TRBL;
    if (gb_trouble_code)
	return NULL;

    radius = pp->gamma * L;	/* comes in as a fraction of diagonal */

#define posn(x,y)	((x)*scale)+(y)
#define bitset(v,i)	(v[(i)/NBBY]&(1<<((i)%NBBY)))
#define setbit(v,i)	v[(i)/NBBY] |= (1<<((i)%NBBY))

/* create a new graph structure */
   
    if ((G=gb_new_graph(pp->n)) == NULL)
	return NULL;

    nbytes = ((scale*scale)%NBBY ? (scale*scale/NBBY)+1 : (scale*scale)/NBBY);

    if (nbytes < STACKMAX) {	/* small amount - just do it in the stack */
        occ = (u_char *) alloca(nbytes);
        mallocd = 0;
    } else {
        occ = (u_char *) malloc(nbytes);
        mallocd = 1;
    }

    for (i=0; i<nbytes; i++) occ[i] = 0;


/* place each of n points in the plane */

    for (i=0,vp = G->vertices; i<pp->n; i++,vp++) {
	sprintf(namestr,"%d",i);	/* name is just node number */
	vp->name = gb_save_string(namestr);
        do {
            vp->xpos = gb_next_rand()%scale;         /* position: 0..scale-1 */
            vp->ypos = gb_next_rand()%scale;         /* position: 0..scale-1 */
	    index = posn(vp->xpos,vp->ypos);
        } while (bitset(occ,index));
        setbit(occ,index);
    }

/* Now go back and add the edges */
	
    for (i=0,up = G->vertices; i<(pp->n)-1; i++,up++)
	for (j=i+1, vp = &G->vertices[i+1]; j<pp->n; j++,vp++) { 
	    d = distance(up,vp);
            switch (pp->method) {
	    case RG2:		/* Waxman's */
		d = randfrac()*L;
		/* fall through */
            case RG1:		/* Waxman's */
		p = pp->alpha * exp(-d/(L*pp->beta));
		break;
	    case RANDOM:	/* the classic */
		p = pp->alpha;
		break;
	    case EXP:	
		p = pp->alpha * exp(-d/(L-d));
		break;
            case DL:		/* Doar-Leslie */
		p = pp->alpha * (pp->gamma/pp->n) * exp(-d/(L*pp->beta));
		break;
	    case LOC:		/* Locality model, two probabilities */
		if (d <= radius) p = pp->alpha;
		else p = pp->beta;	
		break;
	    default:
		die("Bad edge method in geo()!");
	    }
	    if (randfrac() < p) 
		gb_new_edge(up,vp,(int)rint(d));
		
	}

/* Fill in the "id" string to say how the graph was created */

    G->Gscale = scale;
    sprintf(G->id,"geo(%ld,", seed);
    i = strlen(G->id);
    i += printparms(G->id+i,pp);
    G->id[i] = ')';
    G->id[i+1] = (char) 0;
    strcpy(G->util_types,GEO_UTIL);

/* clean up */
    if (mallocd) free(occ);

    return G;
}


#define NOLEAF	0
#define LEAFOK	1

/* Return pointer to node with least degree (least degree > 1, if lflag==0)
 * Such a node is guaranteed to exist, provided the graph is connected and
 * has > 2 nodes.
 */

Vertex *
find_small_deg(Graph *g,int lflag)
{
int i,deg,least=INT_MAX;
Arc *ep;
Vertex *vp,*foo;

   foo = NULL;
   for (i=0,vp=g->vertices; i<g->n; i++,vp++) {
	deg = 0;
	for (ep=vp->arcs;ep;ep=ep->next) deg++;
	if (deg < least) 
	    if (lflag == LEAFOK || deg > 1) {
		least = deg;
		foo = vp;
	    }
   }
   return foo;
}

/* find_thresh_deg returns the lowest-numbered node with degree less than
 * threshold, IF such exists.  If all node degrees exceed threshold,
 * returns lowest-numbered node, i.e. g->vertices.
 */
Vertex *
find_thresh_deg(Graph *g,int thresh)
{
int i,deg;
Arc *ep;
Vertex *vp;

   for (i=0,vp=g->vertices; i<g->n; i++,vp++) {
	deg = 0;
        for (ep=vp->arcs;ep;ep=ep->next) deg++;
	if (deg < thresh)
	    return vp;
   }
   return g->vertices;
}


/* Determines what "level" in the graph the edge between u and v is,
 * based on the longest common substring of their "name" strings.  
 */

int
edge_level(Vertex *u, Vertex *v, int nlev)
{
char ss[MAXNAMELEN], tt[MAXNAMELEN];
register char *s=ss, *t=tt, *b, *c;

nlev -= 1;

strcpy(ss,u->name);
strcpy(tt,v->name);

b = strchr(s,'.');
c = strchr(t,'.');
if (b && c) {
  *b++ = '\0';
  *c++ = '\0';
}
while (!strcmp(s,t)) { /* still equal => must still be a '.' */
  s = b;
  t = c;
  b = strchr(s,'.');
  c = strchr(t,'.');
  if (b && c) {
    *b++ = '\0';
    *c++ = '\0';
  }
  nlev -= 1;
}

return nlev;

/* NOTREACHED */
}

Graph *
geo_hier(long seed,
	int nlevels,	/* number of levels (=size of following array) */
	int edgemeth,	/* method of attaching edges */
	int aux,	/* auxiliary parameter for edge method (threshold) */
	geo_parms *pp)	/* array of parameter structures, one per level */
{
Graph *newG, *tG, *GG, *srcG, *dstG;
long *numv;		/* array of sizes of lower-level graphs */
geo_parms *curparms, workparms[MAXLEVEL];
register i,k,indx;
long dst;
int temp,total,lowsize,otherend,blen,level;
long maxP[MAXLEVEL], maxDiam[MAXLEVEL], wt[MAXLEVEL];
Vertex *np,*vp,*up,*base;
Arc *ap;
char vnamestr[MAXNAMELEN];


     if (seed)		/* convention: zero seed means don't use */
	gb_init_rand(seed);

     if (nlevels < 1 || nlevels > MAXLEVEL) {
	gb_trouble_code = bad_specs+HIER_TRBL;
	return NULL;
     }

     /* 1 <= nlevels <= MAXLEVEL */

     /* copy the parameters so we can modify them, and caller doesn't
      * see the changes.
      */
     for (level=0; level<nlevels; level++)
	bcopy((char *)&pp[level],&workparms[level],sizeof(geo_parms));

     level = 0;

     gb_trouble_code = 0;
     
     tG = NULL;
     do {
	gb_recycle(tG);
        tG = geo(0L,workparms);
     } while (tG != NULL && !isconnected(tG));

     if (tG==NULL)
	return tG;

     maxDiam[0] = fdiam(tG);
     maxP[0] = maxDiam[0];
     wt[0] = 1;

     for (i=1; i<nlevels; i++)
       maxDiam[i] = -1;

     curparms = workparms;

     while (++level < nlevels) {
       long tdiam;

	curparms++;	/* parameters for graphs @ next level */

        /* spread out the numbers of nodes per graph at this level */
	numv = (long *) calloc(tG->n,sizeof(long));
	lowsize = curparms->n;
        randomize(numv,tG->n,curparms->n,3*tG->n);

	/* create a subordinate graph for each vertex in the "top" graph,
         * and add it into the new graph as a whole.
	 * We construct the subgraphs all at once to ensure that each
	 * has a unique address.
	 */
	for (i=0,vp=tG->vertices; i<tG->n; i++,vp++) {
	    curparms->n = numv[i];
	    do {
		newG = geo(0L,curparms);
		if (newG==NULL) return NULL;
	    } while (!isconnected(newG));
	    vp->sub = newG;
	    tdiam = fdiam(newG);
	    if (tdiam>maxDiam[level])
	      maxDiam[level] = tdiam;
	}

	/* do some calculations before "flattening" the top Graph */

	total = 0;

	for (i=0; i<tG->n; i++) {	/* translate node numbers */
	    temp = numv[i];		
	    numv[i]= total;
	    total += temp;
	}

	if (total != tG->n*lowsize) {
	    fprintf(stderr,"bad size of new graph!\n");
	    fprintf(stderr,"total %d tG->n %d lowsize %d\n",total,tG->n,lowsize);
	    gb_trouble_code = impossible+HIER_TRBL;
	    return NULL;
	}

	/* now create what will become the "new" top-level graph */
	newG = gb_new_graph(total);
	if (newG==NULL) {
	    gb_trouble_code += HIER_TRBL;
	    return NULL;
	}

	/* resolution of the new graph */
	newG->Gscale = tG->Gscale * curparms->scale;

       /* compute edge weights for this level */

       wt[level] = maxP[level-1] + 1;
       maxP[level] = (maxDiam[level]*wt[level])
	 + (maxDiam[level-1]*maxP[level-1]);

       for (i=0,vp=tG->vertices; i<tG->n; i++,vp++) {
	 strcpy(vnamestr,vp->name);	/* base name for all "offspring" */
	 blen = strlen(vnamestr);
	 vnamestr[blen] = '.';
	    
	 GG = tG->vertices[i].sub;
	 base = newG->vertices + numv[i];	/* start of this node's */
	 for (k=0,np=base,up=GG->vertices; k<GG->n; k++,np++,up++) {

	   /* add the node's edges */
	   for (ap=up->arcs; ap; ap=ap->next)  {
	     otherend = ap->tip - GG->vertices;
	     if (k < otherend)
	       gb_new_edge(np,base+otherend,ap->len);
	     
	   }

	   /* now set the new node's position */
	   np->xpos = tG->vertices[i].xpos * curparms->scale + up->xpos;
	   np->ypos = tG->vertices[i].ypos * curparms->scale + up->ypos;

	   /* give the "new" node a name by catenating top & bot names */
	   strcpy(vnamestr+blen+1,up->name);
	   np->name = gb_save_string(vnamestr);

	 } /* loop over GG's vertices */
       }  /* loop over top-level vertices */

       /*
	* Now we have to transfer the top-level edges to new graph.
	* This is done by one of three methods:
	*    0: choose a random node in each subgraph
	*    1: attach to the smallest-degree non-leaf node in each
	*    2: attach to smallest-degree node
	*    3: attach to first node with degree less than aux
	*/
       for (i=0; i<tG->n; i++) {
	 Vertex *srcp, *dstp;
	 Graph *srcG, *dstG;

	 srcG = tG->vertices[i].sub;

	 if (srcG == NULL) {	/* paranoia */
	   gb_trouble_code = impossible+HIER_TRBL+1;
	   return NULL;
	 }

	 for (ap=tG->vertices[i].arcs; ap; ap=ap->next) {

	   dst = ap->tip - tG->vertices;

	   if (i > dst)	/* consider each edge only ONCE */
	     continue;

	   dstG = ap->tip->sub;

	   if (dstG == NULL) {	/* paranoia */
	     gb_trouble_code = impossible+HIER_TRBL+1;
	     return NULL;
	   }

	   /* choose endpoints of the top-level edge */

	   switch (edgemeth) {
	   case 0:	/* choose random node in each */
	     srcp = srcG->vertices + gb_next_rand()%srcG->n;
	     dstp = dstG->vertices + gb_next_rand()%dstG->n;
	     break;

	   case 1:	/* find nonleaf node of least degree in each */
	     /* This causes problems with graph size < 3 */
	     if (srcG->n > 2)
	       srcp = find_small_deg(srcG,NOLEAF);
	     else
	       srcp = find_small_deg(srcG,LEAFOK);
	     if (dstG->n > 2)
	       dstp = find_small_deg(dstG,NOLEAF);
	     else
	       dstp = find_small_deg(dstG,LEAFOK);
	     break;

	   case 2: /* find node of smallest degree */
	     srcp = find_small_deg(srcG,LEAFOK);
	     dstp = find_small_deg(dstG,LEAFOK);
	     break;

	   case 3: /* first node w/degree < aux */
	     srcp = find_thresh_deg(srcG,aux);
	     dstp = find_thresh_deg(dstG,aux);
	   default:
	     gb_trouble_code = bad_specs+HIER_TRBL;
	     return NULL;

	   }	/* switch on edgemeth */

	   /* pointer arithmetic: isn't it fun?
	      printf("Copying edge from %d to %d\n",
	      numv[i]+(srcp - srcG->vertices),
	      numv[dst] + (dstp - dstG->vertices));
	      */
	   if (srcp==NULL || dstp==NULL) {
	     gb_trouble_code = impossible + HIER_TRBL+2;
	     return NULL;
	   }
		
	   srcp = newG->vertices + numv[i] + (srcp - srcG->vertices);
	   dstp = newG->vertices + numv[dst] + (dstp - dstG->vertices);

	   gb_new_edge(srcp,dstp,idist(srcp,dstp));

	 } /* for each arc */
       } /* for each vertex of top graph */

        /* now make the "new" graph the "top" graph and recycle others */
       for (i=0,vp=tG->vertices; i<tG->n; i++,vp++)
	 gb_recycle(vp->sub);

       gb_recycle(tG);

       tG = newG;

       free(numv);
     }	/* while more levels */

/* Finally, go back and add the policy weights,
 * based upon the computed max diameters
 * and Max Path lengths.
 */
   for (i=0; i<tG->n; i++)
     for (ap=tG->vertices[i].arcs; ap; ap=ap->next) {
       dst = ap->tip - tG->vertices;
       if (i > dst)	/* consider each edge only ONCE */
	 continue;

       assert(i != dst); /* no self loops */

       /* i < dst: it is safe to refer to ap's mate by ap+1.  */
       level = edge_level(&tG->vertices[i],&tG->vertices[dst],nlevels);
       ap->policywt = (ap+1)->policywt = wt[level];

     }

/* construct the utility and id strings for the new graph.
 * Space constraints will restrict us to keeping about 4 levels'
 * worth of info.
 */
  {
    char buf[ID_FIELD_SIZE+1];
    register char *cp;
    int len, nextlen, left;

    strcpy(tG->util_types,GEO_UTIL);	/* same for all geo graphs,	*/
					/* defined in geo.h		*/
    cp = tG->id;
    sprintf(cp,"geo_hier(%d,%d,%d,%d,[",seed,nlevels,edgemeth,aux);
    len = strlen(cp);
    left = ID_FIELD_SIZE - len;
    cp += len;
    
    for (i=0; (i < nlevels) && (left > 0); i++) {
      nextlen = printparms(buf,&pp[i]);
      strncpy(cp,buf,left);
      left -= nextlen;
      cp += nextlen;
    }
    if (left > 0) {
      sprintf(buf,"])");
      nextlen = strlen(buf);
      strncpy(cp,buf,left);
    }
  }

  return tG;
}	/* geo_hier() */
