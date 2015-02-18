/* $Id: edriver.c,v 1.1 1996/10/04 13:33:19 ewz Exp $
 *
 * edriver.c -- driver program to call eval routines. 
 *
 * Input: <filestem> [-nd] [-<f0><f1>]*
 *
 *      The file "./<filestem>.gb" is expected to contain a graph
 *      description created by "gb_save()".
 *
 *	If the -nd option is given, the distribution files will
 *	not be created.  They take up lots of space for larger graphs.
 *
 *      The options -<f0><f1> specify the fields to use
 *      in computing the shortest path metrics.  For each -<f0><f1>
 *      pair, shortest paths will be computed using <f0> as the distance
 *      metric and <f1> as the measurement metric, where <f0> and <f1>
 *      are one of {l, a, b, h}, indicating the arc field to use
 *      (l = len, a = a, b = b) or to use unit weight (h).  The scalar
 *      properties of diameter and average depth will be stored in the
 *      scalar file; if the -nd option is not present, the depth 
 *      distribution will be stored in the file <filestem>-<f0><f1>d.
 *	
 * Output: 	
 *
 *      ./<filestem>-ev - scalar characteristics of graph:
 *             		average vertex degree 
 *                      diam-<f0><f1>
 *                      avgdepth-<f0><f1>
 *			number of biconnected components
 *	./<filestem>-dd - vertex degree distribution
 *      ./<filestem>-<f0><f1>d - depth distribution for <f0><f1> metrics
 *
 *	Default is to evaluate all characteristics. 
 *
 */

#include <stdio.h>
#include "gb_graph.h"
#include "gb_save.h"
#include "gb_dijk.h"
#include "eval.h"

#define MAXF	132
#define OPEN_OUT(f,s)   \
		{ strcpy(outfile+plen,s);\
		  if ((f = fopen(outfile,"a"))==NULL) {\
			fprintf(stderr,"Unable to open output file %s\n",s);\
			exit(2);\
    		   } \
		 }\

main(argc,argv)
    int argc;
    char *argv[];
{
    int i; 
    int min, max, sum, bins, *ddist; 
    float avg;
    Graph *g; 
    char infile[MAXF];
    char outfile[MAXF];
    char *dstr = "-nd";
    int plen, idx;
    FILE *ddf, *evf, *pdf, *fp, *fopen();
    int prdist = 1;
    enum Field f0, f1;
    int first = 1;

    if (argc == 1) {
	printf("Usage: edriver <filestem> [-nd] [-<f0><f1>]*\n\n");
	return;
    }
    /* determine whether to print distributions */
    /* determine where in argv the field pairs begin */
    if (argc >= 3 && strcmp(argv[2],dstr) == 0) {
        prdist = 0;
	      idx = 3;
    }
    else idx = 2;

    sprintf(infile,"%s.gb",argv[1]);

    if ((g = restore_graph(infile))==NULL) {
	fprintf(stderr,"Unable to restore graph from %s.\n",infile);
	exit(1);
    }

    /* open output file -ev in append mode */
    /* check whether this is a new run of stat or an additional run */
    /* if new then put header info in -ev */
    /*             open -dd */
    /*             run node degree distribution routines */
    /* run path routines for each field spec */
    /* if new then run bicomps */

    /* test if this is the first time evaluating this graph file */
    sprintf(outfile,"%s",argv[1]);
    plen = strlen(outfile);
    strcpy(outfile+plen,"-ev");   
    if ((fp = fopen(outfile, "r")) != NULL) {
	first = 0;
	fclose(fp);
    }

    OPEN_OUT(evf,"-ev");

    if (first) {
    if (prdist) OPEN_OUT(ddf,"-dd");

/* node degree distribution */
/* fprintf(ddf,"#node degrees\n"); */
    max = finddegdist(g,&ddist);
    sum = 0;
    for (i = 0; i <= max; i++) {
	sum += i*ddist[i];
	if (prdist) fprintf(ddf,"%3d : %d\n",i,ddist[i]);
    }
    if (prdist) fclose(ddf);

    putc('#',evf);
    g->id[ID_FIELD_SIZE-1] = 0;	/* just to be safe, sometimes they get
					truncated */
    fputs(g->id,evf);
    putc('\n',evf);

    fprintf(evf,"avgdeg %f\n",(float)sum/g->n);
    }

    /* go through each field specification, one at a time */
    for (idx; idx < argc; idx++) {
      switch (argv[idx][1]) {
	case 'l': f0 = Len; break;
        case 'a': f0 = A; break;
        case 'b': f0 = B; break;
        case 'h': f0 = Hops; break;
      }
      switch (argv[idx][2]) {
	case 'l': f1 = Len; break;
        case 'a': f1 = A; break;
        case 'b': f1 = B; break;
        case 'h': f1 = Hops; break;
      }
      dopaths(g,f0,f1,&min,&max,&avg); 
      fprintf(evf,"diam%s %d\n",argv[idx],max);
      fprintf(evf,"avgdepth%s %.3f\n",argv[idx],avg);     
      if (prdist) {
	OPEN_OUT(pdf,argv[idx]);
	dodepthdist(g,&ddist);
        bins = max - min + 1;
	for (i = 0; i<bins; i++)
	  fprintf(pdf,"%3d : %d\n",min+i,ddist[i]);
	fclose(pdf); 
      }
    }

	
/* N.B. do bicomponents AFTER everything else, it mess up some fields! */

    if (first) 
        fprintf(evf,"bicomp %d\n",bicomp(g,0));

    fclose(evf);

    exit(0);

}
