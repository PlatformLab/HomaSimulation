/*-*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 by the University of Southern California
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation in source and binary forms for non-commercial purposes
 * and without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both the copyright notice and
 * this permission notice appear in supporting documentation. and that
 * any documentation, advertising materials, and other materials related
 * to such distribution and use acknowledge that the software was
 * developed by the University of Southern California, Information
 * Sciences Institute.  The name of the University may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
 * the suitability of this software for any purpose.  THIS SOFTWARE IS
 * PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Other copyrights might apply to parts of this software and are so
 * noted when applicable.
 *
 *  sgb2comns.c -- a common converter from sgb format to ns format which may be used
 *  for creating hierarchical as well as flat topology. 
 *
 *  usage :  sgb2hierns -sgb <sgbgraph.file> -out <outfile> {optional-arguments -hier 1
 *  -topo <topo.file -used for scenario generation>}
 *
 *  Based on sgb2ns & sgb2hierns converters
 *  Maintainer: Padma Haldar (haldar@isi.edu)
 */

#include <stdio.h>
#include <strings.h>
#include "gb_graph.h"
#include "gb_save.h"
#include "geog.h"

#define TRUE   1
#define FALSE  0
#define TINY   16
#define SMALL  64
#define MED    256
#define LARGE  4096
#define HUGE   655536


void print_error();
void parse_input(int argc, char** argv, int* flag, FILE** output);
void sgb2hierns(Graph *g, FILE *output, FILE *topofile);
void sgb2flatns(Graph *g, FILE *output) ;
void create_hierarchy(Graph *g, int *domain, char *transits, int *n, \
		       int *l, int *q, int *stubnode, int *stubsize, \
		      char **address, char *node_per_cluster);
void print_topofile(FILE *output, int domain, char *transits, int n,\
		    int l, int *stubnode, int *stubsize);
void print_hier(FILE *fout, char **address, Graph *g, \
		int domain, char *node_per_cluster, int *q);
void print_flat(FILE *fout, Graph *g);
void print_hdr(FILE *fout, Graph *g);

main(argc,argv)
    int argc;
    char *argv[];
{
	
	int hier_flag=0;
	FILE *output=NULL;
	FILE *fout=NULL;
	Graph *g;
	
	
	if (argc < 3) {
		print_error();
	}
	fout = fopen(argv[2],"w");
	if (argc > 3) {
		parse_input(argc, argv, &hier_flag, &output);
	}
	
	g = restore_graph(argv[1]);
	if (g == NULL) {
		printf("%s does not contain a correct SGB graph\n",argv[1]);
		return;
	}
	
	if (hier_flag) 
		sgb2hierns (g, fout, output);
	else 
		sgb2flatns (g, fout);
}



void print_error()
{
	 /* for the purpose of scenario generator, \
	    need to return a list of transits & stubs \
	    and num of nodes in each - hence the optional \
	    third arg topofile */
	printf("sgb2comns <sgb-graph> <outfile.tcl>\noptional args: \
-hier(turn on hierarchy flag) -topo <topofile>(generate topology file for scenario generator)\n\n");
	printf("Example : \"sgb2comns t100-0.gb hts100.tcl -hier -topo hts100.topofile\"\n\n");
	exit(1);
	
}


	      
void parse_input(int argc, char** argv, int* flag, FILE** output) 
{
	int i;
	
	for (i = 3; i < argc; i++) {
		if (strcmp(argv[i], "-hier") == 0)
			*flag = 1;
		else if (strcmp(argv[i], "-topo") == 0) {
			i++;
			*output = fopen(argv[i], "w");
		}
		else
			print_error();
	}
	
}

void sgb2hierns(Graph *g, FILE *output, FILE *topofile) 
{
	int     domain = 0,
		n = 0,
		l = 0;
	char    transits[LARGE],
		node_per_cluster[HUGE];
	int     q[HUGE],
		stubsize[LARGE],
		stubnode[LARGE];
	char    *address[HUGE];
	
	// transits[0] = '\0';
// 	node_per_cluster[0] = '\0';
	
	create_hierarchy(g, &domain, transits, &n, &l, q, stubnode, \
			 stubsize, address, node_per_cluster);
	
	if (topofile != NULL) 
		print_topofile(topofile, domain, transits, n, l, \
			       stubnode, stubsize);
	print_hier(output, address, g, domain, node_per_cluster, q);
}


void sgb2flatns(Graph *g, FILE *output) 
{
	print_flat(output, g);
}


void create_hierarchy(Graph *g, int *domain, char *transits, int *num, \
		      int *lp, int *q, int *stubnode, int *stubsize, \
		      char **address, char *node_per_cluster) 
{

	Vertex  *v;
	Arc     *a;
	
	int     i=0,
		n=0,
		p=0,
		l=0,
		q_b=0,
		r=0;
	char    *temp,
		*index;
	char    name[SMALL],
		m[420],
		tempstr[TINY];
	
/* generating hierarchical topology from transit-stub*/
	q[p] = 0;
	for (v = g->vertices,i; i < g->n; i++,v++) {
		strcpy(name, v->name);
		temp = name;
		temp += 2;
		index = strstr(temp, ".");
		temp[index - temp] = '\0';
		printf("temp = %s\n",temp);
		
		if ( p == atoi(temp)) { /* in same domain as before */
			
			if (name[0] == 'T') { /* for transits -> single node domains */
				r = 0;
				address[i] = (char *)malloc(strlen(v->name));
				sprintf(address[i],"%d.%d.%d",p,q[p],r);
				printf("address[%d] = %d.%d.%d\n",i,p,q[p],r);
				strcat(node_per_cluster, "1 ");
				
				sprintf(tempstr, "%d ", i); 
				strcat(transits, tempstr); 
				q[p] = q[p] + 1;
			}
			else if (name[0] == 'S') { /* for stubs */
				strcpy(name, v->name);
				temp = name;
				while(temp){
					index = strstr(temp, ".");
					if (index != NULL)
						temp = index + 1;
					else{
						if (atoi(temp) == 0) {
							/* counting stub nodes for scenario gen */
							stubnode[l] = i;
							l++;
							
							if ( q_b != 0) {
								sprintf(tempstr, "%d ", r + 1);
								strcat(node_per_cluster, tempstr);
								
								stubsize[n] = r + 1;
								n++;
							} 
							r = 0;
							address[i] = (char *)malloc(strlen(v->name));
							sprintf(address[i],"%d.%d.%d", p, q[p], r);
							printf("address[%d] = %d.%d.%d\n",i,p,q[p],r);
							q_b = q[p];
							q[p] = q[p] + 1;
						}
						else {
							r++;
							address[i] = (char *)malloc(strlen(v->name));
							sprintf(address[i],"%d.%d.%d", p, q_b, r);
							printf("address[%d] = %d.%d.%d\n",i,p,q_b,r);
						}
						break;
					}
				}
			}
		}
		else {
			sprintf(tempstr, "%d ", r + 1);
			printf("again...node_per_cluster = %s\n",node_per_cluster);
			printf("tempstr = %s\n",tempstr);
			strcat(node_per_cluster, tempstr);
			printf("here..stubsize = %d\n",stubsize[n]);
			stubsize[n] = r + 1;
			n++; 
			
			p = atoi(temp);
			q[p] = 0;
			q_b = 0;
			r = 0;
			address[i] = (char *)malloc(strlen(v->name));
			sprintf(address[i],"%d.%d.%d",p,q[p],r);
			printf("address[%d] = %d.%d.%d\n",i,p,q[p],r);
			
			strcat(node_per_cluster, "1 ");
			printf("again...node_per_cluster = %s\n",node_per_cluster);
			sprintf(tempstr, "%d ", i); 
			strcat(transits, tempstr); 
			
			q[p] = q[p] + 1;
		}
		
	}
	
	(*domain) = p + 1;
	/* assuming all domains have equal # of clusters */
	/* for domains having diff no of clusters , use q[domain num]*/
	
	sprintf(tempstr, "%d ", r + 1);
	strcat(node_per_cluster, tempstr);
	stubsize[n] = r + 1;
	*num = n;
	*lp = l;
	
	
}



void print_topofile(FILE *output, int domain, char *transits, int n,\
		    int l, int *stubnode, int *stubsize) 
{
	int i;
	
	
	fprintf(output, "domains %d\n", domain);
	fprintf(output, "transits %s\n", transits);
	fprintf(output, "total-stubs %d\n", n+1);
	fprintf(output, "stubs ");
	for(i = 0; i < l; i++) 
		fprintf(output, "%d ", stubnode[i]);
	fprintf(output, "\nnodes/stub ");
	for(i = 0; i <= n; i++) 
		fprintf(output, "%d ", stubsize[i]);
	fprintf(output, "\n\nEnd of topology file. \n");
	fclose(output);
}


void print_hier(FILE *fout, char **address, Graph *g, \
		int domain, char *node_per_cluster, int *q )
{
	int     i,
		j,
		nlink;
	Vertex  *v;
	Arc     *a;
	
	

	fprintf(fout,"# Generated by sgb2comns\n");
	print_hdr(fout, g);

	fprintf(fout, "#Creating hierarchical topology from transit-stub graph:\n\n");
	fprintf(fout, "proc create-hier-topology {nsns node linkBW} {\n");
	fprintf(fout, "\tupvar $node n\n"); 
	fprintf(fout, "\tupvar $nsns ns\n\n"); 
/*      fprintf(fout, "\tglobal ns n\n\n"); */
	fprintf(fout,"\tset verbose 1\n\n");

	/* nodes */
	fprintf(fout, "\tif {$verbose} { \n");
	fprintf(fout, "\t\tputs \"Creating hierarchical nodes..\" \n\n");
	fprintf(fout, "\t}\n");
	fprintf(fout, "\tset i 0 \n");
	fprintf(fout, "\tforeach a { \n\n");
	for (i = 0; i < g->n; i++) {
		fprintf(fout, "\t\t%s\n",address[i]);
	}
	fprintf(fout,"\t} { \n");
	fprintf(fout,"\t\tset n($i) [$ns node $a]\n");
	fprintf(fout,"\t\tincr i \n");
	fprintf(fout,"\t\tif {[expr $i %% 100] == 0} {\n");
	fprintf(fout,"\t\t\tputs \"creating node $i...\" \n \t\t} \n\t}");
	fprintf(fout,"\n\n");
	fprintf(fout,"# Topology information :\n");
	fprintf(fout, "\tlappend domain %d\n", domain);
	fprintf(fout, "\tAddrParams set domain_num_ $domain\n");
	fprintf(fout, "\tlappend cluster ");
	for (i = 0; i < domain; i++) {
		fprintf(fout, "%d ",q[i]);
	}
	fprintf(fout, "\n");
	fprintf(fout, "\tAddrParams set cluster_num_ $cluster\n");
	fprintf(fout, "\tlappend eilastlevel %s\n",node_per_cluster);
	fprintf(fout, "\tAddrParams set nodes_num_ $eilastlevel\n");
	fprintf(fout,"\n\n");

	/* edges */
	fprintf(fout, "\t# EDGES (from-node to-node length a b):\n");
	nlink = 0;
	fprintf(fout, "\tif {$verbose} { \n");
	fprintf(fout, "\t\tputs \"Creating links 0...\"\n");
	fprintf(fout, "\t\tflush stdout \n");
	fprintf(fout, "\t}\n");
	fprintf(fout, "\tset i 0 \n");
	fprintf(fout, "\tforeach t { \n");
    
	for (v = g->vertices,i=0; i < g->n; i++,v++) 
		for (a = v->arcs; a != NULL; a = a->next) {
			j = a->tip - g->vertices;
			if (j > i) {
				fprintf(fout, "\t\t{%d %d %dms} \n",i, j, 10 * a->len);
				
				nlink++;
			}
		}
	fprintf(fout, "\t} { \n");
	fprintf(fout, "\t$ns duplex-link-of-interfaces $n([lindex $t 0]) $n([lindex $t 1]) $linkBW [lindex $t 2] DropTail \n");
	fprintf(fout, "\tincr i \n");
	fprintf(fout, "\tif {[expr $i %% 100] == 0} { \n");
	fprintf(fout, "\t\tputs \"creating link $i...\" \n");
	fprintf(fout, "\t} \n } \n");
	
	/* srm members. join-group will be performed by Agent/SRM::start */
	/* return the number of nodes in this topology */
	fprintf(fout, "\treturn %d\n", g->n);
	fprintf(fout, "}\n");
	
	for (i=0; i < g->n; i++)
		if (address[i] != NULL)
			free(address[i]);
	fprintf(fout,"# end of hier topology generation\n");
	fclose(fout);
	
}

void print_hdr(FILE *fout, Graph *g) 
{
	fprintf(fout,"# GRAPH (#nodes #edges id uu vv ww xx yy zz):\n");
	fprintf(fout,"# %d %d %s ",g->n, g->m, g->id);
	if (g->util_types[8] == 'I') fprintf(fout,"%ld ",g->uu.I);
	if (g->util_types[9] == 'I') fprintf(fout,"%ld ",g->vv.I);
	if (g->util_types[10] == 'I') fprintf(fout,"%ld ",g->ww.I);
	if (g->util_types[11] == 'I') fprintf(fout,"%ld ",g->xx.I);
	if (g->util_types[12] == 'I') fprintf(fout,"%ld ",g->yy.I);
	if (g->util_types[13] == 'I') fprintf(fout,"%ld ",g->zz.I);
	fprintf(fout,"\n\n");
}


void print_flat(FILE *fout, Graph *g) 
{
	Vertex  *v;
	Arc     *a;
	int     i,
		j;
	
	
	print_hdr(fout, g);

	fprintf(fout, "proc create-topology {nsns node linkBW} {\n");
	fprintf(fout, "\tupvar $node n\n");
	fprintf(fout, "\tupvar $nsns ns\n\n");
	fprintf(fout,"\tset verbose 1\n\n");
	/* nodes */
	fprintf(fout, "\tif {$verbose} { \n");
	fprintf(fout, "\t\tputs \"Creating nodes...\" \n");
	fprintf(fout, "}\n");
	fprintf(fout, "\t\tfor {set i 0} {$i < %d} {incr i} {\n", g->n);
	fprintf(fout, "\t\t\tset n($i) [$ns node]\n");
	fprintf(fout, "\t}\n");
	fprintf(fout, "\n");

	/* edges */
	fprintf(fout, "\t# EDGES (from-node to-node length a b):\n");
	// nlink = 0;
	fprintf(fout, "\tif {$verbose} { \n");
	fprintf(fout, "\t\tputs -nonewline \"Creating links 0...\"\n");
	fprintf(fout, "\t\tflush stdout \n");
	fprintf(fout, "\t}\n");
	fprintf(fout, "\tset i 0 \n");
	fprintf(fout, "\tforeach t { \n");
	
	for (v = g->vertices,i=0; i < g->n; i++,v++) 
		for (a = v->arcs; a != NULL; a = a->next) {
			j = a->tip - g->vertices;
			if (j > i) {
				fprintf(fout,"\t\t{%d %d %dms} \n",i,\
					j, 10 * a->len); 
// 				nlink++;
			}
		}
	fprintf(fout, "\t} { \n");
	fprintf(fout, "\t$ns duplex-link-of-interfaces $n([lindex $t 0]) $n([lindex $t 1]) $linkBW [lindex $t 2] DropTail \n");
	fprintf(fout, "\tincr i \n");
	fprintf(fout, "\tif {[expr $i %% 100] == 0} { \n");
	fprintf(fout, "\t\tputs \"creating link $i...\" \n");
	fprintf(fout, "\t} \n } \n");
	
	/* srm members. join-group will be performed by Agent/SRM::start */
	/* return the number of nodes in this topology */
	fprintf(fout, "\treturn %d", g->n);
	fprintf(fout, "}\n");
}

