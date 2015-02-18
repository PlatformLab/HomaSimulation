/* $Id: ts.c,v 1.1 1996/10/04 13:37:05 calvert Exp $
 * 
 * ts.c -- routines to construct transit-stub graphs
 *         using the Stanford GraphBase tools.
 *
 * This code calls the routines in geog.c
 *
 */

#include <stdio.h>
#include <sys/types.h>		/* for NBBY */
#ifndef FBSD
#include <alloca.h>
#endif
#include "gb_graph.h"
#include <math.h>
#include "geog.h"
#include <limits.h>
#include <string.h>
#include <assert.h>

static char tsId[]="$Id: ts.c,v 1.1 1996/10/04 13:37:05 calvert Exp $";

/* convention for trouble codes: TS adds 7-9 */
#define TS_TRBL		7


#define vtoi(g,v)	((v) - (g)->vertices)

#define link	vv.G		/* Graph utility field */

/* Graph aux fields */

/* The following three are stored with the FINAL graph */
#define MaxTdiam	xx.I    /* Maximum Transit diameter (hops) */
#define MaxSdiam	yy.I    /* Maximum Stub diameter (hops) */
#define Topdiam		zz.I    /* Top-level Diameter (hops) */

/* The following two are used ONLY in Stub graphs */
#define Gxoff   xx.I            /* global x offset of stub domain graph */
#define Gyoff   yy.I            /* global y offset of stub domain graph */

#define TS_UTIL 	"ZZZIIZIZIZZIII"
                      /* VVVVVVAAGGGGGG */
                      /* uvwxyzabuvwxyz
			         uvwxyz */


#define sub     u.G             /* subordinate graph of a node */
#define flat	z.V		/* corresponding node in "flat" graph */

#define vindex(V,G)	((V) - (G)->vertices) 
#define	halfup(x)	(((x)>>1)+1)

#define OVERALL_DENSITY_FACTOR	80.0
#define STUB_DENSITY_FACTOR	16.0
#define TRANSIT_SPREAD		0.5

#define STACKMAX	0x800	/* max memory allocated in stack	*/

#define RANDMULT	3	/* multiplier for random iterations	*/
				/* determines max deviation from mean	*/

/*
#define BADRETURN(x)	{ free(nnodes); free(nstubs); free(nstubnodes);\
			  return (x); }
*/

/* fast diameter computation using Floyd-Warshall
 * Returns the HOP diameter of the graph, i.e. each edge given UNIT wt.
 * Leaves the LENGTH diameter of the graph in g->Gldiam.
 * It also works for DIRECTED graphs. */
long
fdiam(Graph *g)
{
register i,j,k;
long **dist, **ldist;
int changed,mallocd;
long diam, ldiam, newdist = 0, otherend;
Vertex *v;
Arc *a;

    i = g->n*g->n*sizeof(long*);
    if (i<STACKMAX) {
	dist = (long **)alloca(i);
	ldist = (long**)alloca(i);
	mallocd = 0;
    } else {
	dist = (long **)malloc(i);
        ldist = (long**)malloc(i);
	mallocd = 1;
    }

    for (i=0; i < g->n; i++) {
	if (mallocd) {
	    dist[i] = (long *)malloc(g->n*sizeof(long));
	    ldist[i] = (long *)malloc(g->n*sizeof(long));
	} else {
	    dist[i] = (long *)alloca(g->n*sizeof(long));
	    ldist[i] = (long *)alloca(g->n*sizeof(long));
	}
	for (j=0; j < g->n; j++)
	  dist[i][j] = ldist[i][j] =  BIGINT;

	dist[i][i] = ldist[i][i] = 0;
    }

    for (i=0,v=g->vertices; i < g->n; i++,v++)
	for (a=v->arcs; a; a=a->next) {
	    otherend = vtoi(g,a->tip);
	    ldist[i][otherend] = a->len;       /* edge weight */
	    dist[i][otherend] = 1;
	}

    /* iterate until nothing changes */
    changed = 1;
    while (changed) {
      /* INVARIANT: dist[i][k] == BIGINT  === ldist[i][k] == BIGINT */
	changed = 0;
	for (i=0; i<g->n; i++)
	    for (j=0; j<g->n; j++) {
		for (k=0; k<g->n; k++) { /* i < k < j */
		    if (i==j || j==k || i==k)
			continue;
		    /* i != j && j != k && i != k */
		    if (dist[i][k] == BIGINT ||
			dist[k][j] == BIGINT)
			continue;	/* nothing to gain */
		    newdist = dist[i][k] + dist[k][j];
		    if (newdist < dist[i][j]) {
			dist[i][j] = newdist;
			changed++;
		    } /* if shorter */
		    newdist = ldist[i][k] + ldist[k][j];
		    if (newdist < ldist[i][j]) {
			ldist[i][j] = newdist;
			changed++;
		    } /* if shorter */
	        } /* for k */

            } /* for j */

    } /* while (changed) */

    diam = ldiam = 0;
    /* now find max shortest path */
    for (i=0; i<g->n; i++)
	for (j=0; j<g->n; j++) {
	  if (dist[i][j] > diam)
	    diam = dist[i][j];
	  if (ldist[i][j] > diam)
	    ldiam = ldist[i][j];
        }

    if (mallocd) {
      for (i=0; i<g->n; i++) {
	free(dist[i]);
	free(ldist[i]);
      }
      free(dist);
      free(ldist);
    }

/* Store the Euclidean diameter; for now we don't do anything with it.
    g->Gldiam = ldiam;
*/ 


    return diam;

} /* fdiam */

void
die(s)
{
fprintf(stderr,"Fatal error %s\n",s);
exit(1);
}

/* Create a copy of each edge in fromG in toG.			*/
/* Also places a pointer in each node in fromG pointing to the  */
/* corresponding node in toG.					*/
/* Requires: fromG != NULL, toG != NULL, toG->n >= fromG->n	*/
void
copyedges(Graph *fromG, Graph *toG, long base)
{
register i, indx;
Vertex *np, *vp, *basep;
Arc *ap;

basep = toG->vertices + base;

for (i=0,vp=fromG->vertices,np=basep; i<fromG->n;
     i++,vp++,np++) {
  
  vp->flat = np;	/* keep a pointer from old v to new */

  for (ap=vp->arcs; ap; ap=ap->next) {
    indx = vindex(ap->tip,fromG);
    if (i<indx) /* do each edge only once */ {
      gb_new_edge(np,basep+indx,ap->len); /* euclidean doesn't change! */
      np->arcs->policywt = 1;           /* unit weight for policy  */
      (np->arcs+1)->policywt = 1;       /* both directions!  */
    }
  }
}

} /* copyedges() */
		

/* Generate Transit-Stub graphs.					*/
/* Returns NULL without cleaning up when any subroutine call [e.g. geo(),  */
/* gb_new_graph()] fails.						   */
Graph *
transtub(long seed,
	 int spx,		/* mean # stubs/transit node	*/
	 int nrants,		/* # random transit-stub edges	*/
	 int nranss,		/* # random stub-stub edges	*/
	 geo_parms* toppp,	/* params for transit connectivity */
	 geo_parms* transpp,	/*   "     "  transit domains	*/
	 geo_parms* stubpp)	/*   "     "  stub domains	*/
{

int topdiam,	/* diameter of top-level graph */
    transdiam=0,  /* max diameter of a transit domain graph*/
    stubdiam=0;	/* max diameter of a stub domain graph */
long ts,	/* length of transit-stub edges */
     ss,	/* length of stub-stub edges */
     tt;	/* length of transit-transit edges */

Graph *topG=NULL, *newG=NULL, *stubG=NULL, *finalG, *dgp, *ddgp, *sgp, *tempg;
/* XXX this should be ifdef'd to be "int" on 32-bit machines and "short"  */
/* on 64-bit machines.  Who needs a graph with more than 2 billion nodes? */

long *nnodes=NULL, *nstubs=NULL, *nstubnodes=NULL;
long maxtnodes, maxstubs, maxstubnodes, maxtotal;

/* for positioning in plane. */
long globscale, xoffset, yoffset, maxstuboff, maxx, minx, maxy, miny, xrange,
yrange;
long i,j,k;
long indx, diam, totalnodes, base, dom;
char dnodename[ID_FIELD_SIZE], snodename[ID_FIELD_SIZE];
int dnamelen, numtries=0;
register Vertex *v,*dnp, *snp,	/* domain node and stub node pointers	*/
       *ddnp, *fp, *tp, *tmp;

Arc *a;
long dnn,snn,		/* domain node number, stub node number	*/
     snum,		/* stub number */
     attachnode;	/* index of edge endpoint */

/* XXXXXX We need to copy ALL parameters because we modify them, and they
 * need to be saved with the output graph. */

geo_parms workparms;

   if (seed)
	gb_init_rand(seed);

/* allocate dynamic arrays.  We can calc upper bounds on sizes		*/
/* because increase above mean due to randomize() is at most number	*/
/* of iterations.  Note: These look weird, but they are correct.	*/

   maxtnodes = transpp->n + (RANDMULT *toppp->n);
   maxstubs = spx + (RANDMULT * maxtnodes);
   maxstubnodes = stubpp->n + (RANDMULT*maxstubs);

   /*   We can already calculate the total number of nodes  */
   totalnodes = toppp->n * transpp->n * (spx * stubpp->n + 1);

   nstubs = (long *)malloc(maxtnodes*sizeof(long)); /* stubs per T node */
   nstubnodes = (long *)malloc(maxstubs*sizeof(long)); /* stub nodes per stub*/
   nnodes = (long *)malloc(toppp->n*sizeof(long));  /* nodes per T dom */

/* Compute the scales.  The global scale determines the actual unit sizes.
 * The others determine how big (i.e. how much of the whole square) a transit
 * domain and a stub domain are.  That is, the fraction of the square
 * covered by a transit domain is TRANSIT_SPREAD^2. XXX should be a parameter
 * Fraction of a square covered by a stub domain = (stubpp->scale/globscale)^2.
 */

   globscale = (long) rint(sqrt(totalnodes*OVERALL_DENSITY_FACTOR));
   toppp->scale = globscale;
   transpp->scale = (long) rint(globscale*TRANSIT_SPREAD);
   if (transpp->scale&1)
     transpp->scale += 1;
   stubpp->scale = (long) rint(sqrt(maxstubnodes*STUB_DENSITY_FACTOR));

   assert(stubpp->scale < globscale);

   do {
	gb_recycle(topG);
	topG = geo(0,toppp);
	if (topG==NULL) {
	    free(nnodes);
	    free(nstubs);
	    free(nstubnodes);
	    return NULL;       /* don't mess with geo's trouble code */
	}
   } while (!isconnected(topG) && ++numtries < 100);

   if (numtries >= 100) 
	die("Failed to get a connected graph after 100 tries\n");
         /* XXX return NULL + set code */

   topdiam = fdiam(topG);

   /* randomize the number of nodes per transit domain */

   randomize(nnodes,topG->n,transpp->n,RANDMULT*topG->n);

   workparms = *transpp;

   for (i=0,v=topG->vertices;  i<topG->n; i++,v++) {
     
     xoffset = v->xpos - (transpp->scale/2);
     yoffset = v->ypos - (transpp->scale/2);
     if (xoffset < 0)
       xoffset = 0;
     if (yoffset < 0)
       yoffset = 0;
     if ((xoffset + transpp->scale) > globscale)
       xoffset = globscale - transpp->scale;
     if ((yoffset + transpp->scale) > globscale)
       yoffset = globscale - transpp->scale;


     /* v == &topG->vertices[i], represents one whole transit domain	*/
     /* create a connected domain graph */

     workparms.n = nnodes[i];
     newG = geo(0,&workparms);   /* create graph with nnodes[i] nodes */
     if (newG==NULL) return NULL;

     numtries = 0;
     while (!isconnected(newG) && ++numtries<=100) {
       gb_recycle(newG);
       newG = geo(0,&workparms);   /* create graph with nnodes[i] nodes */
       if (newG == NULL) return NULL;
     }
     if (numtries > 100)
       die("Failed to get connected domain graph.");
     if (gb_trouble_code)
       return NULL;
     
     diam = fdiam(newG);

     if (diam > transdiam) transdiam = diam;
     
     v->sub = newG;

     /* randomize the number of stubs per transit node for this t domain */

     randomize(nstubs,newG->n,spx,RANDMULT*newG->n);
        
     /* now for each node k in this T domain, create some S		*/
     /* domains to attach to it.  These (nstubs[k] of 'em) domains 	*/
     /* average stubpp->n nodes per domain.				*/

     workparms = *stubpp;

     for (k=0,dnp=newG->vertices; k<newG->n; k++,dnp++) {

       /* dnp == &newG->vertices[k] */
       /* k is the index of the current transit node */

       dnp->sub = NULL;

       dnp->xpos += xoffset;     /* position this node in the global plane */
       dnp->ypos += yoffset;

       /* Compute the range of possible offsets for stubs from this node. */
       /* The furthest point of a stub may be up to stubscale from node */
       /* (in each direction).  That is, the origin of the node is in the */
       /* range [max(0,dnp->pos-stubpp->scale),dnp->pos] */

       minx = dnp->xpos - stubpp->scale;
       if (minx < 0) minx = 0;
       miny = dnp->ypos - stubpp->scale;
       if (miny < 0) miny = 0;

       maxx = dnp->xpos;
       if (maxx + stubpp->scale > globscale)
	 maxx = globscale - stubpp->scale;
       maxy = dnp->ypos;
       if (maxy + stubpp->scale > globscale)
	 maxy = globscale - stubpp->scale;

       xrange = maxx - minx;
       yrange = maxy - miny;

       randomize(nstubnodes,nstubs[k],stubpp->n,RANDMULT*nstubs[k]);

       for (j=0; j< nstubs[k]; j++) {	/* create this many stubs */ 
	 workparms.n = nstubnodes[j];	/* with this many nodes */

	 stubG = geo(0,&workparms);
	 if (stubG == NULL) return NULL;

	 while (!isconnected(stubG)) {
	   gb_recycle(stubG);
	   stubG = geo(0,&workparms);
	   if (stubG == NULL) return NULL;
	 }

	 /* compute the offset of this stub from dnp's position */
	 /* These won't be used until the graph is "flattened" in the */
	 /* final stage. */
	 if (xrange)
	   stubG->Gxoff = minx + (random() % xrange);
	 else
	   stubG->Gxoff = minx;
	 if (yrange)
	   stubG->Gyoff = miny + (random() % yrange);
	 else
	   stubG->Gyoff = miny;

	 /* keep track of max of all stub diameters */
	 diam = fdiam(stubG);
	 if (diam > stubdiam) stubdiam = diam;

	 /* add the graph to the node's list */
	 stubG->link = dnp->sub;
	 dnp->sub = stubG;

       } /* create stubs attached to node u */

     } /* for each vertex of this transit domain */

   }  /* for each top-level domain */

   /*
    * Edge length calculations. Main constraints are:
    *  (0) 2ts > topdiam
    *  (1) 2ss > stubdiam
    *  (2) 2tt > transdiam
    *  (3) 2ts > topdiam*tt + (tipdiam+1)*transdiam  [implies (0)]
    *  (4) 2ss > 2stubdiam+2ts+(topdiam)tt+(topdiam+1)transdiam
    *      =
    *       ss > stubdiam + ts + (topdiam)tt/2 + (topdiam+1)transdiam/2
    *      => 
    *       ss > stubdiam
    *      => {2ss > ss, transitivity >}
    *       2ss > stubdiam [i.e. (1)]
    *  (5) ss ~ 2ts + e [desirable but may or may not always be true]
    *  where 
    *  tt = length of edges connecting transit domains
    *  ts = length of edges connecting transits to stubs
    *  ss = length of (random) edges connecting stubs to stubs.
    *
    *  Noting that 2*((X>>1)+1) > X for nonnegative X, we define
    *
    *	halfup(x) to be (((x)>>1)+1)
    */

    tt = halfup(transdiam);
    ts = halfup(topdiam*tt) + halfup((topdiam+1)*transdiam);
    ss = stubdiam  + 2*ts;

   /*
    *  Note that all intra-domain edges can keep original length!
    */

   /* Now we can flatten the hierarchy of graphs into the final Graph */

   finalG = gb_new_graph(totalnodes);
   base = 0;
   for (dom=0,v=topG->vertices; dom<topG->n; dom++,v++) { /* for each domain */
	/* v = &topG->vertices[dom] */

	dgp = v->sub;	/* dgp points to the "current" domain graph */

	copyedges(dgp,finalG,base);	/* copy in edges of the domain */
	/* NB lengths of edges do not change      */
	/* Policy weights are set to unity by copyedges() */

	base += dgp->n;			/* account for the domain's nodes */

	/* now go through the nodes of this domain */
	for (dnn=0,dnp=dgp->vertices; dnn<dgp->n; dnn++,dnp++) {

	    /* name of this (transit) node */

	    sprintf(dnodename,"T:%d.%d",dom,dnn);
	    dnp->flat->name = gb_save_string(dnodename);
	    
	    /* position of this node */
	    dnp->flat->xpos = dnp->xpos;
	    dnp->flat->ypos = dnp->ypos;

	    /* get ready for stubs */

	    strcpy(snodename,dnodename);
	    snodename[0] = 'S';	/* change transit to stub */
	    dnamelen = strlen(dnodename);

	    /* now deal with stubs */
	    snum = 0;
	    sgp = dnp->sub;
	    while (sgp) {
	      Vertex *anp;

		copyedges(sgp,finalG,base);

		for (snn=0,snp=sgp->vertices; snn<sgp->n; snn++,snp++) {

		    /* name each node */
		    sprintf(snodename+dnamelen,"/%d.%d",snum,snn);
		    snp->flat->name = gb_save_string(snodename);

                    /* and position it correctly */
		    snp->flat->xpos = snp->xpos + sgp->Gxoff;
		    snp->flat->ypos = snp->ypos + sgp->Gyoff;
		    assert(snp->flat->xpos < globscale);
		    assert(snp->flat->ypos < globscale);


	        } /* for each node of this stub */

		attachnode = gb_next_rand()%sgp->n;
                anp = finalG->vertices+(base+attachnode);
	        assert((dnp->flat-finalG->vertices) < (anp-finalG->vertices));
		gb_new_edge(dnp->flat,anp, idist(dnp->flat,anp));
                dnp->flat->arcs->policywt = ts;
	        (dnp->flat->arcs+1)->policywt = ts;

		base += sgp->n;	/* account for this stub's nodes */

		sgp = sgp->link;

		snum++;

	    }	/* for each stub graph of this node */

	}  /* for each node of this domain */

   }	/* for each domain */

   if (base != totalnodes) {
	fprintf(stderr,"incorrect node total: base=%d, totalnodes=%d\n",
		base,totalnodes);
	exit(3);
   }

   /* Now we have to place the top-level (interdomain) edges	*/
   /* Can do this now because each "old" node has a pointer to	*/
   /* corresponding node in the "flat" graph, also each node has*/
   /* a hierarchical name. */

   for (dom=0,v=topG->vertices; dom<topG->n; dom++,v++) { /* for each domain */
	dgp = v->sub;
	for (a=v->arcs;a;a=a->next) {	 /* add edges to other domains */
	    /* choose an endpoint in each graph */
	    dnp = dgp->vertices + (gb_next_rand()%dgp->n);
	    ddgp = a->tip->sub;
	    ddnp = ddgp->vertices + (gb_next_rand()%ddgp->n);
	    fp = dnp->flat;
	    tp = ddnp->flat;
	    if (tp < fp) {
	      tmp = fp;
	      fp = tp;
	      tp = tmp;
	    }
	    gb_new_edge(fp,tp,idist(fp,tp));
	    /* fp < tp */
	    fp->arcs->policywt = tt;
	    (fp->arcs+1)->policywt = tt;
        }
   }


   /* Next add some RANDOM transit-stub edges.			*/
   for (i=0; i<nrants; i++) {
	dnp = topG->vertices + (gb_next_rand()%topG->n); /*choose dom*/
	dgp = dnp->sub;
	dnp = dgp->vertices[gb_next_rand()%dgp->n].flat; /* and a node */
	assert(dnp->name[0]=='T');	/* sanity check */

	do {	/* choose S node NOT already connected to this T dom node */
	   snp = finalG->vertices + (gb_next_rand()%finalG->n);
	} while (!td_OK(snp,dnp));
	if (snp < dnp) {     /* we require dnp < snp */
	  tmp = dnp;
	  dnp = snp;
	  snp = tmp;
	}
	gb_new_edge(dnp,snp,idist(dnp,snp));
	dnp->arcs->policywt = ts;
	(dnp->arcs+1)->policywt = ts;
   }


   /* Next, add some RANDOM stub-stub edges.			*/
   /* Don't add edges between nodes in the same stub domain;	*/
   /* anything else is OK. */

   for (i=0; i<nranss; i++) {
	do {
	    /* pick two nodes at random that aren't in the same stub */
	    dnp = finalG->vertices + (gb_next_rand()%finalG->n);
	    ddnp = finalG->vertices + (gb_next_rand()%finalG->n);
	} while (!stubs_OK(dnp,ddnp));
	if (ddnp < dnp) {  /* required is dnp < ddnp */
	  tmp = dnp;
	  dnp = ddnp;
	  ddnp = dnp;
	}
	gb_new_edge(dnp,ddnp,idist(dnp,ddnp));
	/* fp < tp */
	dnp->arcs->policywt = ss;
	(dnp->arcs+1)->policywt = ss;
   }


   /* Finally, put the parameter info in the "id" string of the new graph */
   sprintf(finalG->id,"transtub(%ld,%d,%d,%d,",
	seed,spx,nrants,nranss);
   i = strlen(finalG->id);
   i += printparms(dnodename,toppp);
   if (i<ID_FIELD_SIZE) {
	strcat(finalG->id,dnodename);
	finalG->id[i++] = ',';
	finalG->id[i] = (char) 0;
        i += printparms(dnodename,transpp);
        if (i<ID_FIELD_SIZE) {
	    strcat(finalG->id,dnodename);
	    finalG->id[i++] = ',';
	    finalG->id[i] = (char) 0;
	    i += printparms(dnodename,stubpp);
            if (i<ID_FIELD_SIZE) {
		strcat(finalG->id,dnodename);
		finalG->id[i++] = ')';
                if (i<ID_FIELD_SIZE)
		    finalG->id[i] = (char) 0;
	    } else {
		strncat(finalG->id,dnodename,
			(ID_FIELD_SIZE-strlen(finalG->id)));
	    }
        } else
	    finalG->id[strlen(finalG->id)] = (char) 0;
   } else
	finalG->id[strlen(finalG->id)] = (char) 0;

   strcpy(finalG->util_types,TS_UTIL);
   finalG->Gscale = globscale;		/* uu */
   finalG->MaxTdiam = transdiam;	/* xx */
   finalG->MaxSdiam = stubdiam;		/* yy */
   finalG->Topdiam = topdiam;		/* zz */

   /* CLEAN UP: free up all the memory we grabbed. */

   for (v=topG->vertices; v<topG->vertices+topG->n; v++) {
	dgp = v->sub;
	for (dnp = dgp->vertices; dnp<dgp->vertices+dgp->n; dnp++) {
	    sgp = NULL;	
	    tempg = dnp->sub;
	    while (tempg) {
		sgp = tempg;
		tempg = tempg->link;
		gb_recycle(sgp);
	    }
	}
	gb_recycle(dgp);
   }
   gb_recycle(topG);

   free((char *)nstubnodes);
   free((char *)nstubs);
   free((char *)nnodes);	/* free the last little bit */

   /* return the new graph! */
   return finalG;

}  /* ts() */

/* return true if it's OK to put an edge between stub node snp and	*/
/* transit node dnp.   The criteria are:				*/
/*    - snp is a stub node						*/
/*    - dnp is a transit node 						*/
/*    - the stub domain is not administratively "under" this transit    */
/*      node.								*/
int
td_OK(Vertex *snp,Vertex *dnp)
{
int dlen, slen;

    if (*snp->name != 'S') return 0;
    dlen = strlen(dnp->name+2);
    slen = strcspn(snp->name+2,"/");

    /* if the domain prefix is the same, it's NOT OK */
    return ((dlen != slen) || strncmp(dnp->name+2,snp->name+2,dlen));
}

/* OK to put an extra edge between two stub nodes, as long as they are	*/
/* not already in the same stub domain.  This is checked by the initial	*/
/* prefix of their names, i.e. the part before the final '.'		*/
/* if these two strings differ, it's OK.				*/
int
stubs_OK(Vertex *snp0,Vertex *snp1)
{
int slen0, slen1;
char *cp;

    if (*snp0->name != 'S' ||
        *snp1->name != 'S')
	return 0;

    /* find the last occurrence of "." in each string */
    /* I don't trust strrchr(), gdb doesn't seem to either */
    slen0 = strlen(snp0->name);
    cp = snp0->name + slen0;	/* initially points to null */
    while (*cp != '.' && --cp > snp0->name) slen0--;

    slen1 = strlen(snp0->name);
    cp = snp1->name + slen1;	/* initially points to null */
    while (*cp != '.' && --cp > snp1->name) slen1--;


    /* strncmp returns nonzero if strings differ, which means "OK" */
    return strncmp(snp0->name,snp1->name,(slen0>slen1?slen1:slen0));
}
