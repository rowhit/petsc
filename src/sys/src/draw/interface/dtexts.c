#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: dtexts.c,v 1.13 1997/07/09 20:57:34 balay Exp bsmith $";
#endif
/*
       Provides the calling sequences for all the basic Draw routines.
*/
#include "src/draw/drawimpl.h"  /*I "draw.h" I*/

#undef __FUNC__  
#define __FUNC__ "DrawStringSetSize" 
/*@
   DrawStringSetSize - Sets the size for charactor text.  The width is
   relative to the user coordinates of the window; 0.0 denotes the natural
   width; 1.0 denotes the entire viewport. 

   Input Parameters:
.  draw - the drawing context
.  width - the width in user coordinates
.  height - the charactor height

  Note:
  Only a limited range of sizes are available.

.keywords: draw, text, set, size
@*/
int DrawStringSetSize(Draw draw,double width,double height)
{
  PetscValidHeaderSpecific(draw,DRAW_COOKIE);
  if (draw->type == DRAW_NULLWINDOW) return 0;
  return (*draw->ops.textsetsize)(draw,width,height);
}
