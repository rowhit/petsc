#ifndef lint
static char vcid[] = "$Id: preonly.c,v 1.4 1994/12/23 20:25:49 bsmith Exp bsmith $";
#endif

/*                       
       This implements a stub method that applies ONLY the preconditioner.
       This may be used in inner iterations, where it is desired to 
       allow multiple iterations as well as the "0-iteration" case
       
*/
#include <stdio.h>
#include <math.h>
#include "petsc.h"
#include "kspimpl.h"

static int KSPiPREONLYSetUp(KSP itP)
{
 return KSPCheckDef( itP );
}

static int  KSPiPREONLYSolve(KSP itP,int *its)
{
Vec      X,B;
X        = itP->vec_sol;
B        = itP->vec_rhs;
PCApply(itP->B,B,X);

itP->nmatop   += 1;
itP->nvectors += 0;
*its = 1;
return 0;
}

int KSPiPREONLYCreate(KSP itP)
{
itP->MethodPrivate        = (void *) 0;
itP->method               = KSPPREONLY;
itP->setup                = KSPiPREONLYSetUp;
itP->solver               = KSPiPREONLYSolve;
itP->adjustwork           = 0;
itP->destroy              = KSPiDefaultDestroy;
  itP->converged            = KSPDefaultConverged;
  itP->BuildSolution        = KSPDefaultBuildSolution;
  itP->BuildResidual        = KSPDefaultBuildResidual;
return 0;
}
