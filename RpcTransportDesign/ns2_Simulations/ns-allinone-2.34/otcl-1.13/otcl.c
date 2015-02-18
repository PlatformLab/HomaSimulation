/* -*- Mode: c++ -*-
 *
 *  $Id: otcl.c,v 1.23 2005/09/07 04:44:18 tom_henderson Exp $
 *  
 *  Copyright 1993 Massachusetts Institute of Technology
 * 
 *  Permission to use, copy, modify, distribute, and sell this software and its
 *  documentation for any purpose is hereby granted without fee, provided that
 *  the above copyright notice appear in all copies and that both that
 *  copyright notice and this permission notice appear in supporting
 *  documentation, and that the name of M.I.T. not be used in advertising or
 *  publicity pertaining to distribution of the software without specific,
 *  written prior permission.  M.I.T. makes no representations about the
 *  suitability of this software for any purpose.  It is provided "as is"
 *  without express or implied warranty.
 * 
 */

#include <stdlib.h>
#include <string.h>
#include <tclInt.h>
#include <otcl.h> 


/*
 * compatibility definitions to bridge 7.x -> 7.5
 */

#if TCL_MAJOR_VERSION < 7
  #error Tcl distribution is TOO OLD
#elif TCL_MAJOR_VERSION == 7 && TCL_MINOR_VERSION <= 3
  typedef char* Tcl_Command;
  static char* Tcl_GetCommandName(Tcl_Interp* in, Tcl_Command id) {
    return id;
  }

  static int Tcl_UpVar(Tcl_Interp* in, char* lvl, char* l, char* g, int flg){
    char* args[4];
    args[0] = "uplevel"; args[1] = "1";
    args[2]=l; args[3]=g;
    return Tcl_UpvarCmd(0, in, 4, args);
  }

  #define Tcl_CreateCommand(A,B,C,D,E)     \
    strcpy((char*)ckalloc(strlen(B)+1), B);\
    Tcl_CreateCommand(A,B,C,D,E)
#endif

#if TCL_MAJOR_VERSION <= 7
#define TclIsVarUndefined(varPtr) \
    ((varPtr)->flags == VAR_UNDEFINED)
#endif

#if TCL_MAJOR_VERSION < 8
#define ObjVarTablePtr(OBJ) (&(OBJ)->variables.varTable)
#define compat_Tcl_AddObjErrorInfo(a,b,c) Tcl_AddErrorInfo(a,b)
#else
#define ObjVarTablePtr(OBJ) ((OBJ)->variables.varTablePtr)
#define compat_Tcl_AddObjErrorInfo(a,b,c) Tcl_AddObjErrorInfo(a,b,c)
#endif



/*
 * object and class internals
 */


typedef struct OTclObject {
  Tcl_Command id;
  Tcl_Interp* teardown;
  struct OTclClass* cl;
  struct OTclClass* type;
  Tcl_HashTable* procs;
  CallFrame variables;
} OTclObject;


typedef struct OTclClass {
  struct OTclObject object;
  struct OTclClasses* super;
  struct OTclClasses* sub;
  int color;
  struct OTclClasses* order;
  struct OTclClass* parent;
  Tcl_HashTable instprocs;
  Tcl_HashTable instances;
  Tcl_HashTable* objectdata;
} OTclClass;


typedef struct OTclClasses {
  struct OTclClass* cl;
  struct OTclClasses* next;
} OTclClasses;


/*
 * definitions of the main otcl objects
 */
static Tcl_HashTable* theObjects = 0;
static Tcl_HashTable* theClasses = 0;
static Tcl_CmdProc* ProcInterpId = 0;

/*
 * error return functions
 */


static int
OTclErrMsg(Tcl_Interp *in, char* msg, Tcl_FreeProc* type) {
  Tcl_SetResult(in, msg, type);
  return TCL_ERROR;
}

static int
OTclErrArgCnt(Tcl_Interp *in, CONST84 char *cmdname, char *arglist) {
  Tcl_ResetResult(in);
  Tcl_AppendResult(in, "wrong # args: should be {", cmdname, 0);
  if (arglist != 0) Tcl_AppendResult(in, " ", arglist, 0);
  Tcl_AppendResult(in, "}", 0);
  return TCL_ERROR;
}


static int
OTclErrBadVal(Tcl_Interp *in, char *expected, CONST84 char *value) {
  Tcl_ResetResult(in);
  Tcl_AppendResult(in, "expected ", expected, " but got", 0);
  Tcl_AppendElement(in, value);
  return TCL_ERROR;
}


static int
OTclErrType(Tcl_Interp *in, CONST84 char* nm, char* wt) {
  Tcl_ResetResult(in);
  Tcl_AppendResult(in,"type check failed: ",nm," is not of type ",wt,0);
  return TCL_ERROR;
}


/*
 * precedence ordering functions
 */


enum colors { WHITE, GRAY, BLACK };

static int
TopoSort(OTclClass* cl, OTclClass* base, OTclClasses* (*next)(OTclClass*)) {
  OTclClasses* sl = (*next)(cl);
  OTclClasses* pl;

  /*
   * careful to reset the color of unreported classes to
   * white in case we unwind with error, and on final exit
   * reset color of reported classes to white
   */

  cl->color = GRAY;
  for (; sl != 0; sl = sl->next) {
    OTclClass* sc = sl->cl;
    if (sc->color==GRAY) { cl->color = WHITE; return 0; }
    if (sc->color==WHITE && !TopoSort(sc, base, next)) {
      cl->color=WHITE;
      if (cl == base) {
	OTclClasses* pc = cl->order;
	while (pc != 0) { pc->cl->color = WHITE; pc = pc->next; }
      }
      return 0;
    }
  }
  cl->color = BLACK;
  pl = (OTclClasses*)ckalloc(sizeof(OTclClasses));
  pl->cl = cl;
  pl->next = base->order;
  base->order = pl;
  if (cl == base) {
    OTclClasses* pc = cl->order;
    while (pc != 0) { pc->cl->color = WHITE; pc = pc->next; }
  }
  return 1;
}


static void
RC(OTclClasses* sl) {
  while (sl != 0) {
    OTclClasses* n = sl->next;
    ckfree((char*)sl); sl = n;
  }
} 


static OTclClasses* Super(OTclClass* cl) { return cl->super; }

static OTclClasses*
ComputePrecedence(OTclClass* cl) {
  if (!cl->order) {
    int ok = TopoSort(cl, cl, Super);
    if (!ok) { RC(cl->order); cl->order = 0; }
  }
  return cl->order;
}


static OTclClasses* Sub(OTclClass* cl) { return cl->sub; }

static OTclClasses*
ComputeDependents(OTclClass* cl) {
  if (!cl->order) {
    int ok = TopoSort(cl, cl, Sub);
    if (!ok) { RC(cl->order); cl->order = 0; }
  }
  return cl->order;
}


static void
FlushPrecedences(OTclClass* cl) {
  OTclClasses* pc;
  RC(cl->order); cl->order = 0;
  pc = ComputeDependents(cl);

  /*
   * ordering doesn't matter here - we're just using toposort
   * to find all lower classes so we can flush their caches
   */

  if (pc) pc = pc->next;
  while (pc != 0) {
    RC(pc->cl->order); pc->cl->order = 0;
    pc = pc->next;
  }  
  RC(cl->order); cl->order = 0;
}


static void
AddInstance(OTclObject* obj, OTclClass* cl) {
  obj->cl = cl;
  if (cl != 0) {
    int nw;
    (void) Tcl_CreateHashEntry(&cl->instances, (char*)obj, &nw);
  }
}


static int
RemoveInstance(OTclObject* obj, OTclClass* cl) {
  if (cl != 0) {
    Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&cl->instances, (char*)obj);
    if (hPtr) { Tcl_DeleteHashEntry(hPtr); return 1; }
  }
  return 0;
}


/*
 * superclass/subclass list maintenance
 */


static void
AS(OTclClass* cl, OTclClass* s, OTclClasses** sl) {
  OTclClasses* l = *sl;
  while (l &&  l->cl != s) l = l->next;
  if (!l) {
    OTclClasses* sc = (OTclClasses*)ckalloc(sizeof(OTclClasses));
    sc->cl = s; sc->next = *sl; *sl = sc;
  }
}

static void
AddSuper(OTclClass* cl, OTclClass* super) {
  if (cl && super) {
    
    /*
     * keep corresponding sub in step with super
     */ 

    AS(cl, super, &cl->super);
    AS(super, cl, &super->sub);
  }
}


static int
RS(OTclClass* cl, OTclClass* s, OTclClasses** sl) {
  OTclClasses* l = *sl;
  if (!l) return 0;
  if (l->cl == s) {
    *sl = l->next;
    ckfree((char*)l);
    return 1;
  }
  while (l->next && l->next->cl != s) l = l->next;
  if (l->next) {
    OTclClasses* n = l->next->next;
    ckfree((char*)(l->next));
    l->next = n;
    return 1;
  }
  return 0;
}


static int
RemoveSuper(OTclClass* cl, OTclClass* super) {

  /*
   * keep corresponding sub in step with super
   */ 

  int sp = RS(cl, super, &cl->super);
  int sb = RS(super, cl, &super->sub);
  return (sp && sb);
}


/*
 * internal type checking
 */


static OTclClass*
InObject(Tcl_Interp* in) {
  Tcl_HashEntry* hp = Tcl_FindHashEntry(theObjects, (char*)in);
  if (hp != 0) return (OTclClass*)Tcl_GetHashValue(hp);
  return 0;
}


static OTclClass*
InClass(Tcl_Interp* in) {
  Tcl_HashEntry* hp = Tcl_FindHashEntry(theClasses, (char*)in);
  if (hp != 0) return (OTclClass*)Tcl_GetHashValue(hp);
  return 0;
}


static int
IsType(OTclObject* obj, OTclClass* type) {
  OTclClass* t = obj ? obj->type : 0;
  while (t && t!=type) t = t->parent;
  return (t != 0);
}


/*
 * methods lookup and dispatch
 */


static int
LookupMethod(Tcl_HashTable* methods, CONST84 char* nm, Tcl_CmdProc** pr,
	     ClientData* cd, Tcl_CmdDeleteProc** dp) 
{
  Tcl_HashEntry *hPtr = Tcl_FindHashEntry(methods, nm);
  if (hPtr != 0) {
    Tcl_CmdInfo* co = (Tcl_CmdInfo*)Tcl_GetHashValue(hPtr);
    if (pr != 0) *pr = co->proc;
    if (cd != 0) *cd = co->clientData;
    if (dp != 0) *dp = co->deleteProc;
    return 1;
  }
  return 0;
}


static OTclClass*
SearchCMethod(OTclClasses* pl, CONST84 char* nm, Tcl_CmdProc** pr,
	      ClientData* cd, Tcl_CmdDeleteProc** dp)
{ 
  while (pl != 0) {
    Tcl_HashTable* cm = &pl->cl->instprocs;
    if (LookupMethod(cm, nm, pr, cd, 0) != 0) break;
    pl = pl->next;
  }
  return pl ? pl->cl : 0;
}


#define OTCLSMALLARGS 8

static int 
OTclDispatch(ClientData cd, Tcl_Interp* in, int argc, CONST84 char* argv[]) {
  OTclObject* self = (OTclObject*)cd;
  Tcl_CmdProc* proc = 0;
  ClientData cp = 0;
  OTclClass* cl = 0;

  if (argc < 2) return OTclErrArgCnt(in, argv[0], "message ?args...?");

  /*
   * try for local methods first, then up the class heirarchy
   */ 

  if (!self->procs || !LookupMethod(self->procs, argv[1], &proc, &cp, 0))
    cl = SearchCMethod(ComputePrecedence(self->cl),argv[1],&proc,&cp,0);

  if (proc) {
    CONST84 char* sargs[OTCLSMALLARGS];
    CONST84 char** args = sargs;
    int result;
    int i;
    
    /*
     * permute args to be:  self self class method <rest>
     * and, if method has no clientdata, pass an object pointer.
     */ 

    cp = (cp != 0) ? cp : cd;
    if (argc+2 > OTCLSMALLARGS)
      args = (CONST84 char**)ckalloc((argc+2)*sizeof(char*));
    args[0] = argv[0]; 
    args[1] = argv[0];
    args[2] = cl ? (char *) Tcl_GetCommandName(in, cl->object.id) : "";
    for (i = 1; i < argc; i++) args[i+2] = argv[i];
	
    /*
    printf("%d ", argc);
    for (i = 0; i < argc; i++)
      printf("%s ", argv[i]);
    printf("\n");
    */
    /*
    for (i = 0; i < argc + 2; i++)
      printf("%s ", args[i]);
    printf("\n");
    */

    result = (*proc)(cp, in, argc+2, (const char **) args);
    /* this adds to the stack trace */
    if (result == TCL_ERROR) {
	    char msg[150];
	    /* old_args2 is because args[2] was getting
	     * clobbered sometimes => seg fault.
	     * ---johnh
	     */
	    CONST84 char *old_args2 = cl ? (char *) Tcl_GetCommandName(in, cl->object.id) : argv[0];
	    sprintf(msg, "\n    (%.40s %.40s line %d)",
		    old_args2, argv[1], in->errorLine);
	    compat_Tcl_AddObjErrorInfo(in, msg, -1);
    }
    if (argc+2 > OTCLSMALLARGS) { ckfree((char*)args); args = 0; }
    return result;
  }

  /*
   * back off and try unknown
   */

  if (!self->procs || !LookupMethod(self->procs, "unknown", &proc, &cp, 0))
    cl = SearchCMethod(ComputePrecedence(self->cl),"unknown",&proc,&cp,0);
  
  if (proc) {
    CONST84 char* sargs[OTCLSMALLARGS];
    CONST84 char** args = sargs;
    int result;
    int i;
    
    /*
     * permute args to be:  self self class method <rest>
     * and, if method has no clientdata, pass an object pointer.
     */ 

    cp = (cp != 0) ? cp : cd;
    if (argc+3 > OTCLSMALLARGS)
      args = (CONST84 char**)ckalloc((argc+3)*sizeof(char*));
    args[0] = argv[0]; 
    args[1] = argv[0];
    args[2] = cl ? (char *) Tcl_GetCommandName(in, cl->object.id) : "";
    args[3] = "unknown";
    for (i = 1; i < argc; i++) args[i+3] = argv[i];
    result = (*proc)(cp, in, argc+3, (const char **) args);
    if (result == TCL_ERROR) {
	    char msg[100];
	    sprintf(msg, "\n    (%.30s unknown line %d)",
		    cl ? args[2] : argv[0], in->errorLine);
	    compat_Tcl_AddObjErrorInfo(in, msg, -1);
    }
    if (argc+3 > OTCLSMALLARGS) { ckfree((char*)args); args = 0; }
    return result;
  }  

  /*
   * and if that fails too, error out
   */

  Tcl_ResetResult(in);
  Tcl_AppendResult(in, argv[0], ": unable to dispatch method ", argv[1], 0);
  return TCL_ERROR;
}


/*
 * autoloading
 */


static void
AutoLoaderDP(ClientData cd) {
  ckfree((char*)cd);
}

static int
AutoLoader(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {

  /*
   * cd is a script to evaluate; object context reconstructed from argv
   */

  OTclObject* obj = OTclGetObject(in, argv[1]);
  OTclClass* cl = argv[2][0] ? OTclGetClass(in, argv[2]) : 0;
  CONST84 char* clname = cl ? argv[2] : "{}"; 
  Tcl_CmdProc* proc = 0;
  ClientData cp = 0;

  if (Tcl_Eval(in, (char*)cd) != TCL_OK) {
    Tcl_AppendResult(in, " during autoloading (object=", argv[1],
		     ", class=", clname, ", proc=", argv[3],")", 0);
    return TCL_ERROR;
  }

  /*
   * the above eval should have displaced this procedure from the object,
   * so check by looking at our old spot in the table, and if successful
   * continue dispatch with the right clientdata.
   */ 

  if (cl)
    (void) LookupMethod(&cl->instprocs, argv[3], &proc, &cp, 0);
  else if (obj->procs)
    (void) LookupMethod(obj->procs, argv[3], &proc, &cp, 0);

  if (proc && proc != (Tcl_CmdProc *) AutoLoader) {
    ClientData cdata = (cp != 0) ? cp : (ClientData)obj;
    return (*proc)(cdata, in, argc, (const char **) argv);
  }
  
  Tcl_ResetResult(in);
  Tcl_AppendResult(in, "no new proc during autoloading (object=", argv[1],
		   ", class=", clname, ", proc=", argv[3],")", 0);
  return TCL_ERROR;
}


int
MakeAuto(Tcl_CmdInfo* proc, CONST84 char* loader) {
  proc->proc = (Tcl_CmdProc *) AutoLoader;
  proc->deleteProc = AutoLoaderDP;
  proc->clientData = (ClientData)strcpy(ckalloc(strlen(loader)+1), loader);
  return (proc->clientData != 0);
}


/*
 * creating, installing, listing and removing procs
 */


static void
AddMethod(Tcl_HashTable* methods, CONST84 char* nm, Tcl_CmdProc* pr,
	  ClientData cd, Tcl_CmdDeleteProc* dp, ClientData dd)
{ 
  int nw = 0;
  Tcl_HashEntry *hPtr = Tcl_CreateHashEntry(methods, nm, &nw);
  Tcl_CmdInfo* co = (Tcl_CmdInfo*)ckalloc(sizeof(Tcl_CmdInfo));
  co->proc = pr;
  co->clientData = cd;
  co->deleteProc = dp;
  co->deleteData = dd;
  Tcl_SetHashValue(hPtr, (ClientData)co);
}


static int
RemoveMethod(Tcl_HashTable* methods, CONST84 char* nm, ClientData cd) { 
  Tcl_HashEntry *hPtr = Tcl_FindHashEntry(methods, nm);
  if (hPtr != 0) {
    Tcl_CmdInfo* co = (Tcl_CmdInfo*)Tcl_GetHashValue(hPtr);
    if (co->deleteProc != 0) (*co->deleteProc)(co->deleteData);
    ckfree((char*)co);
    Tcl_DeleteHashEntry(hPtr);
    return 1;
  }
  return 0;
}

#if TCL_MAJOR_VERSION >= 8
typedef struct {
	Tcl_Interp* interp;	
	int procUid;
} OTclDeleteProcData;


static int s_ProcUid=0; 
static const char s_otclProcPrefix[] = "::otcl::p";
static char s_otclProcName[sizeof(s_otclProcPrefix) + 8];

const char* GetProcName(int index)
{
	sprintf(s_otclProcName, "%s%d", s_otclProcPrefix, index);
	return s_otclProcName;
}

static void
OTclDeleteProc(ClientData cd)
{
  OTclDeleteProcData* pdpd = (OTclDeleteProcData*)cd;
  /* cleanup, ignore any errors */
  Tcl_Command cmd;
  cmd = Tcl_FindCommand(pdpd->interp, (char*)GetProcName(pdpd->procUid),
			(Tcl_Namespace*)NULL, 0);
  if (cmd) 
	  Tcl_DeleteCommandFromToken(pdpd->interp, cmd);
  ckfree((char*)pdpd);
}
#endif  


int
MakeProc(Tcl_CmdInfo* proc, Tcl_Interp* in, int argc, CONST84 char* argv[]) {    
  CONST84 char* name = argv[1];
  CONST84 char* oargs = argv[2];
  CONST84 char* nargs = (CONST84 char*)ckalloc(strlen("self class proc ")+strlen(argv[2])+1);
  int ok = 0;
  CONST84 char* id;

#if TCL_MAJOR_VERSION >= 8
  Tcl_Obj **objv;
  int i;
  id= (char*)GetProcName(++s_ProcUid);
#else
  id= "__OTclProc__";
#endif

  /*
   * add the standard method args automatically
   */

  argv[1] = id;
  (void)strcpy((char *)nargs, "self class proc ");
  if (argv[2][0] != 0) (void) strcat((char *)nargs, argv[2]);
  argv[2] = nargs;

#if TCL_MAJOR_VERSION >= 8
  objv = (Tcl_Obj **)ckalloc(argc * sizeof(Tcl_Obj *));
  for (i = 0; i < argc; i++) {
    objv[i] = Tcl_NewStringObj(argv[i], -1); /* let strlen() decide length */
    Tcl_IncrRefCount(objv[i]);
  }

  /*
   * use standard Tcl_ProcCmd to digest, and fish result out of interp
   */
  if (Tcl_ProcObjCmd(0, in, argc, objv) == TCL_OK) {
    if (Tcl_GetCommandInfo(in, id, proc) && proc->proc == ProcInterpId) {
      OTclDeleteProcData* pData =
        (OTclDeleteProcData*)(ckalloc(sizeof(OTclDeleteProcData)));

      pData->procUid = s_ProcUid;
      pData->interp = in;

      /* set the delete procedure to be OTclDeleteProc, which will
       * remove the procedure, the deleteProc will be called in, for example,
       * RemoveMethod, note that we are changing a copy of proc, the original
       * proc structure still has the right deleteProc */
      proc->deleteProc = OTclDeleteProc;
      proc->deleteData = (ClientData)pData;
      ok = 1;
    }
  }

  for (i = 0; i < argc; i++)
    Tcl_DecrRefCount(objv[i]);
  ckfree((char *)objv);
  
#else /* TCL_MAJOR_VERSION < 8 */
  
  if (Tcl_ProcCmd(0, in, argc, argv) == TCL_OK) {
    if (Tcl_GetCommandInfo(in, id, proc) && proc->proc == ProcInterpId) {
      Tcl_CmdDeleteProc* dp = proc->deleteProc;
      proc->deleteProc = 0;
      if (Tcl_SetCommandInfo(in, id, proc))
      	(void)Tcl_DeleteCommand(in, id);
      proc->deleteProc = dp;
      ok = 1;
    }
  }

#endif /* TCL_MAJOR_VERSION < 8 */

  ckfree((char*)nargs);
  argv[1] = name;
  argv[2] = oargs;
  
  return ok;
}

static void
ListKeys(Tcl_Interp* in, Tcl_HashTable* table, CONST84 char* pattern) {
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr = table ? Tcl_FirstHashEntry(table, &hSrch) : 0;
  Tcl_ResetResult(in);
  for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
    char* key = Tcl_GetHashKey(table, hPtr);
    if (!pattern || Tcl_StringMatch(key, pattern))
      Tcl_AppendElement(in, key);
  }
}


static void
ListInstanceKeys(Tcl_Interp* in, Tcl_HashTable* table, CONST84 char* pattern) {
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr = table ? Tcl_FirstHashEntry(table, &hSrch) : 0;
  Tcl_ResetResult(in);
  for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
    OTclObject* obj = (OTclObject*)Tcl_GetHashKey(table, hPtr);
    CONST84 char* name = (char *) Tcl_GetCommandName(in, obj->id);
    if (!pattern || Tcl_StringMatch(name, pattern))
      Tcl_AppendElement(in, name);
  }
}


static void
ListProcKeys(Tcl_Interp* in, Tcl_HashTable* table, CONST84 char* pattern) {
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr = table ? Tcl_FirstHashEntry(table, &hSrch) : 0;
  Tcl_ResetResult(in);
  for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
    CONST84 char* key = Tcl_GetHashKey(table, hPtr);
    Tcl_CmdProc* proc = ((Tcl_CmdInfo*)Tcl_GetHashValue(hPtr))->proc;
    if (pattern && !Tcl_StringMatch(key, pattern)) continue;
    
    /*
     * also counts anything to be autoloaded as a proc
     */

    if (proc!=(Tcl_CmdProc *) AutoLoader && proc!=ProcInterpId) continue; 
    Tcl_AppendElement(in, key);
  }
}


static Proc*
FindProc(Tcl_HashTable* table, CONST84 char* name) {
  Tcl_HashEntry* hPtr = table ? Tcl_FindHashEntry(table, name) : 0;
  if (hPtr) {
    Tcl_CmdInfo* co = (Tcl_CmdInfo*)Tcl_GetHashValue(hPtr);
    if (co->proc == ProcInterpId)
      return (Proc*)co->clientData;
  }
  return 0;
}
 

static int
ListProcArgs(Tcl_Interp* in, Tcl_HashTable* table, CONST84 char* name) {
  Proc* proc = FindProc(table, name);
  if (proc) {
#if TCL_MAJOR_VERSION == 7
    Arg* args = proc->argPtr;
#else
    CompiledLocal* args = proc->firstLocalPtr;
#endif
    int i = 0;

    /*
     * skip over hidden self, class, proc args 
     */

    for (; args!=0 && i<3; args = args->nextPtr, i++) ;
    Tcl_ResetResult(in);
    while (args != 0) {
#if TCL_MAJOR_VERSION >= 8
/*#if TCL_RELEASE_SERIAL >= 3*/
#if ((TCL_MINOR_VERSION == 0) && (TCL_RELEASE_SERIAL >= 3)) || (TCL_MINOR_VERSION > 0)
	    if (TclIsVarArgument(args))
#else
	    if (args->isArg)
#endif
#endif
		    Tcl_AppendElement(in, args->name);
	    args = args->nextPtr;
    }
    return TCL_OK;
  }
  return OTclErrBadVal(in, "a tcl method name", name);
}


static int
ListProcDefault(Tcl_Interp* in, Tcl_HashTable* table,
		CONST84 char* name, CONST84 char* arg, CONST84 char* var)
{

  /*
   * code snarfed from tcl info default
   */

  Proc* proc = FindProc(table, name);
  if (proc) {
#if TCL_MAJOR_VERSION < 8
    Arg *ap;
    for (ap = proc->argPtr; ap != 0; ap = ap->nextPtr) {
      if (strcmp(arg, ap->name) != 0) continue;
      if (ap->defValue != 0) {
	if (Tcl_SetVar(in, var, ap->defValue, 0) == 0) {
#else 
    CompiledLocal *ap;
    for (ap = proc->firstLocalPtr; ap != 0; ap = ap->nextPtr) {
      if (strcmp(arg, ap->name) != 0) continue;
      if (ap->defValuePtr != 0) {
	if (Tcl_SetVar(in, 
		       var, 
#if TCL_MINOR_VERSION == 0
		       TclGetStringFromObj(ap->defValuePtr, 
					   (int *) NULL),
#else
		       TclGetString(ap->defValuePtr),
#endif
		       0) == NULL) {
#endif
	  Tcl_ResetResult(in);
	  Tcl_AppendResult(in, "couldn't store default value in variable \"",
			   var, "\"", (char *) 0);
	  return TCL_ERROR;
	}
	Tcl_SetResult(in, "1", TCL_STATIC);
      } else {
	if (Tcl_SetVar(in, var, "", 0) == 0) {
	  Tcl_AppendResult(in, "couldn't store default value in variable \"",
			   var, "\"", (char *) 0);
	  return TCL_ERROR;
	}
	Tcl_SetResult(in, "0", TCL_STATIC);
      }
      return TCL_OK;
    }
    Tcl_ResetResult(in);
    Tcl_AppendResult(in, "procedure \"", name,
		     "\" doesn't have an argument \"", arg, "\"", (char *) 0);
    return TCL_ERROR;
  }
  return OTclErrBadVal(in, "a tcl method name", name);
}


static int
ListProcBody(Tcl_Interp* in, Tcl_HashTable* table, CONST84 char* name) {
  Proc* proc = FindProc(table, name);
  if (proc) {
    Tcl_ResetResult(in);
#if TCL_MAJOR_VERSION< 8
    Tcl_AppendResult(in, proc->command, 0);
#else 
    Tcl_AppendResult(in, 
#if TCL_MINOR_VERSION == 0
		     TclGetStringFromObj(proc->bodyPtr, (int *)NULL),
#else
		     TclGetString(proc->bodyPtr),
#endif
		     0);
#endif
    return TCL_OK;
  }
  return OTclErrBadVal(in, "a tcl method name", name);
}
 
/*
 * object creation
 */

static void
PrimitiveOInit(void* mem, Tcl_Interp* in, CONST84 char* name, OTclClass* cl) {
  OTclObject* obj = (OTclObject*)mem;

  obj->teardown = in;
  AddInstance(obj, cl);
  obj->type = InObject(in);
  obj->procs = 0;
  
  /*
   * fake callframe needed to interface to tcl variable
   * manipulations. looks like one below global 
   */

  Tcl_InitHashTable(ObjVarTablePtr(obj), TCL_STRING_KEYS);
  obj->variables.level = 1;
#if TCL_MAJOR_VERSION < 8
  obj->variables.argc = 0;
  obj->variables.argv = 0;
#else
  obj->variables.numCompiledLocals = 0;
  obj->variables.compiledLocals = 0;
#endif
  obj->variables.callerPtr = 0;
  obj->variables.callerVarPtr = 0;

#if TCL_MAJOR_VERSION >= 8
  /* we need to deal with new members in CallFrame in Tcl8.0 */
  obj->variables.isProcCallFrame = 1;
  /* XXX: is it correct to assign global namespace here? */
  obj->variables.nsPtr = ((Interp *)in)->globalNsPtr; 
  obj->variables.objc = 0;
  obj->variables.objv = NULL; /* we don't want byte codes for now */
  obj->variables.procPtr = (Proc *) ckalloc(sizeof(Proc));
  obj->variables.procPtr->iPtr = (Interp *)in;
  obj->variables.procPtr->refCount = 1;
  /* XXX it correct to assign global namespace here? */
  obj->variables.procPtr->cmdPtr = NULL;
  obj->variables.procPtr->bodyPtr = NULL;
  obj->variables.procPtr->numArgs  = 0;	/* actual argument count is set below. */
  obj->variables.procPtr->numCompiledLocals = 0;
  obj->variables.procPtr->firstLocalPtr = NULL;
  obj->variables.procPtr->lastLocalPtr = NULL;
#endif
}


static void PrimitiveODestroyNoFree(ClientData cd);

static void 
PrimitiveODestroy(ClientData cd) {
  PrimitiveODestroyNoFree(cd);   
  ckfree((char*)cd);
}

static void
PrimitiveODestroyNoFree(ClientData cd) {
  OTclObject* obj = (OTclObject*)cd;
  Tcl_HashSearch hs;
  Tcl_HashEntry* hp;
  Tcl_HashSearch hs2;
  Tcl_HashEntry* hp2;
  Tcl_Interp* in;

  /*
   * check and latch against recurrent calls with obj->teardown
   */

  if (!obj || !obj->teardown) return;
  in = obj->teardown; obj->teardown = 0;

  /*
   * call and latch user destroy with obj->id if we haven't
   */

  if (obj->id) {
    CONST84 char* args[2] = { "", "destroy" };
    Tcl_CmdInfo info;

    /*
     * but under 7.4p1 it is too late, so check with info
     */

    args[0] = (char *) Tcl_GetCommandName(in, obj->id);
    if (Tcl_GetCommandInfo(in, args[0], &info))
      (void) OTclDispatch(cd, in, 2, args);
    obj->id = 0;
  }

  /*
   * resume the primitive teardown for procs and variables.
   * variables unset here were lost from user destroy, and
   * any trace error messages will be swallowed.
   */
  hp = Tcl_FirstHashEntry(ObjVarTablePtr(obj), &hs);
  while (hp != 0) {
    for (;;) {
      Var* vp = (Var*)Tcl_GetHashValue(hp);
      if (!TclIsVarUndefined(vp)) break;
      hp = Tcl_NextHashEntry(&hs);
      if (hp == 0)
	      goto done;
    }
    if (hp != 0) {
      char* name = Tcl_GetHashKey(ObjVarTablePtr(obj), hp);
      (void)OTclUnsetInstVar(obj, in, name, TCL_LEAVE_ERR_MSG);
    }
    hp = Tcl_FirstHashEntry(ObjVarTablePtr(obj), &hs);
  }
done:
  hp = Tcl_FirstHashEntry(ObjVarTablePtr(obj), &hs);
  while (hp != 0) {
   /*
    * We delete the hash table below so disassociate
    * each remaining (undefined) var from its hash table entry.
    * (Otherwise, tcl will later try to delete
    * the already-freed hash table entry.)
    */
    Var* vp = (Var*)Tcl_GetHashValue(hp);
    vp->hPtr = 0;
    hp = Tcl_NextHashEntry(&hs);
  }
  Tcl_DeleteHashTable(ObjVarTablePtr(obj));
  hp2 = obj->procs ? Tcl_FirstHashEntry(obj->procs, &hs2) : 0; 
  for (; hp2 != 0; hp2 = Tcl_NextHashEntry(&hs2)) {
    Tcl_CmdInfo* co = (Tcl_CmdInfo*)Tcl_GetHashValue(hp2);
    ClientData cdest = cd;
    if (co->clientData != 0) cdest = co->clientData;
    if (co->deleteProc != 0) (*co->deleteProc)(co->deleteData);
    ckfree((char*)co);
  }
  if (obj->procs) {
    Tcl_DeleteHashTable(obj->procs); ckfree((char*)(obj->procs));
  }

  (void)RemoveInstance(obj, obj->cl);

#if TCL_MAJOR_VERSION >= 8
  ckfree((char*)(obj->variables.procPtr));
  ckfree((char*)(obj->variables.varTablePtr));
#endif
}


static OTclObject*
PrimitiveOCreate(Tcl_Interp* in, CONST84 char* name, OTclClass* cl) {
  OTclObject* obj = (OTclObject*)ckalloc(sizeof(OTclObject)); 
#if TCL_MAJOR_VERSION < 8
  if (obj != 0) {
    PrimitiveOInit(obj, in, name, cl);
    obj->id = Tcl_CreateCommand(in, name, OTclDispatch, (ClientData)obj,
				PrimitiveODestroy);
  }
#else
  obj->variables.varTablePtr = (Tcl_HashTable *)ckalloc(sizeof(Tcl_HashTable));
  if (obj != 0)
    if (obj->variables.varTablePtr != 0) {
      PrimitiveOInit(obj, in, name, cl);
      obj->id = Tcl_CreateCommand(in, name, (Tcl_CmdProc *) OTclDispatch, 
				  (ClientData)obj, PrimitiveODestroy);
    } else {
      ckfree((char *)obj);
      obj = NULL;
    }
#endif
  return obj;
}

static void
PrimitiveCInit(void* mem, Tcl_Interp* in, CONST84 char* name, OTclClass* class) {
  OTclObject* obj = (OTclObject*)mem;
  OTclClass* cl = (OTclClass*)mem;

  obj->type = InClass(in);
  cl->super = 0;
  cl->sub = 0;
  AddSuper(cl, InObject(in));
  cl->parent = InObject(in);
  cl->color = WHITE;
  cl->order = 0;
  Tcl_InitHashTable(&cl->instprocs, TCL_STRING_KEYS);
  Tcl_InitHashTable(&cl->instances, TCL_ONE_WORD_KEYS);
  cl->objectdata = 0;
}


static void
PrimitiveCDestroy(ClientData cd) {
  OTclClass* cl = (OTclClass*)cd;
  OTclObject* obj = (OTclObject*)cd;
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr;
  Tcl_Interp* in;

  /*
   * check and latch against recurrent calls with obj->teardown
   */
  
  if (!obj || !obj->teardown) return;
  in = obj->teardown; obj->teardown = 0;

  /*
   * call and latch user destroy with obj->id if we haven't
   */

  if (obj->id) {
    CONST84 char* args[2] = { "", "destroy" };
    Tcl_CmdInfo info;

    /*
     * but under 7.4p1 it is too late, so check with info
     */

    args[0] = (char *) Tcl_GetCommandName(in, obj->id);
    if (Tcl_GetCommandInfo(in, args[0], &info))
      (void) OTclDispatch(cd, in, 2, args);
    obj->id = 0;
  }

  /*
   * resume the primitive teardown for instances and instprocs
   */ 

  hPtr = Tcl_FirstHashEntry(&cl->instances, &hSrch);
  while (hPtr) {
    /*
     * allow circularity for meta-classes
     */
    OTclObject* inst;
    for (;;) {
      inst = (OTclObject*)Tcl_GetHashKey(&cl->instances, hPtr);
      if (inst != (OTclObject*)cl) {
        CONST84 char* name = (char *) Tcl_GetCommandName(inst->teardown, inst->id);
	(void)Tcl_DeleteCommand(inst->teardown, name);
	break;
      }
      hPtr = Tcl_NextHashEntry(&hSrch);
      if (hPtr == 0)
	goto done;
    }
    hPtr = Tcl_FirstHashEntry(&cl->instances, &hSrch);
  }
done:
  hPtr = Tcl_FirstHashEntry(&cl->instprocs, &hSrch); 
  for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
    /* for version 8 the instprocs are registered, so no need to delete them (?) */
    Tcl_CmdInfo* co = (Tcl_CmdInfo*)Tcl_GetHashValue(hPtr);
    ClientData cdest = cd;
    if (co->clientData != 0) cdest = co->clientData;
    if (co->deleteProc != 0) (*co->deleteProc)(co->deleteData);
    ckfree((char*)co);
  }
  Tcl_DeleteHashTable(&cl->instprocs);

  if (cl->objectdata) {
    Tcl_DeleteHashTable(cl->objectdata);
    ckfree((char*)(cl->objectdata)); cl->objectdata = 0;
  }

  /*
   * flush all caches, unlink superclasses
   */ 

  FlushPrecedences(cl);
  while (cl->super) (void)RemoveSuper(cl, cl->super->cl);
  while (cl->sub) (void)RemoveSuper(cl->sub->cl, cl);

  /*
   * handoff the primitive teardown
   */

  obj->teardown = in;  
  /* don't want to free the memory since we need to 
   * delete the hash table later, because we want the 
   * PrimitiveODestory to destory the hash entries first */
  PrimitiveODestroyNoFree(cd);
  Tcl_DeleteHashTable(&cl->instances);
  ckfree((char*)cd);
}


static OTclClass*
PrimitiveCCreate(Tcl_Interp* in, CONST84 char* name, OTclClass* class){
  OTclClass* cl = (OTclClass*)ckalloc(sizeof(OTclClass));
#if TCL_MAJOR_VERSION < 8
  if (cl != 0) {
    OTclObject* obj = (OTclObject*)cl;
    PrimitiveOInit(obj, in, name, class);
    PrimitiveCInit(cl, in, name, class);
    obj->id = Tcl_CreateCommand(in, name, OTclDispatch, (ClientData)cl,
				PrimitiveCDestroy);
  }
#else
  cl->object.variables.varTablePtr = (Tcl_HashTable *)ckalloc(sizeof(Tcl_HashTable));
  if (cl != 0) 
    if (cl->object.variables.varTablePtr != 0) {
      OTclObject* obj = (OTclObject*)cl;
      PrimitiveOInit(obj, in, name, class);
      PrimitiveCInit(cl, in, name, class);
      obj->id = Tcl_CreateCommand(in, name, (Tcl_CmdProc *) OTclDispatch, 
				  (ClientData)cl, PrimitiveCDestroy);
    } else {
      ckfree((char *)cl);
      cl = NULL;
    }
#endif
  return cl;
}


/*
 * object method implementations
 */


static int
OTclOAllocMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclClass* cl = OTclAsClass(in, cd);
  OTclObject* newobj;
  int i;

  if (!cl) return OTclErrType(in, argv[0], "Class");
  if (argc < 5) return OTclErrArgCnt(in, argv[0], "alloc <obj> ?args?");
  newobj = PrimitiveOCreate(in, argv[4], cl);
  if (newobj == 0) return OTclErrMsg(in,"Object alloc failed", TCL_STATIC);
  
  /*
   * this alloc doesn't process any extra args, so return them all
   */

  Tcl_ResetResult(in);
  for (i = 5; i < argc; i++) Tcl_AppendElement(in, argv[i]);
  return TCL_OK;
}


static int
OTclOInitMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclObject* obj = OTclAsObject(in, cd);
  int i;

  if (!obj) return OTclErrType(in, argv[0], "Object");
  if (argc < 4) return OTclErrArgCnt(in, argv[0], "init ?args?");
  if (argc & 1) return OTclErrMsg(in, "uneven number of args", TCL_STATIC);

  for (i=4; i<argc; i+=2) {
    int result;
    CONST84 char* args[3];
    args[0] = argv[0];
    args[1] = argv[i]; if (args[1][0] == '-') args[1]++;
    args[2] = argv[i+1];
    result = OTclDispatch(cd, in, 3, args);
    if (result != TCL_OK) {
      Tcl_AppendResult(in, " during {", args[0], "} {",
		       args[1], "} {", args[2], "}", 0);
      return result;
    }
  }
  Tcl_ResetResult(in);
  return TCL_OK;
}


static int
OTclODestroyMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclObject* obj = OTclAsObject(in, cd);
  Tcl_HashSearch hs;
  Tcl_HashEntry* hp;
  Tcl_Command oid;
  int result = TCL_OK;

  if (!obj) return OTclErrType(in, argv[0], "Object");
  if (argc != 4) return OTclErrArgCnt(in, argv[0], "destroy");

  /*
   * unset variables here, while it may not be too late
   * to deal with trace error messages
   */

  hp = Tcl_FirstHashEntry(ObjVarTablePtr(obj), &hs);
  while (hp != 0) {
    for (;;) {
      Var* vp = (Var*)Tcl_GetHashValue(hp);
      if (!TclIsVarUndefined(vp)) break;
      hp = Tcl_NextHashEntry(&hs);
      if (hp == 0)
        goto done;
    }
    if (hp != 0) {
      CONST84 char* name = Tcl_GetHashKey(ObjVarTablePtr(obj), hp);
      result = OTclUnsetInstVar(obj, in, name, TCL_LEAVE_ERR_MSG);
      if (result != TCL_OK) break;
    }
    hp = Tcl_FirstHashEntry(ObjVarTablePtr(obj), &hs);
  }
  if (hp != 0) return TCL_ERROR;
done:
  /*
   * latch, and call delete command if not already in progress
   */

  oid = obj->id; obj->id = 0;
  if (obj->teardown != 0) {
    CONST84 char* name = (char *) Tcl_GetCommandName(in, oid);
    return (Tcl_DeleteCommand(in, name) == 0) ? TCL_OK : TCL_ERROR;
  }
  
  Tcl_ResetResult(in);
  return TCL_OK;
}


static int
OTclOClassMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclObject* obj = OTclAsObject(in, cd);
  OTclClass* cl;

  if (!obj) return OTclErrType(in, argv[0], "Object");
  if (argc != 5) return OTclErrArgCnt(in, argv[0], "class <class>");

  /*
   * allow a change to any class; type system enforces safety later
   */

  cl = OTclGetClass(in, argv[4]);
  if (!cl) return OTclErrBadVal(in, "a class", argv[4]);
  (void)RemoveInstance(obj, obj->cl);
  AddInstance(obj, cl);
  return TCL_OK;
}


static int OTclCInfoMethod(ClientData, Tcl_Interp*, int, CONST84 char*[]);

static int
OTclOInfoMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclObject* obj = OTclAsObject(in, cd);

  if (!obj) return OTclErrType(in, argv[0], "Object");
  if (argc < 5) return OTclErrArgCnt(in,argv[0],"info <opt> ?args?");
  
  if (!strcmp(argv[4], "class")) {
    if (argc > 6) return OTclErrArgCnt(in,argv[0],"info class ?class?");
    if (argc == 5) {
      Tcl_SetResult(in, (char *)Tcl_GetCommandName(in, obj->cl->object.id),
		    TCL_VOLATILE);
    } else {
      int result;
      CONST84 char* saved = argv[4];
      argv[4] = "superclass";
      result = OTclCInfoMethod((ClientData)obj->cl, in, argc, argv);
      argv[4] = saved;
      return result;
    }
  } else if (!strcmp(argv[4], "commands")) {
    if (argc > 6) return OTclErrArgCnt(in,argv[0],"info commands ?pat?");
    ListKeys(in, obj->procs, (argc == 6) ? argv[5] : 0);
  } else if (!strcmp(argv[4], "procs")) {
    if (argc > 6) return OTclErrArgCnt(in,argv[0],"info procs ?pat?");
    ListProcKeys(in, obj->procs, (argc == 6) ? argv[5] : 0);
  } else if (!strcmp(argv[4], "args")) {
    if (argc != 6) return OTclErrArgCnt(in,argv[0],"info args <proc>");
    return ListProcArgs(in, obj->procs, argv[5]);
  } else if (!strcmp(argv[4], "default")) {
    if (argc != 8)
      return OTclErrArgCnt(in,argv[0],"info default <proc> <arg> <var>");
    return ListProcDefault(in, obj->procs, argv[5], argv[6], argv[7]);
  } else if (!strcmp(argv[4], "body")) {
    if (argc != 6) return OTclErrArgCnt(in,argv[0],"info body <proc>");
    return ListProcBody(in, obj->procs, argv[5]);
  } else if (!strcmp(argv[4], "vars")) {
    if (argc > 6) return OTclErrArgCnt(in,argv[0],"info vars ?pat?");
    ListKeys(in, ObjVarTablePtr(obj), (argc == 6) ? argv[5] : 0);
  } else {
    return OTclErrBadVal(in, "an info option", argv[4]);
  }
  return TCL_OK;
}


static int
OTclOProcMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclObject* obj = OTclAsObject(in, cd);
  Tcl_CmdInfo proc;
  int op;

  if (!obj) return OTclErrType(in, argv[0], "Object");
  if (argc != 7) return OTclErrArgCnt(in,argv[0],"proc name args body");

  /*
   * if the args list is "auto", the body is a script to load the proc
   */

  if (!strcmp("auto", argv[5])) op = MakeAuto(&proc, argv[6]);
  else if (argv[5][0]==0 && argv[6][0]==0) op = -1;
  else op = MakeProc(&proc,in, argc-3, argv+3);

  if (!obj->procs) {
    obj->procs = (Tcl_HashTable*)ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(obj->procs, TCL_STRING_KEYS);
  }
  (void)RemoveMethod(obj->procs, argv[4], (ClientData)obj);
  if (op == 1) AddMethod(obj->procs, argv[4], proc.proc,
			 proc.clientData, proc.deleteProc, proc.deleteData);
  
  return (op != 0) ? TCL_OK : TCL_ERROR;
}


static int
OTclONextMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclObject* obj = OTclAsObject(in, cd);
  CONST84 char* class = (char *) Tcl_GetVar(in, "class",0);
  CONST84 char* method = (char *) Tcl_GetVar(in, "proc",0);

  if (!obj) return OTclErrType(in, argv[0], "Object");
  if (argc < 4) return OTclErrArgCnt(in, argv[0], "next ?args?");
  if (!method||!class) return OTclErrMsg(in,"no executing proc", TCL_STATIC);

  argv[2] = class;
  argv[3] = method;
  return OTclNextMethod(obj, in, argc, argv);
}


static int
OTclOSetMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclObject* obj = OTclAsObject(in, cd);
  CONST84 char* result;

  if (!obj) return OTclErrType(in, argv[0], "Object");
  if (argc<5 || argc>6) return OTclErrArgCnt(in, argv[0], "set var ?value?");
  if (argc == 6)
    result = OTclSetInstVar(obj, in, argv[4], argv[5], TCL_LEAVE_ERR_MSG);
  else
    result = OTclGetInstVar(obj, in, argv[4], TCL_LEAVE_ERR_MSG);
  if (result != 0) Tcl_SetResult(in, (char *)result, TCL_VOLATILE); 
  return (result != 0) ? TCL_OK : TCL_ERROR;
}


static int
OTclOUnsetMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclObject* obj = OTclAsObject(in, cd);
  int result = TCL_ERROR;
  int i;

  if (!obj) return OTclErrType(in, argv[0], "Object");
  if (argc < 5) return OTclErrArgCnt(in, argv[0], "unset ?vars?");

  for (i=4; i<argc; i++) {
    result = OTclUnsetInstVar(obj, in, argv[i], TCL_LEAVE_ERR_MSG);
    if (result != TCL_OK) break;
  }
  return result;
}

/*
 * This (fairly low-level) routine is exported to allow tclcl
 * to avoid generating/evaling tcl code to do its instvar.
 */
int
OTclOInstVarOne(OTclObject* obj, Tcl_Interp *in, char *frameName, CONST84 char *varName, CONST84 char *localName, int flags)
{
  Interp* iPtr = (Interp*)in;
  int result = TCL_ERROR;

  /*
   * Fake things as if the caller's stack frame is just over
   * the object, then use UpVar to suck the object's variable
   * into the caller.
   *
   * Patched for global instvar by Orion Hodson <O.Hodson@cs.ucl.ac.uk>
   */
  if (iPtr->varFramePtr) {
  	CallFrame* saved = iPtr->varFramePtr->callerVarPtr;
  	int level = iPtr->varFramePtr->level;

 	iPtr->varFramePtr->callerVarPtr = &obj->variables;
  	iPtr->varFramePtr->level = obj->variables.level+1;
  	result = Tcl_UpVar(in, frameName, varName, localName, flags);
  	iPtr->varFramePtr->callerVarPtr = saved;
  	iPtr->varFramePtr->level = level;
  } else {
  	Tcl_SetResult(in, "no instvar in global :: scope", TCL_STATIC);
  }
  return result;
}

static int
OTclOInstVarMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[])
{
  OTclObject* obj = OTclAsObject(in, cd);
  int i;
  int result = TCL_ERROR;

  if (!obj) return OTclErrType(in, argv[0], "Object");
  if (argc < 5) return OTclErrArgCnt(in, argv[0], "instvar ?vars?");

  for (i=4; i<argc; i++) {
    int ac;
    CONST84 char **av;
    result = Tcl_SplitList(in, argv[i], &ac, (const char ***) &av);
    if (result != TCL_OK) break;
    if (ac == 1) {
      result = OTclOInstVarOne(obj, in, "1", av[0], av[0], 0);
    } else if (ac == 2) {
      result = OTclOInstVarOne(obj, in, "1", av[0], av[1], 0);
    } else {
      result = TCL_ERROR;
      Tcl_ResetResult(in);
      Tcl_AppendResult(in, "expected ?inst/local? or ?inst? ?local? but got ",
		       argv[i]);
    }
    ckfree((char*)av);
    if (result != TCL_OK) break;
  }
  return result;
}


/*
 * class method implementations
 */


static int
OTclCAllocMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclClass* cl = OTclAsClass(in, cd);
  OTclClass* newcl;
  int i;

  if (!cl) return OTclErrType(in, argv[0], "Class");
  if (argc < 5) return OTclErrArgCnt(in, argv[0], "alloc <cl> ?args?");
  newcl = PrimitiveCCreate(in, argv[4], cl);
  if (newcl == 0) return OTclErrMsg(in,"Class alloc failed", TCL_STATIC);

  /*
   * this alloc doesn't process any extra args, so return them all
   */

  Tcl_ResetResult(in);
  for (i = 5; i < argc; i++) Tcl_AppendElement(in, argv[i]);
  return TCL_OK;
}


static int
OTclCCreateMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclClass* cl = OTclAsClass(in, cd);
  OTclObject* obj;
  Tcl_CmdProc* proc = 0;
  ClientData cp = 0;
  OTclClasses* pl;
  CONST84 char* args[4];
  int result;
  int i;

  if (!cl) return OTclErrType(in, argv[0], "Class");
  if (argc < 5) return OTclErrArgCnt(in, argv[0], "create <obj> ?args?");
  for (pl = ComputePrecedence(cl); pl != 0; pl = pl->next) {
    Tcl_HashTable* procs = pl->cl->object.procs;
    if (procs && LookupMethod(procs,"alloc",&proc,&cp,0)) break;
  }
  if (!pl) return OTclErrMsg(in, "no reachable alloc", TCL_STATIC);
  
  for (i=0; i<4; i++) args[i] = argv[i];
  argv[0] = (char *) Tcl_GetCommandName(in, pl->cl->object.id);
  argv[1] = argv[0];
  argv[2] = "";
  argv[3] = "alloc";
  cp = (cp != 0) ? cp : (ClientData)pl->cl;
  result = (*proc)(cp, in, argc, (const char **) argv);
  for (i=0; i<4; i++) argv[i] = args[i];
  if (result != TCL_OK) return result;

  obj = OTclGetObject(in, argv[4]);
  if (obj == 0) 
	  return OTclErrMsg(in, "couldn't find result of alloc", TCL_STATIC);
  (void)RemoveInstance(obj, obj->cl);
  AddInstance(obj, cl);

  result = Tcl_VarEval(in, argv[4], " init ", in->result, 0);
  if (result != TCL_OK) return result;
  Tcl_SetResult(in, (char *)argv[4], TCL_VOLATILE);
  return TCL_OK;
}


static int
OTclCSuperClassMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclClass* cl = OTclAsClass(in, cd);
  OTclClasses* osl = 0;
  int ac = 0;
  CONST84 char** av = 0;
  OTclClass** scl = 0;
  int reversed = 0;
  int i, j;

  if (!cl) return OTclErrType(in, argv[0], "Class");
  if (argc != 5) return OTclErrArgCnt(in, argv[0], "superclass <classes>");
  if (Tcl_SplitList(in, argv[4], &ac, (const char ***) &av) != TCL_OK)
    return TCL_ERROR;

  scl = (OTclClass**)ckalloc(ac*sizeof(OTclClass*));
  for (i = 0; i < ac; i++) {
    scl[i] = OTclGetClass(in, av[i]);
    if (!scl[i]) {

      /*
       * try to force autoloading if we can't resolve a class name
       */

      int loaded = 0;
      char* args = (char*)ckalloc(strlen("auto_load ")+strlen(av[i])+1);
      (void)strcpy(args, "auto_load ");
      (void) strcat(args, av[i]);
      if (Tcl_Eval(in, args) == TCL_OK) {
	scl[i] = OTclGetClass(in, av[i]);
	loaded = (scl[i] != 0);
      }
      ckfree(args);
      if (!loaded) {
	ckfree((char*)av);
	ckfree((char*)scl);
	return OTclErrBadVal(in, "a list of classes", argv[4]);
      }
    }
  }

  /*
   * check that superclasses don't precede their classes
   */

  for (i = 0; i < ac; i++) {
    if (reversed != 0) break;
    for (j = i+1; j < ac; j++) {
      OTclClasses* dl = ComputePrecedence(scl[j]);
      if (reversed != 0) break;
      while (dl != 0) {
	if (dl->cl == scl[i]) break;
	dl = dl->next;
      }
      if (dl != 0) reversed = 1;
    }
  }
  
  if (reversed != 0) {
    ckfree((char*)av);
    ckfree((char*)scl);
    return OTclErrBadVal(in, "classes in dependence order", argv[4]);
  }
  
  while (cl->super != 0) {
    
    /*
     * build up an old superclass list in case we need to revert
     */ 

    OTclClass* sc = cl->super->cl;
    OTclClasses* l = osl;
    osl = (OTclClasses*)ckalloc(sizeof(OTclClasses));
    osl->cl = sc;
    osl->next = l;
    (void)RemoveSuper(cl, cl->super->cl);
  }
  for (i = 0; i < ac; i++)
    AddSuper(cl, scl[i]);
  ckfree((char*)av);
  ckfree((char*)scl);
  FlushPrecedences(cl);
  
  if (!ComputePrecedence(cl)) {

    /*
     * cycle in the superclass graph, backtrack
     */ 

    OTclClasses* l;
    while (cl->super != 0) (void)RemoveSuper(cl, cl->super->cl);
    for (l = osl; l != 0; l = l->next) AddSuper(cl, l->cl);
    RC(osl);
    return OTclErrBadVal(in, "a cycle-free graph", argv[4]);
  }
  RC(osl);
  Tcl_ResetResult(in);
  return TCL_OK;
}


static int
OTclCInfoMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclClass* cl = OTclAsClass(in, cd);

  if (!cl) return OTclErrType(in, argv[0], "Class");
  if (argc < 5) return OTclErrArgCnt(in,argv[0],"info <opt> ?args?");

  if (!strcmp(argv[4], "superclass")) {
    if (argc > 6) return OTclErrArgCnt(in,argv[0],"info superclass ?class?");
    if (argc == 5) {
      OTclClasses* sl = cl->super;
      OTclClasses* sc = 0;
      
      /*
       * reverse the list to obtain presentation order
       */ 
      
      Tcl_ResetResult(in);
      while (sc != sl) {
	OTclClasses* nl = sl;
	while (nl->next != sc) nl = nl->next;
	Tcl_AppendElement(in, Tcl_GetCommandName(in, nl->cl->object.id));
	sc = nl;
      }
    } else {
      OTclClass* isc = OTclGetClass(in, argv[5]);
      OTclClasses* pl;
      if (isc == 0) return OTclErrBadVal(in, "a class", argv[5]);
      pl = ComputePrecedence(cl);
      
      /*
       * search precedence to see if we're related or not
       */

      while (pl != 0) {
	if (pl->cl == isc) {
	  Tcl_SetResult(in, "1", TCL_STATIC);
	  break;
	}
	pl = pl->next;
      }
      if (pl == 0) Tcl_SetResult(in, "0", TCL_STATIC);
    }
  } else if (!strcmp(argv[4], "subclass")) {
    if (argc > 6) return OTclErrArgCnt(in,argv[0],"info subclass ?class?");
    if (argc == 5) {
      OTclClasses* sl = cl->sub;
      OTclClasses* sc = 0;
      
      /*
       * order unimportant
       */ 
      
      Tcl_ResetResult(in);
      for (sc = sl; sc != 0; sc = sc->next)
	Tcl_AppendElement(in, Tcl_GetCommandName(in, sc->cl->object.id));
    } else {
      OTclClass* isc = OTclGetClass(in, argv[5]);
      OTclClasses* pl;
      OTclClasses* saved;
      if (isc == 0) return OTclErrBadVal(in, "a class", argv[5]);
      saved = cl->order; cl->order = 0;
      pl = ComputeDependents(cl);
      
      /*
       * search precedence to see if we're related or not
       */

      while (pl != 0) {
	if (pl->cl == isc) {
	  Tcl_SetResult(in, "1", TCL_STATIC);
	  break;
	}
	pl = pl->next;
      }
      if (pl == 0) Tcl_SetResult(in, "0", TCL_STATIC);
      RC(cl->order); cl->order = saved;
    }
  } else if (!strcmp(argv[4], "heritage")) {
    OTclClasses* pl = ComputePrecedence(cl);
    CONST84 char* pattern = (argc == 6) ? argv[5] : 0;
    if (argc > 6) return OTclErrArgCnt(in,argv[0],"info heritage ?pat?");
    if (pl) pl = pl->next;
    Tcl_ResetResult(in);
    for (; pl != 0; pl = pl->next) {
      CONST84 char* name = (char *) Tcl_GetCommandName(in, pl->cl->object.id);
      if (pattern && !Tcl_StringMatch(name, pattern)) continue;
      Tcl_AppendElement(in, name);
    }
  } else if (!strcmp(argv[4], "instances")) {
    if (argc > 6) return OTclErrArgCnt(in,argv[0],"info instances ?pat?");
    ListInstanceKeys(in, &cl->instances, (argc == 6) ? argv[5] : 0);
  } else if (!strcmp(argv[4], "instcommands")) {
    if (argc > 6) return OTclErrArgCnt(in,argv[0],"info instcommands ?pat?");
    ListKeys(in, &cl->instprocs, (argc == 6) ? argv[5] : 0);
  } else if (!strcmp(argv[4], "instprocs")) {
    if (argc > 6) return OTclErrArgCnt(in,argv[0],"info instprocs ?pat?");
    ListProcKeys(in, &cl->instprocs, (argc == 6) ? argv[5] : 0);
  } else if (!strcmp(argv[4], "instargs")) {
    if (argc != 6) return OTclErrArgCnt(in,argv[0],"info instargs <instproc>");
    return ListProcArgs(in, &cl->instprocs, argv[5]);
  } else if (!strcmp(argv[4], "instdefault")) {
    if (argc != 8)
      return OTclErrArgCnt(in,argv[0],
			   "info instdefault <instproc> <arg> <var>");
    return ListProcDefault(in, &cl->instprocs, argv[5], argv[6], argv[7]);
  } else if (!strcmp(argv[4], "instbody")) {
    if (argc != 6) return OTclErrArgCnt(in,argv[0],"info instbody <instproc>");
    return ListProcBody(in, &cl->instprocs, argv[5]);
  } else {
    return OTclOInfoMethod(cd, in, argc, argv);
  }
  return TCL_OK;
}


static int
OTclCInstProcMethod(ClientData cd, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclClass* cl = OTclAsClass(in, cd);
  Tcl_CmdInfo proc;
  int op;

  if (!cl) return OTclErrType(in, argv[0], "Class");
  if (argc != 7) return OTclErrArgCnt(in,argv[0],"instproc name args body");
  
  /*
   * if the args list is "auto", the body is a script to load the proc
   */

  if (!strcmp("auto", argv[5])) op = MakeAuto(&proc, argv[6]);
  else if (argv[5][0]==0 && argv[6][0]==0) op = -1;
  else op = MakeProc(&proc,in, argc-3, argv+3);

  (void)RemoveMethod(&cl->instprocs, argv[4], (ClientData)cl);
  if (op == 1) AddMethod(&cl->instprocs, argv[4], proc.proc,
			 proc.clientData, proc.deleteProc, proc.deleteData);
  
  return (op != 0) ? TCL_OK : TCL_ERROR;
}

/*
 * C interface routines for manipulating objects and classes
 */


extern OTclObject*
OTclAsObject(Tcl_Interp* in, ClientData cd) {
  OTclObject* obj = (OTclObject*)cd;
  return IsType(obj, InObject(in)) ? obj : 0;
}


extern OTclClass*
OTclAsClass(Tcl_Interp* in, ClientData cd) {
  OTclClass* cl = (OTclClass*)cd;
  return IsType((OTclObject*)cl, InClass(in)) ? cl : 0;
}


extern OTclObject*
OTclGetObject(Tcl_Interp* in, CONST84 char* name) {
  Tcl_CmdInfo info;
  OTclObject* obj = 0;
  if (Tcl_GetCommandInfo(in, name, &info))
    if (info.proc == (Tcl_CmdProc *) OTclDispatch)
      obj = OTclAsObject(in, info.clientData);
  return obj;
}


extern OTclClass*
OTclGetClass(Tcl_Interp* in, CONST84 char* name) {
  Tcl_CmdInfo info;
  OTclClass* cl = 0;
  if (Tcl_GetCommandInfo(in, name, &info))
    if (info.proc == (Tcl_CmdProc *) OTclDispatch)
      cl = OTclAsClass(in, info.clientData);
  return cl;
}


extern OTclObject*
OTclCreateObject(Tcl_Interp* in, CONST84 char* name, OTclClass* cl) {
  CONST84 char* args[3];
  args[0] = (char *) Tcl_GetCommandName(in, cl->object.id);
  args[1] = "create";
  args[2] = name;
  if (OTclDispatch((ClientData)cl,in,3,args) != TCL_OK) return 0;
  return OTclGetObject(in, name);
}


extern OTclClass*
OTclCreateClass(Tcl_Interp* in, CONST84 char* name, OTclClass* cl){
  CONST84 char* args[3];
  args[0] = (char *) Tcl_GetCommandName(in, cl->object.id);
  args[1] = "create";
  args[2] = name;
  if (OTclDispatch((ClientData)cl,in,3,args) != TCL_OK) return 0;
  return OTclGetClass(in, name);
}


extern int
OTclDeleteObject(Tcl_Interp* in, OTclObject* obj) {
  CONST84 char* args[2];
  args[0] = (char *) Tcl_GetCommandName(in, obj->id);
  args[1] = "destroy";
  return OTclDispatch((ClientData)obj, in, 2, args);
}


extern int
OTclDeleteClass(Tcl_Interp* in, OTclClass* cl) {
  CONST84 char* args[2];
  args[0] = (char *) Tcl_GetCommandName(in, cl->object.id);
  args[1] = "destroy";
  return OTclDispatch((ClientData)cl, in, 2, args);
}


extern void
OTclAddPMethod(OTclObject* obj, char* nm, Tcl_CmdProc* proc,
	       ClientData cd, Tcl_CmdDeleteProc* dp)
{
  if (obj->procs)
    (void)RemoveMethod(obj->procs, nm, (ClientData)obj);
  else {
    obj->procs = (Tcl_HashTable*)ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(obj->procs, TCL_STRING_KEYS);
  }
  AddMethod(obj->procs, nm, proc, cd, dp, cd);
}


extern void
OTclAddIMethod(OTclClass* cl, char* nm, Tcl_CmdProc* proc,
	       ClientData cd, Tcl_CmdDeleteProc* dp)
{
  (void)RemoveMethod(&cl->instprocs, nm, (ClientData)cl);
  AddMethod(&cl->instprocs, nm, proc, cd, dp, cd);
}


extern int
OTclRemovePMethod(OTclObject* obj, char* nm) {
  if (obj->procs) return RemoveMethod(obj->procs, nm, (ClientData)obj);
  else return 0;
}


extern int
OTclRemoveIMethod(OTclClass* cl, char* nm) {
  return RemoveMethod(&cl->instprocs, nm, (ClientData)cl);
}


extern int
OTclNextMethod(OTclObject* obj, Tcl_Interp* in, int argc, CONST84 char*argv[]) {
  OTclClass* cl = 0;
  OTclClass* ncl;
  OTclClasses* pl;
  Tcl_CmdProc* proc = 0;
  ClientData cp = 0;
  CONST84 char* class = argv[2];
  int result = TCL_OK;

  if (class[0]){
    cl = OTclGetClass(in, class);
    if (!cl) return OTclErrBadVal(in, "a class", class);
  }  

  /*
   * if we are already in the precedence ordering, then advance
   * past our last point; otherwise (if cl==0) start from the start
   */

  pl = ComputePrecedence(obj->cl);
  while (pl && cl) {
    if (pl->cl == cl) cl = 0;
    pl = pl->next;
  }

  /*
   * search for a further class method and patch args before launching.
   * if no further method, return without error.
   */

  ncl = SearchCMethod(pl, argv[3], &proc, &cp, 0);  
  if (proc != 0) {
    cp = (cp != 0) ? cp : (ClientData)obj;
    argv[2] = (char *) Tcl_GetCommandName(in, ncl->object.id);
    result = (*proc)(cp, in, argc, (const char **) argv);
    argv[2] = class;
  }
  return result;
}


extern CONST84_RETURN char*
OTclSetInstVar(OTclObject* obj,Tcl_Interp* in, 
	       CONST84 char* name, CONST84 char* value,int flgs){
  Interp* iPtr = (Interp*)in;
  CallFrame* saved = iPtr->varFramePtr;
  CONST84 char* result;

  iPtr->varFramePtr = &obj->variables;
  result = (char *) Tcl_SetVar(in, name, value, flgs);
  iPtr->varFramePtr = saved;
  return result;
}


extern CONST84_RETURN char*
OTclGetInstVar(OTclObject* obj, Tcl_Interp* in, CONST84 char* name, int flgs){
  Interp* iPtr = (Interp*)in;
  CallFrame* saved = iPtr->varFramePtr;
  CONST84 char* result;

  iPtr->varFramePtr = &obj->variables;
  result = (char *) Tcl_GetVar(in, name, flgs);
  iPtr->varFramePtr = saved;
  return result;
}


extern int
OTclUnsetInstVar(OTclObject* obj, Tcl_Interp* in, CONST84 char* name, int flgs) {
  Interp* iPtr = (Interp*)in;
  CallFrame* saved = iPtr->varFramePtr;
  int result;

  iPtr->varFramePtr = &obj->variables;
  result = Tcl_UnsetVar(in, name, flgs);
  iPtr->varFramePtr = saved;
  return result;
}


extern void
OTclSetObjectData(OTclObject* obj, OTclClass* cl, ClientData data) {
  Tcl_HashEntry *hPtr;
  int nw;

  if (!cl->objectdata) {
    cl->objectdata = (Tcl_HashTable*)ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(cl->objectdata, TCL_ONE_WORD_KEYS);
  }
  hPtr = Tcl_CreateHashEntry(cl->objectdata, (char*)obj, &nw);
  Tcl_SetHashValue(hPtr, data);
}


extern int
OTclGetObjectData(OTclObject* obj, OTclClass* cl, ClientData* data) {
  Tcl_HashEntry *hPtr;

  if (!cl->objectdata) return 0;
  hPtr = Tcl_FindHashEntry(cl->objectdata, (char*)obj);
  if (data) *data = hPtr ? Tcl_GetHashValue(hPtr) : 0;
  return (hPtr != 0);
}


extern int
OTclUnsetObjectData(OTclObject* obj, OTclClass* cl) {
  Tcl_HashEntry *hPtr;

  if (!cl->objectdata) return 0;
  hPtr = Tcl_FindHashEntry(cl->objectdata, (char*)obj);
  if (hPtr) Tcl_DeleteHashEntry(hPtr);
  return (hPtr != 0);
}


/*
 * Tcl extension initialization routine
 */

#define MAXTCLPROC 4096

extern int
Otcl_Init(Tcl_Interp* in) {
  OTclClass* theobj = 0;
  OTclClass* thecls = 0;
  Tcl_HashEntry* hp1;
  Tcl_HashEntry* hp2;
  int nw1;
  int nw2;
  char tm[MAXTCLPROC];
#if TCL_MAJOR_VERSION >= 8
  Tcl_Namespace *namespacePtr;
#endif
  
  /*
   * discover Tcl's hidden proc interpreter
   */
  
  if (ProcInterpId == 0) {
    char* args[4];
#if TCL_MAJOR_VERSION >= 8
    int i;
    int res = 0;
    Tcl_Obj* objv[4];
#endif

    args[0]="proc"; args[1]="_fake_"; args[2]=""; args[3]="return";

#if TCL_MAJOR_VERSION < 8
    if (Tcl_ProcCmd(0, in, 4, args) == TCL_OK) {
      Tcl_CmdInfo info;
      if (Tcl_GetCommandInfo(in, args[1], &info)) {
	ProcInterpId = info.proc;
	(void)Tcl_DeleteCommand(in, args[1]);
      } else return OTclErrMsg(in, "proc failed", TCL_STATIC);
    } else return TCL_ERROR;
#else /*TCL_MAJOR_VERSION >= 8*/
    for (i = 0; i < 4; i++) {
      objv[i] = Tcl_NewStringObj(args[i], -1);
      Tcl_IncrRefCount(objv[i]);
    }
    if (Tcl_ProcObjCmd(0, in, 4, objv) == TCL_OK) {
      Tcl_CmdInfo info;
      if (Tcl_GetCommandInfo(in, args[1], &info)) {
	ProcInterpId = info.proc;
	(void)Tcl_DeleteCommand(in, args[1]);
      } else 
	res = 1;
    } else 
      res = 2;
    for (i = 0; i < 4; i++)
      Tcl_DecrRefCount(objv[i]);
    switch (res) {
    case 1: return OTclErrMsg(in, "proc failed", TCL_STATIC);
    case 2: return TCL_ERROR;
    }
#endif  /*TCL_MAJOR_VERSION >= 8*/
  }
  /*
   * bootstrap the tables of base objects and classes
   */
  
  if (theObjects == 0) {
    theObjects = (Tcl_HashTable*)ckalloc(sizeof(Tcl_HashTable));
    if (!theObjects) return OTclErrMsg(in, "Object table failed", TCL_STATIC);
    Tcl_InitHashTable(theObjects, TCL_ONE_WORD_KEYS);
  }

  if (theClasses == 0) {
    theClasses = (Tcl_HashTable*)ckalloc(sizeof(Tcl_HashTable));
    if (!theClasses) return OTclErrMsg(in, "Class table failed", TCL_STATIC);
    Tcl_InitHashTable(theClasses, TCL_ONE_WORD_KEYS);
  }
    
  hp1 = Tcl_CreateHashEntry(theObjects, (char*)in, &nw1);
  if (nw1) theobj = PrimitiveCCreate(in, "Object", 0);
  hp2 = Tcl_CreateHashEntry(theClasses, (char*)in, &nw2);
  if (nw2) thecls = PrimitiveCCreate(in, "Class", 0);

  if (!nw1 && !nw2) {
    Tcl_SetResult(in, "0", TCL_STATIC);
    return TCL_OK;
  } else if (!theobj || !thecls) {
    if (theobj) PrimitiveCDestroy((ClientData)theobj);
    if (thecls) PrimitiveCDestroy((ClientData)thecls);
    return OTclErrMsg(in, "Object/Class failed", TCL_STATIC);
  }

  Tcl_SetHashValue(hp1, (char*)theobj);
  Tcl_SetHashValue(hp2, (char*)thecls);
    
  theobj->object.type = thecls;
  theobj->parent = 0;
  thecls->object.type = thecls;
  thecls->parent = theobj;

  AddInstance((OTclObject*)theobj, thecls);
  AddInstance((OTclObject*)thecls, thecls);
  AddSuper(thecls, theobj);

#if TCL_MAJOR_VERSION >= 8
  /* create the otcl namespace of otcl instprocs and procs */
  namespacePtr = Tcl_CreateNamespace(in, "otcl", (ClientData) NULL,
				     (Tcl_NamespaceDeleteProc *) NULL);  
  if (namespacePtr==NULL)
    return OTclErrMsg(in, "creation of name space failed", TCL_STATIC);
#endif
  
  /*
   * and fill them with functionality
   */
  
  OTclAddPMethod((OTclObject*)theobj, "alloc", (Tcl_CmdProc *) OTclOAllocMethod, 0, 0);
  OTclAddIMethod(theobj, "init", (Tcl_CmdProc *) OTclOInitMethod, 0, 0);
  OTclAddIMethod(theobj, "destroy", (Tcl_CmdProc *) OTclODestroyMethod, 0, 0);
  OTclAddIMethod(theobj, "class", (Tcl_CmdProc *) OTclOClassMethod, 0, 0);
  OTclAddIMethod(theobj, "info", (Tcl_CmdProc *) OTclOInfoMethod, 0, 0);
  OTclAddIMethod(theobj, "proc", (Tcl_CmdProc *) OTclOProcMethod, 0, 0);
  OTclAddIMethod(theobj, "next", (Tcl_CmdProc *) OTclONextMethod, 0, 0);
  OTclAddIMethod(theobj, "set", (Tcl_CmdProc *) OTclOSetMethod, 0, 0);
  OTclAddIMethod(theobj, "unset", (Tcl_CmdProc *) OTclOUnsetMethod, 0, 0);
  OTclAddIMethod(theobj, "instvar", (Tcl_CmdProc *) OTclOInstVarMethod, 0, 0);

  OTclAddPMethod((OTclObject*)thecls, "alloc", (Tcl_CmdProc *) OTclCAllocMethod, 0, 0);
  OTclAddIMethod(thecls, "create", (Tcl_CmdProc *) OTclCCreateMethod, 0, 0);
  OTclAddIMethod(thecls, "superclass", (Tcl_CmdProc *) OTclCSuperClassMethod, 0, 0);
  OTclAddIMethod(thecls, "info", (Tcl_CmdProc *) OTclCInfoMethod, 0, 0);
  OTclAddIMethod(thecls, "instproc", (Tcl_CmdProc *) OTclCInstProcMethod, 0, 0);

  /*
   * with some methods and library procs in tcl - they could go in a
   * otcl.tcl file, but they're embedded here with Tcl_Eval to avoid
   * the need to carry around a separate library.
   */

  (void)strcpy(tm, "Object instproc array {opt ary args} {             \n");
  (void)strcat(tm, "  $self instvar $ary                               \n");
  (void)strcat(tm, "  eval array [list $opt] [list $ary] $args         \n");
  (void)strcat(tm, "}                                                  \n");
  if (Tcl_Eval(in, tm) != TCL_OK) return TCL_ERROR;
  
  (void)strcpy(tm, "Class instproc unknown {m args} {                  \n");
  (void)strcat(tm, "  if {$m == {create}} then {                       \n");
  (void)strcat(tm, "    error \"$self: unable to dispatch $m\"         \n");
  (void)strcat(tm, "  }                                                \n");
  (void)strcat(tm, "  eval [list $self] create [list $m] $args         \n");
  (void)strcat(tm, "}                                                  \n");
  if (Tcl_Eval(in, tm) != TCL_OK) return TCL_ERROR;
  
  (void)strcpy(tm, "proc otcl_load {obj file} {                        \n");
  (void)strcat(tm, "   global auto_index                               \n");
  (void)strcat(tm, "   source $file                                    \n");
  (void)strcat(tm, "   foreach i [array names auto_index             \\\n");
  (void)strcat(tm, "       [list $obj *proc *]] {                      \n");
  (void)strcat(tm, "     set type [lindex $i 1]                        \n");
  (void)strcat(tm, "     set meth [lindex $i 2]                        \n");
  (void)strcat(tm, "     if {[$obj info ${type}s $meth] == {}} then {  \n");
  (void)strcat(tm, "       $obj $type $meth {auto} $auto_index($i)     \n");
  (void)strcat(tm, "     }                                             \n");
  (void)strcat(tm, "   }                                               \n");
  (void)strcat(tm, " }                                                 \n");
  if (Tcl_Eval(in, tm) != TCL_OK) return TCL_ERROR;

  (void)strcpy(tm, "proc otcl_mkindex {meta dir args} {                \n");
  (void)strcat(tm, "  set sp {[ 	]+}                            \n");
  (void)strcat(tm, "  set st {^[ 	]*}                            \n");
  (void)strcat(tm, "  set wd {([^ 	]+)}                           \n");
  (void)strcat(tm, "  foreach creator $meta {                          \n");
  (void)strcat(tm, "    lappend cp \"$st$creator${sp}create$sp$wd\"    \n");
  (void)strcat(tm, "    lappend ap \"$st$creator$sp$wd\"               \n");
  (void)strcat(tm, "  }                                                \n");
  (void)strcat(tm, "  foreach method {proc instproc} {                 \n");
  (void)strcat(tm, "    lappend mp \"$st$wd${sp}($method)$sp$wd\"      \n");
  (void)strcat(tm, "  }                                                \n");
  (void)strcat(tm, "  foreach cl [concat Class [Class info heritage]] {\n");
  (void)strcat(tm, "    eval lappend meths [$cl info instcommands]     \n");
  (void)strcat(tm, "  }                                                \n");
  (void)strcat(tm, "  set old [pwd]                                    \n");
  (void)strcat(tm, "  cd $dir                                          \n");
  (void)strcat(tm, "  append idx \"# Tcl autoload index file, \"       \n");
  (void)strcat(tm, "  append idx \"version 2.0\\n\"                    \n");
  (void)strcat(tm, "  append idx \"# otcl additions generated with \"  \n");
  (void)strcat(tm, "  append idx \"\\\"otcl_mkindex [list $meta] \"    \n");
  (void)strcat(tm, "  append idx \"[list $dir] $args\\\"\\n\"          \n");
  (void)strcat(tm, "  set oc 0                                         \n");
  (void)strcat(tm, "  set mc 0                                         \n");
  (void)strcat(tm, "  foreach file [eval glob -nocomplain -- $args] {  \n");
  (void)strcat(tm, "    if {[catch {set f [open $file]} msg]} then {   \n");
  (void)strcat(tm, "      catch {close $f}                             \n");
  (void)strcat(tm, "      cd $old                                      \n");
  (void)strcat(tm, "      error $msg                                   \n");
  (void)strcat(tm, "    }                                              \n");
  (void)strcat(tm, "    while {[gets $f line] >= 0} {                  \n");
  (void)strcat(tm, "      foreach c $cp {                              \n");
  (void)strcat(tm, "	    if {[regexp $c $line x obj]==1 &&          \n");
  (void)strcat(tm, "	        [string index $obj 0]!={$}} then {     \n");
  (void)strcat(tm, "	      incr oc                                  \n");
  (void)strcat(tm, "	      append idx \"set auto_index($obj) \"     \n");
  (void)strcat(tm, "	      append idx \"\\\"otcl_load $obj \"       \n");
  (void)strcat(tm, "          append idx \"\\$dir/$file\\\"\\n\"       \n");
  (void)strcat(tm, "	    }                                          \n");
  (void)strcat(tm, "	  }                                            \n");
  (void)strcat(tm, "      foreach a $ap {                              \n");
  (void)strcat(tm, "	    if {[regexp $a $line x obj]==1 &&          \n");
  (void)strcat(tm, "	        [string index $obj 0]!={$} &&          \n");
  (void)strcat(tm, "	        [lsearch -exact $meths $obj]==-1} {    \n");
  (void)strcat(tm, "	      incr oc                                  \n");
  (void)strcat(tm, "	      append idx \"set auto_index($obj) \"     \n");
  (void)strcat(tm, "	      append idx \"\\\"otcl_load $obj \"       \n");
  (void)strcat(tm, "          append idx \"\\$dir/$file\\\"\\n\"       \n");
  (void)strcat(tm, "	    }                                          \n");
  (void)strcat(tm, "	  }                                            \n");
  (void)strcat(tm, "      foreach m $mp {                              \n");
  (void)strcat(tm, "	    if {[regexp $m $line x obj ty pr]==1 &&    \n");
  (void)strcat(tm, "	        [string index $obj 0]!={$} &&          \n");
  (void)strcat(tm, "	        [string index $pr 0]!={$}} then {      \n");
  (void)strcat(tm, "	        incr mc                                \n");
  (void)strcat(tm, "	        append idx \"set \\{auto_index($obj \" \n");
  (void)strcat(tm, "	        append idx \"$ty $pr)\\} \\\"source \" \n");
  (void)strcat(tm, "	        append idx \"\\$dir/$file\\\"\\n\"     \n");
  (void)strcat(tm, "	    }                                          \n");
  (void)strcat(tm, "      }                                            \n");
  (void)strcat(tm, "    }                                              \n");
  (void)strcat(tm, "    close $f                                       \n");
  (void)strcat(tm, "  }                                                \n");
  (void)strcat(tm, "  set t [open tclIndex a+]                         \n");
  (void)strcat(tm, "  puts $t $idx nonewline                           \n");
  (void)strcat(tm, "  close $t                                         \n");
  (void)strcat(tm, "  cd $old                                          \n");
  (void)strcat(tm, "  return \"$oc objects, $mc methods\"              \n");
  (void)strcat(tm, "}                                                  \n");
  if (Tcl_Eval(in, tm) != TCL_OK) return TCL_ERROR;

  Tcl_SetResult(in, "1", TCL_STATIC);
  return TCL_OK;
}

/*
 * Otcl strangness:  why isn't c listed?
 *    dash> otclsh
 *    % Class Foo
 *    Foo
 *    % Foo instproc a a {}
 *    % Foo instproc b {} { }
 *    % Foo instproc c {} {}
 *    % Foo info instprocs
 *    a b
 * -johnh, 30-Jun-98
 */
