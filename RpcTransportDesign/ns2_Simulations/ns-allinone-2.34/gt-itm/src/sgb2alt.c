/* $Id: sgb2alt.c,v 1.1 1996/10/04 13:33:38 ewz Exp $
 *
 *  sgb2alt.c -- converter from sgb format to alternative format. 
 *
 *  Alternative format:
 *
 *   GRAPH (#nodes #edges id uu vv ww xx yy zz):
 *   <#nodes> <#edges> <id> [all integer utility fields]
 *
 *   VERTICES (index name u v w x y z):
 *   0 <name-node0> [all integer utility fields for node0]
 *   1 <name-node1> [all integer utility fields for node1]
 *   ....
 *   
 *   EDGES (from-node to-node length a b):
 *   <from-node> <to-node> <length> [all integer utility fields]
 *   ....
 *
 */

#include <stdio.h>
#include <strings.h>
#include "gb_graph.h"
#include "gb_save.h"
#include "geog.h"

main(argc,argv)
    int argc;
    char *argv[];
{
    int i, j;
    Vertex *v;
    Arc *a;
    Graph *g;
    FILE *fopen(), *fout;

    if (argc != 3) {
	    printf("sgb2old <sgfile> <altfile>\n\n");
	    return;
    }
    fout = fopen(argv[2],"w");

    g = restore_graph(argv[1]);
		if (g == NULL) {
      printf("%s does not contain a correct SGB graph\n",argv[1]);
			return;
		}
			
    fprintf(fout,"GRAPH (#nodes #edges id uu vv ww xx yy zz):\n");
    fprintf(fout,"%d %d %s ",g->n, g->m, g->id);
    if (g->util_types[8] == 'I') fprintf(fout,"%ld ",g->uu.I);
    if (g->util_types[9] == 'I') fprintf(fout,"%ld ",g->vv.I);
    if (g->util_types[10] == 'I') fprintf(fout,"%ld ",g->ww.I);
    if (g->util_types[11] == 'I') fprintf(fout,"%ld ",g->xx.I);
    if (g->util_types[12] == 'I') fprintf(fout,"%ld ",g->yy.I);
    if (g->util_types[13] == 'I') fprintf(fout,"%ld ",g->zz.I);
    fprintf(fout,"\n\n");

    fprintf(fout,"VERTICES (index name u v w x y z):\n");
    for (v = g->vertices,i=0; i < g->n; i++,v++) {
        fprintf(fout,"%d %s ",i, v->name); 
        if (g->util_types[0] == 'I') fprintf(fout,"%ld ",v->u.I);
        if (g->util_types[1] == 'I') fprintf(fout,"%ld ",v->v.I);
        if (g->util_types[2] == 'I') fprintf(fout,"%ld ",v->w.I);
        if (g->util_types[3] == 'I') fprintf(fout,"%ld ",v->x.I);
        if (g->util_types[4] == 'I') fprintf(fout,"%ld ",v->y.I);
        if (g->util_types[5] == 'I') fprintf(fout,"%ld ",v->z.I);
        fprintf(fout,"\n");
    }
    fprintf(fout,"\n");

    fprintf(fout,"EDGES (from-node to-node length a b):\n");
    for (v = g->vertices,i=0; i < g->n; i++,v++) 
	for (a = v->arcs; a != NULL; a = a->next) {
		j = a->tip - g->vertices;
		if (j > i) {
		    fprintf(fout,"%d %d %d ",i,j,a->len);
		    if (g->util_types[6] == 'I') fprintf(fout,"%ld ",a->a.I);
		    if (g->util_types[7] == 'I') fprintf(fout,"%ld ",a->b.I);
	            fprintf(fout,"\n");
		}
   	}

}	
