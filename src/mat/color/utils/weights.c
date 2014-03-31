#include <petsc-private/matimpl.h>      /*I "petscmat.h"  I*/
#include <../src/mat/impls/aij/seq/aij.h>

#undef __FUNCT__
#define __FUNCT__ "MatColoringCreateNaturalWeights"
PetscErrorCode MatColoringCreateNaturalWeights(MatColoring mc,Mat G,PetscReal **weights,PetscInt **lperm)
{
  PetscErrorCode ierr;
  PetscInt       i,s,e,n;
  PetscRandom    rand;
  PetscReal      r;

  PetscFunctionBegin;
  /* each weight should be the degree plus a random perturbation */
  ierr = PetscRandomCreate(PetscObjectComm((PetscObject)mc),&rand);CHKERRQ(ierr);
  ierr = PetscRandomSetFromOptions(rand);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(G,&s,&e);CHKERRQ(ierr);
  n=e-s;
  ierr = PetscMalloc1(n,weights);CHKERRQ(ierr);
  ierr = PetscMalloc1(n,lperm);CHKERRQ(ierr);
  for (i=s;i<e;i++) {
    ierr = PetscRandomGetValueReal(rand,&r);CHKERRQ(ierr);
    *weights[i-s] = i;
    *lperm[i-s] = i-s;
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatColoringCreateRandomWeights"
PetscErrorCode MatColoringCreateRandomWeights(MatColoring mc,Mat G,PetscReal **weights,PetscInt **lperm)
{
  PetscErrorCode ierr;
  PetscInt       i,s,e,n;
  PetscRandom    rand;
  PetscReal      r;

  PetscFunctionBegin;
  /* each weight should be the degree plus a random perturbation */
  ierr = PetscRandomCreate(PetscObjectComm((PetscObject)mc),&rand);CHKERRQ(ierr);
  ierr = PetscRandomSetFromOptions(rand);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(G,&s,&e);CHKERRQ(ierr);
  n=e-s;
  ierr = PetscMalloc1(n,weights);CHKERRQ(ierr);
  ierr = PetscMalloc1(n,lperm);CHKERRQ(ierr);
  for (i=s;i<e;i++) {
    ierr = PetscRandomGetValueReal(rand,&r);CHKERRQ(ierr);
    *weights[i-s] = PetscAbsReal(r);
    *lperm[i-s] = i-s;
  }
  ierr = PetscSortRealWithPermutation(n,*weights,*lperm);CHKERRQ(ierr);
  ierr = PetscRandomDestroy(&rand);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatColoringGetDegrees"
PetscErrorCode MatColoringGetDegrees(Mat G,PetscInt distance,PetscInt *degrees)
{
  PetscInt       j,i,s,e,n,ln,lm,degree,bidx,idx,dist;
  Mat            lG,*lGs;
  IS             ris;
  PetscErrorCode ierr;
  PetscInt       *seen;
  const PetscInt *gidx;
  PetscInt       *idxbuf;
  PetscInt       *distbuf;
  PetscInt       ncols;
  PetscInt       *coltorow;
  const PetscInt *cols;
  PetscBool      isSEQAIJ;
  Mat_SeqAIJ     *aij;
  PetscInt       *Gi,*Gj;

  PetscFunctionBegin;
  ierr = MatGetOwnershipRange(G,&s,&e);CHKERRQ(ierr);
  n=e-s;
  ierr = ISCreateStride(PetscObjectComm((PetscObject)G),n,s,1,&ris);CHKERRQ(ierr);
  ierr = MatIncreaseOverlap(G,1,&ris,distance);CHKERRQ(ierr);
  ierr = ISSort(ris);CHKERRQ(ierr);
  ierr = MatGetSubMatrices(G,1,&ris,&ris,MAT_INITIAL_MATRIX,&lGs);CHKERRQ(ierr);
  lG = lGs[0];
  ierr = PetscObjectTypeCompare((PetscObject)lG,MATSEQAIJ,&isSEQAIJ);CHKERRQ(ierr);
  if (!isSEQAIJ) {
    SETERRQ(PetscObjectComm((PetscObject)G),PETSC_ERR_ARG_WRONGSTATE,"MatColoringDegrees requires an MPI/SEQAIJ Matrix");
  }
  ierr = MatGetSize(lG,&ln,&lm);CHKERRQ(ierr);
  aij = (Mat_SeqAIJ*)lG->data;
  Gi = aij->i;
  Gj = aij->j;
  ierr = PetscMalloc4(lm,&seen,lm,&idxbuf,lm,&distbuf,lm,&coltorow);CHKERRQ(ierr);
  for (i=0;i<ln;i++) {
    seen[i]=-1;
  }
  ierr = ISGetIndices(ris,&gidx);CHKERRQ(ierr);
  for (i=0;i<ln;i++) {
    if (gidx[i] >= e || gidx[i] < s) continue;
    bidx=-1;
    ncols = Gi[i+1]-Gi[i];
    cols = &(Gj[Gi[i]]);
    degree = 0;
    /* place the distance-one neighbors on the queue */
    for (j=0;j<ncols;j++) {
      bidx++;
      seen[cols[j]] = i;
      distbuf[bidx] = 1;
      idxbuf[bidx] = cols[j];
    }
    while (bidx >= 0) {
      /* pop */
      idx = idxbuf[bidx];
      dist = distbuf[bidx];
      bidx--;
      degree++;
      if (dist < distance) {
        ncols = Gi[idx+1]-Gi[idx];
        cols = &(Gj[Gi[idx]]);
        for (j=0;j<ncols;j++) {
          if (seen[cols[j]] != i) {
            bidx++;
            seen[cols[j]] = i;
            idxbuf[bidx] = cols[j];
            distbuf[bidx] = dist+1;
          }
        }
      }
    }
    degrees[gidx[i]-s] = degree;
  }
  ierr = ISRestoreIndices(ris,&gidx);CHKERRQ(ierr);
  ierr = ISDestroy(&ris);CHKERRQ(ierr);
  ierr = PetscFree4(seen,idxbuf,distbuf,coltorow);CHKERRQ(ierr);
  ierr = MatDestroyMatrices(1,&lGs);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatColoringCreateLargestFirstWeights"
PetscErrorCode MatColoringSetLargestFirstWeights(MatColoring mc,Mat G,PetscReal **weights,PetscInt **lperm)
{
  PetscErrorCode ierr;
  PetscInt       i,s,e,n,ncols;
  PetscRandom    rand;
  PetscReal      r,swp;
  PetscInt       *degrees;

  PetscFunctionBegin;
  /* each weight should be the degree plus a random perturbation */
  ierr = PetscRandomCreate(PetscObjectComm((PetscObject)mc),&rand);CHKERRQ(ierr);
  ierr = PetscRandomSetFromOptions(rand);CHKERRQ(ierr);
  ierr = MatGetOwnershipRange(G,&s,&e);CHKERRQ(ierr);
  n=e-s;
  ierr = PetscMalloc1(n,&degrees);CHKERRQ(ierr);
  ierr = MatColoringGetDegrees(G,mc->dist,degrees);CHKERRQ(ierr);
  for (i=s;i<e;i++) {
    ierr = MatGetRow(G,i,&ncols,NULL,NULL);CHKERRQ(ierr);
    ierr = PetscRandomGetValueReal(rand,&r);CHKERRQ(ierr);
    (*weights)[i-s] = ncols + PetscAbsReal(r);
    (*lperm)[i-s] = i-s;
    ierr = MatRestoreRow(G,i,&ncols,NULL,NULL);CHKERRQ(ierr);
  }
  ierr = PetscSortRealWithPermutation(n,*weights,*lperm);CHKERRQ(ierr);
  for (i=0;i<n/2;i++) {
    swp = (*lperm)[i];
    (*lperm)[i] = (*lperm)[n-i-1];
    (*lperm)[n-i-1] = swp;
  }
  ierr = PetscRandomDestroy(&rand);CHKERRQ(ierr);
  ierr = PetscFree(degrees);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
