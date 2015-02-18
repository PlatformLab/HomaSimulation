/* $Id: itm.c,v 1.1 1996/10/04 13:37:19 calvert Exp $
 *
 * itm.c -- Driver to create graphs using geo(), geo_hier(), and transtub().
 *
 * Each argument is a file containing ONE set of specs for graph generation.
 * Such a file has the following format:
 *    <method keyword> <number of graphs> [<initial seed>]
 *    <method-dependent parameter lines>
 * Supported method names are "geo", "hier", and "ts".
 * Method-dependent parameter lines are described by the following:
 *    <geo_parms> ::=
 *        <n> <scale> <edgeprobmethod> <alpha> [<beta>] [<gamma>]
 *    <"geo" parms> ::= <geo_parms>
 *    <"hier" parms> ::=
 *          <number of levels> <edgeconnmethod> <threshold>
 *          <geo_parms>+  {one per number of levels}
 *    <"ts" parms> ::=
 *          <# stubs/trans node> <#t-s edges> <#s-s edges>
 *          <geo_parms>       {top-level parameters}
 *          <geo_parms>       {transit domain parameters}
 *          <geo_parms>       {stub domain parameters}
 *
 * Note that the stub domain "scale" parameter is ignored; a fraction
 * of the transit scale range is used.  This fraction is STUB_OFF_FACTOR,
 * defined in ts.c.
 * 
 * From the foregoing, it follows that best results will be obtained with
 *   -- a SMALL value for top-level scale parameter
 *   -- a LARGE value for transit-level scale parameter
 * and then the value of stub-level scale parameter won't matter.
 *
 * The indicated number of graphs is produced using the given parameters.
 * If the initial seed is present, it is used; otherwise, DFLTSEED is used.
 *
 * OUTPUT FILE NAMING CONVENTION:
 * The i'th graph created with the parameters from file "arg" is placed
 * in file "arg-i.gb", where the first value of i is zero.
 *
 */
#include <stdio.h>
#include <sys/types.h>		/* for NBBY, number of bits/byte */
#include <stdlib.h>		/* for calloc(),atoi(),etc. */
#include <string.h>		/* for strtok() */
#include "gb_graph.h"
#include "geog.h"

#define LINE 512
#define GEO_KW	"geo"
#define HIER_KW	"hier"
#define TS_KW	"ts"
#define GEO	100
#define HIER	101
#define TS	102

char *delim = " \t\n", *nonestr = "<none>";
static char  errstr[256];


char *
get_geoparms(FILE * f, geo_parms * pp)
{
char *cp;
char inbuf[LINE];

    do {
	if (feof(f))
	    return "file ended unexpectedly";
	fgets(inbuf, LINE - 1, f);
	cp = strtok(inbuf, delim);
    } while ((cp == NULL) || (*cp == '#'));

    if ((pp->n = atoi(cp)) <= 0)
	return "bad n parameter";

    cp = strtok(0, delim);
    if (cp == NULL || (pp->scale = atoi(cp)) <= 0)
	return "bad scale parameter";

    cp = strtok(0, delim);
    if (cp == NULL) return "bad edge method parameter";
    pp->method = atoi(cp);
    if (pp->method < 1 || pp->method > 6)
			return "bad edge method parameter";

    cp = strtok(0, delim);
    if (cp == NULL) return "bad alpha parameter";
    pp->alpha = atof(cp);
    if (pp->alpha < 0.0 || pp->alpha > 1.0)
			return "bad alpha parameter";

    cp = strtok(0, delim);
    if (cp != NULL) {
	       if ((pp->beta = atof(cp)) < 0.0)
	       return "bad beta parameter";
    } else pp->beta = 0.0;

    cp = strtok(0,delim);
    if (cp != NULL) {
			 if ((pp->gamma = atof(cp)) < 0.0)
	   return "bad gamma parameter";
    } else pp->gamma = 0.0;

    return NULL;
}				/* get_parms */

char *
get_hierparms(FILE *f,
                int *nLev,
		int *ecMeth,
		int *haux,
                geo_parms *parmsbuf)
{
char inbuf[LINE];
char *cp;
int i;

    do {
	if (feof(f))
	    return "file ended unexpectedly";
	fgets(inbuf, LINE - 1, f);
	cp = strtok(inbuf, delim);
    } while ((cp == NULL) || (*cp == '#'));

    *nLev = atoi(cp);
    if (*nLev < 0 || *nLev > MAXLEVEL)
	return "bad number of levels";


    cp = strtok(0, delim);
    if (cp == NULL || (*ecMeth = atoi(cp)) < 0)
	return "bad/missing edge connection method";

    cp = strtok(0, delim);
    if (cp != NULL)
	*haux = atoi(cp);

    for (i=0; i<*nLev; i++)
	if (cp = get_geoparms(f,&parmsbuf[i]))
	    return cp;

    return NULL;
}

char *
get_tsparms(FILE *f,
                int *SpT, int *TSe, int *SSe,
                geo_parms *parmsbuf)
{
char inbuf[LINE];
char *cp;
int i;

    if (SpT==NULL ||
	SSe==NULL ||
	TSe==NULL)
	return "bad input parameter (null ptr)";

    do {
	if (feof(f))
	    return "file ended unexpectedly";
	fgets(inbuf, LINE - 1, f);
	cp = strtok(inbuf, delim);
    } while ((cp == NULL) || (*cp == '#'));

    if ((*SpT = atoi(cp)) < 0)
	return "bad stubs/trans parameter";

    cp = strtok(0, delim);

    if (cp == NULL)
	return "missing stub-stub edge parameter";

    if ((*TSe = atoi(cp)) < 0)
	return "bad transit-stub edge parameter";

    cp = strtok(0, delim);

    if (cp == NULL)
	return "missing transit-stub edge parameter";

    if ((*SSe = atoi(cp))  < 0)
	return "bad stub-stub edge parameter";

    for (i=0; i<3; i++)
	if (cp = get_geoparms(f, &parmsbuf[i]))
	    return cp;

    return NULL;
}


char *
makegraph(char *iname)
{
FILE *infile;
Graph *G = NULL;
char inbuf[LINE],outfilename[LINE];
register char *cp;
char *method;
int count, lineno=0, numlevels, nerrors;
int i, m=0, prefixlen;
int edgeConnMeth, haux, stubsPerTrans, tsEdges, ssEdges;
long seed;
geo_parms	parmsbuf[MAXLEVEL];	/* make sure MAXLEVEL >= 3 */

    if ((infile = fopen(iname, "r")) == NULL) {
        sprintf(errstr, "can't open input file %s", iname);
        die(errstr);
    }

    /* set up output file name */

    sprintf(outfilename,"%s-",iname);
    prefixlen = strlen(outfilename);
    
    do {
        fgets(inbuf, LINE - 1, infile); lineno++;
        method = strtok(inbuf, delim);
    } while (((method == NULL) || (*method == '#'))
            && !feof(infile));
		/* skip over comments  and blank lines */

    if ((cp = strtok(NULL, delim))==NULL)
	return "missing <number of graphs>";

    count = atoi(cp);

    if ((cp = strtok(NULL, delim))==NULL)
	seed = 0;
    else
        seed = atol(cp);

    if (strcmp(method,GEO_KW)==0) {

	if (cp = get_geoparms(infile,parmsbuf))
	    return cp;
	m = GEO;

    } else if (strcmp(method,HIER_KW)==0) {

	if (cp = get_hierparms(infile,
		&numlevels, &edgeConnMeth, &haux, parmsbuf))
	    return cp;
	m = HIER;

    } else if (strcmp(method,TS_KW)==0) {

	if (cp = get_tsparms(infile,
		&stubsPerTrans, &tsEdges, &ssEdges, parmsbuf))
	    return cp;
        m = TS;
    } else {
	sprintf(errstr,"Unknown generation method %s",method);
	return errstr;
    }


    if (seed)
	gb_init_rand(seed);
    else
	gb_init_rand(DEFAULT_SEED);

    for (i=0; i<count; i++) {
	sprintf(outfilename+prefixlen,"%d.gb",i);
	switch(m) {
	case GEO:
            do {
		gb_recycle(G);
		G = geo(0,parmsbuf);
	    } while (G != NULL && !isconnected(G));
		break;
	case HIER:
	    G = geo_hier(0,numlevels,edgeConnMeth,haux,parmsbuf);
	    break;
	case TS:
	    G = transtub(0,stubsPerTrans,tsEdges,ssEdges,&parmsbuf[0],
			&parmsbuf[1],&parmsbuf[2]);
	    break;
	default:
	    return "This can't happen!";
	
	}	/* switch */

	if (G==NULL) {
	    sprintf(errstr,"Error creating graph %d, trouble code=%d",i,
               gb_trouble_code);
	    return errstr;
	}

	nerrors = save_graph(G, outfilename);	
	if (nerrors > 0)
	    fprintf(stderr, "%s had %d anomalies\n", outfilename,nerrors);
	gb_recycle(G);
    }	/* for */

    return NULL;
}

main(int argc, char **argv)
{
    FILE           *infile;
    char	   *rv;
    char           *badinpstr = "badly formed input file %s, continuing\n";

    if (argc == 1) {
	printf("itm <spec-file0> <spec-file1> ....\n\n");
	return;
    }
    while (--argc) {

        if (rv = makegraph(*(++argv))) {
	    fprintf(stderr,"Error processing file %s: %s\n",*argv,rv);
	}

    }	/* while on argc */

    exit(0);
}
