
#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: pbvec.c,v 1.85 1997/08/13 22:22:33 bsmith Exp bsmith $";
#endif

/*
   This file contains routines for Parallel vector operations.
 */

#include "petsc.h"
#include <math.h>
#include "pvecimpl.h"   /*I  "vec.h"   I*/

#undef __FUNC__  
#define __FUNC__ "VecDot_MPI"
int VecDot_MPI( Vec xin, Vec yin, Scalar *z )
{
  Scalar    sum, work;
  int       ierr;

  ierr = VecDot_Seq(  xin, yin, &work ); CHKERRQ(ierr);
/*
   This is a ugly hack. But to do it right is kind of silly.
*/
  PLogEventBarrierBegin(VEC_DotBarrier,0,0,0,0,xin->comm);
#if defined(PETSC_COMPLEX)
  MPI_Allreduce(&work,&sum,2,MPI_DOUBLE,MPI_SUM,xin->comm);
#else
  MPI_Allreduce(&work,&sum,1,MPI_DOUBLE,MPI_SUM,xin->comm);
#endif
  PLogEventBarrierEnd(VEC_DotBarrier,0,0,0,0,xin->comm);
  *z = sum;
  return 0;
}

#undef __FUNC__  
#define __FUNC__ "VecTDot_MPI"
int VecTDot_MPI( Vec xin, Vec yin, Scalar *z )
{
  Scalar    sum, work;
  VecTDot_Seq(  xin, yin, &work );
/*
   This is a ugly hack. But to do it right is kind of silly.
*/
  PLogEventBarrierBegin(VEC_DotBarrier,0,0,0,0,xin->comm);
#if defined(PETSC_COMPLEX)
  MPI_Allreduce(&work, &sum,2,MPI_DOUBLE,MPI_SUM,xin->comm );
#else
  MPI_Allreduce(&work, &sum,1,MPI_DOUBLE,MPI_SUM,xin->comm );
#endif
  PLogEventBarrierEnd(VEC_DotBarrier,0,0,0,0,xin->comm);
  *z = sum;
  return 0;
}

#undef __FUNC__  
#define __FUNC__ "VecSetOption_MPI"
int VecSetOption_MPI(Vec v,VecOption op)
{
  Vec_MPI *w = (Vec_MPI *) v->data;

  if (op == VEC_IGNORE_OFF_PROCESSOR_ENTRIES) {
    w->stash.donotstash = 1;
  }
  return 0;
}
    
int VecDuplicate_MPI(Vec,Vec *);

static struct _VeOps DvOps = { VecDuplicate_MPI, 
            VecDuplicateVecs_Default, VecDestroyVecs_Default, VecDot_MPI, 
            VecMDot_MPI,
            VecNorm_MPI, VecTDot_MPI, 
            VecMTDot_MPI,
            VecScale_Seq, VecCopy_Seq,
            VecSet_Seq, VecSwap_Seq, VecAXPY_Seq, VecAXPBY_Seq,
            VecMAXPY_Seq, VecAYPX_Seq,
            VecWAXPY_Seq, VecPointwiseMult_Seq,
            VecPointwiseDivide_Seq, 
            VecSetValues_MPI,
            VecAssemblyBegin_MPI,VecAssemblyEnd_MPI,
            VecGetArray_Seq,VecGetSize_MPI,VecGetSize_Seq,
            VecGetOwnershipRange_MPI,0,VecMax_MPI,VecMin_MPI,
            VecSetRandom_Seq,
            VecSetOption_MPI};

#undef __FUNC__  
#define __FUNC__ "VecCreateMPI_Private"
/*
    VecCreateMPI_Private - Basic create routine called by VecCreateMPI(), VecCreateGhost()
  and VecDuplicate_MPI() to reduce code duplication.
*/
static int VecCreateMPI_Private(MPI_Comm comm,int n,int nghost,int N,int size,int rank,int *owners,Vec *vv)
{
  Vec     v;
  Vec_MPI *s;
  int     mem,i;
  *vv = 0;

  mem           = sizeof(Vec_MPI)+(size+1)*sizeof(int);
  PetscHeaderCreate(v,_p_Vec,VEC_COOKIE,VECMPI,comm,VecDestroy,VecView);
  PLogObjectCreate(v);
  PLogObjectMemory(v,mem + sizeof(struct _p_Vec) + (nghost+1)*sizeof(Scalar));
  s              = (Vec_MPI *) PetscMalloc(mem); CHKPTRQ(s);
  PetscMemcpy(&v->ops,&DvOps,sizeof(DvOps));
  v->data        = (void *) s;
  v->destroy     = VecDestroy_MPI;
  v->view        = VecView_MPI;
  s->n           = n;
  s->nghost      = nghost;
  s->N           = N;
  v->n           = n;
  v->N           = N;
  v->mapping     = 0;
  s->size        = size;
  s->rank        = rank;
  s->array       = (Scalar *) PetscMalloc((nghost+1)*sizeof(Scalar));CHKPTRQ(s->array);
  s->array_allocated = s->array;

  PetscMemzero(s->array,n*sizeof(Scalar));
  s->ownership   = (int *) (s + 1);
  s->insertmode  = NOT_SET_VALUES;
  if (owners) {
    PetscMemcpy(s->ownership,owners,(size+1)*sizeof(int));
  }
  else {
    MPI_Allgather(&n,1,MPI_INT,s->ownership+1,1,MPI_INT,comm);
    s->ownership[0] = 0;
    for (i=2; i<=size; i++ ) {
      s->ownership[i] += s->ownership[i-1];
    }
  }
  s->stash.donotstash = 0;
  s->stash.nmax       = 10;
  s->stash.n          = 0;
  s->stash.array      = (Scalar *) PetscMalloc(10*(sizeof(Scalar)+sizeof(int)));CHKPTRQ(s->stash.array);
  PLogObjectMemory(v,10*sizeof(Scalar) + 10 *sizeof(int));
  s->stash.idx = (int *) (s->stash.array + 10);
  *vv = v;
  return 0;
}

#undef __FUNC__  
#define __FUNC__ "VecCreateMPI"
/*@C
   VecCreateMPI - Creates a parallel vector.

   Input Parameters:
.  comm - the MPI communicator to use
.  n - local vector length (or PETSC_DECIDE to have calculated if N is given)
.  N - global vector length (or PETSC_DECIDE to have calculated if n is given)

   Output Parameter:
.  vv - the vector
 
   Notes:
   Use VecDuplicate() or VecDuplicateVecs() to form additional vectors of the
   same type as an existing vector.

.keywords: vector, create, MPI

.seealso: VecCreateSeq(), VecCreate(), VecDuplicate(), VecDuplicateVecs(), VecCreateGhost()
@*/ 
int VecCreateMPI(MPI_Comm comm,int n,int N,Vec *vv)
{
  int sum, work = n, size, rank;
  *vv = 0;

  MPI_Comm_size(comm,&size);
  MPI_Comm_rank(comm,&rank); 
  if (N == PETSC_DECIDE) { 
    MPI_Allreduce( &work, &sum,1,MPI_INT,MPI_SUM,comm );
    N = sum;
  }
  if (n == PETSC_DECIDE) { 
    n = N/size + ((N % size) > rank);
  }
  return VecCreateMPI_Private(comm,n,n,N,size,rank,0,vv);
}

/*@C
   VecCreateGhost - Creates a parallel vector with ghost padding on each processor.

   Input Parameters:
.  comm - the MPI communicator to use
.  n - local vector length 
.  nghost - local vector length including ghost points
.  N - global vector length (or PETSC_DECIDE to have calculated if n is given)

   Output Parameter:
.  lv - the local vector representation (with ghost points as part of vector)
.  vv - the global vector representation (without ghost points as part of vector)
 
   Notes:
   The two vectors returned share the same array storage space.
   Use VecDuplicate() or VecDuplicateVecs() to form additional vectors of the
   same type as an existing vector. 

.keywords: vector, create, MPI, ghost points, ghost padding

.seealso: VecCreateSeq(), VecCreate(), VecDuplicate(), VecDuplicateVecs(), VecCreateMPI()
@*/ 
int VecCreateGhost(MPI_Comm comm,int n,int nghost,int N,Vec *lv,Vec *vv)
{
  int    sum, work = n, size, rank, ierr;
  Scalar *array;

  *vv = 0;

  if (n == PETSC_DECIDE) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,1,"Must set local size");
  if (nghost == PETSC_DECIDE) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,1,"Must set local ghost size");
  if (nghost < n) SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,1,"Ghost padded length must be no shorter then length");

  MPI_Comm_size(comm,&size);
  MPI_Comm_rank(comm,&rank); 
  if (N == PETSC_DECIDE) { 
    MPI_Allreduce( &work, &sum,1,MPI_INT,MPI_SUM,comm );
    N = sum;
  }
  ierr = VecCreateMPI_Private(comm,n,nghost,N,size,rank,0,vv); CHKERRQ(ierr);
  ierr = VecGetArray(*vv,&array); CHKERRQ(ierr);
  ierr = VecCreateSeqWithArray(PETSC_COMM_SELF,nghost,array,lv); CHKERRQ(ierr);
  ierr = VecRestoreArray(*vv,&array); CHKERRQ(ierr);
  return 0;
}

#undef __FUNC__  
#define __FUNC__ "VecDuplicate_MPI"
int VecDuplicate_MPI( Vec win, Vec *v)
{
  int     ierr;
  Vec_MPI *vw, *w = (Vec_MPI *)win->data;

  ierr = VecCreateMPI_Private(win->comm,w->n,w->nghost,w->N,w->size,w->rank,w->ownership,v);CHKERRQ(ierr);

  /* New vector should inherit stashing property of parent */
  vw                   = (Vec_MPI *)(*v)->data;
  vw->stash.donotstash = w->stash.donotstash;
  
  (*v)->childcopy    = win->childcopy;
  (*v)->childdestroy = win->childdestroy;
  if (win->mapping) {
    (*v)->mapping = win->mapping;
    PetscObjectReference((PetscObject)win->mapping);
  }
  if (win->child) return (*win->childcopy)(win->child,&(*v)->child);
  return 0;
}


