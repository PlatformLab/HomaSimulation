/* $Id: eval.h,v 1.1 1996/06/24 20:16:27 ewz Exp $
 *
 * eval.h -- header file for evaluation routines
 *
 */
#define mdist   u.I

enum Field {Len, A, B, Hops};

void twofield_sptree(Graph*, Vertex*, enum Field, enum Field);

