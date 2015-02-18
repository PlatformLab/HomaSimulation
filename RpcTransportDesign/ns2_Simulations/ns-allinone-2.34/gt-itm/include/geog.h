/* $Id: geog.h,v 1.1 1996/10/04 13:35:33 calvert Exp $
 * 
 * geog.h -- constants and declarations for graph-generation routines.
 *
 */

#include "gb_flip.h"
#include "limits.h"
/* Vertex auxiliary fields	*/
#define xpos	x.I		/* vertex locations (in scale units) */
#define ypos	y.I

/* Arc auxiliary fields */
#define policywt  a.I

/* Graph aux fields */

#define Gscale	uu.I		/* resolution of vertex placement */

/* Utility string for Graph type
 * We use the following:
 * Vertices:  x, y = Euclidean coordinates of each vertex
 * Arcs:      a = policy weight
 * Graphs:    uu = global scale of coordinate system (size of unit square)
 */
#define GEO_UTIL	"ZZZIIZIZIZZZZZ"
                      /* VVVVVVAAGGGGGG */
                      /* uvwxyzabuvwxyz
			         uvwxyz */

/* Methods for calculating edge probabilities, use as arguments to geo() */
/* XXX Make this an enum type */
#define RG2	1
#define RG1	2
#define RANDOM	3
#define EXP	4
#define DL	5
#define LOC	6

/* Maximum number of levels for geo_hier() */
#define MAXLEVEL	5

/* Absolute maximum length of a node name string XXX */
#define MAXNAMELEN       128

/* Default seed for random number generation */
#define DEFAULT_SEED	417890326
#define BIGINT		LONG_MAX
#define randfrac()	(gb_next_rand() / (double) LONG_MAX)

typedef struct geo_parm_struct {
    long	n;	/* number of nodes	*/
    int		scale,	/* placement resolution */
    		method;	/* how to calculate edge probs (RG2 etc. above) */
    double	alpha,	/* probability parameter*/
		beta,	/* ditto		*/
		gamma; /* ditto */
} geo_parms;

Graph *geo(long seed,geo_parms *p);

Graph *
geo_hier(long seed,	/* for seeding the random number generator */
	int nlevels,	/* size of the parameter structure array */
        int edgemeth,	/* method of attaching interlevel edges */
        int aux,	/* auxiliary parameter for edge method? */
	geo_parms *pp);	/* array of parameter structures, one per level */

Graph *
transtub(long seed,
         int spx,               /* mean # stubs/transit node    */
         int nrants,            /* # random transit-stub edges  */
         int nranss,            /* # random stub-stub edges     */
         geo_parms* toppp,      /* params for transit connectivity */
         geo_parms* transpp,    /*   "     "  transit domains   */
         geo_parms* stubpp);     /*   "     "  stub domains      */
