#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: gcookie.c,v 1.8 1997/07/09 20:51:14 balay Exp bsmith $";
#endif
/*
     Provides utility routines for manulating any type of PETSc object.
*/
#include "petsc.h"  /*I   "petsc.h"    I*/

#undef __FUNC__  
#define __FUNC__ "PetscObjectGetCookie"
/*@C
   PetscObjectGetCookie - Gets the cookie for any PetscObject, 

   Input Parameter:
.  obj - any PETSc object, for example a Vec, Mat or KSP.

   Output Parameter:
.  cookie - the cookie

.keywords: object, get, cookie
@*/
int PetscObjectGetCookie(PetscObject obj,int *cookie)
{
  if (!obj) SETERRQ(1,0,"Null object");
  *cookie = obj->cookie;
  return 0;
}

#undef __FUNC__  
#define __FUNC__ "PetscObjectExists"
/*@
   PetscObjectExists - Determines whether a PETSc object has been destroyed.

   Input Parameter:
.  obj - any PETSc object, for example a Vec, Mat or KSP.

   Output Parameter:
.  exists - 0 if object does not exist; 1 if object does exist.

.keywords: object, exists
@*/
int PetscObjectExists(PetscObject obj,int *exists)
{
  *exists = 0;
  if (!obj) return 0;
  if (obj->cookie != PETSCFREEDHEADER) *exists = 1;
  return 0;
}

