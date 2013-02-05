static char help[] = "Time-dependent PDE in 2d for calculating joint PDF. \n";
/*
   p_t = -x_t*p_x -y_t*p_y + f(t)*p_yy
   xmin < x < xmax, ymin < y < ymax;

   Boundary conditions Neumman using mirror values

   Note that x_t and y_t in the above are given functions of x and y; they are not derivatives of x and y. 
   x_t = (y - ws)  y_t = (ws/2H)*(Pm - Pmax*sin(x))

*/

#include <petscdmda.h>
#include <petscts.h>

static const char *const BoundaryTypes[] = {"NONE","GHOSTED","MIRROR","PERIODIC","DMDABoundaryType","DMDA_BOUNDARY_",0};
/*
   User-defined data structures and routines
*/
typedef struct {
  PetscScalar ws;   /* Synchronous speed */
  PetscScalar H;    /* Inertia constant */
  PetscScalar D;    /* Damping constant */
  PetscScalar Pmax; /* Maximum power output of generator */
  PetscScalar PM_min; /* Mean mechanical power input */
  PetscScalar lambda; /* correlation time */
  PetscScalar q;      /* noise strength */
  PetscScalar mux;    /* Initial average angle */
  PetscScalar sigmax; /* Standard deviation of initial angle */
  PetscScalar muy;    /* Average speed */
  PetscScalar sigmay; /* standard deviation of initial speed */
  PetscScalar rho;    /* Cross-correlation coefficient */
  PetscScalar xmin;   /* left boundary of angle */
  PetscScalar xmax;   /* right boundary of angle */
  PetscScalar ymin;   /* bottom boundary of speed */
  PetscScalar ymax;   /* top boundary of speed */
  PetscScalar dx;     /* x step size */
  PetscScalar dy;     /* y step size */
  PetscScalar disper_coe; /* Dispersion coefficient */
  DM          da;
  PetscInt    st_width; /* Stencil width */
  DMDABoundaryType bx; /* x boundary type */
  DMDABoundaryType by; /* y boundary type */
} AppCtx;

PetscErrorCode Parameter_settings(AppCtx*);
PetscErrorCode ini_bou(Vec,AppCtx*);
PetscErrorCode IFunction(TS,PetscReal,Vec,Vec,Vec,void*);
PetscErrorCode IJacobian(TS,PetscReal,Vec,Vec,PetscReal,Mat*,Mat*,MatStructure*,void*);
PetscErrorCode PostStep(TS);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc, char **argv)
{
  PetscErrorCode ierr;
  Vec            x;  /* Solution vector */
  TS             ts;   /* Time-stepping context */
  AppCtx         user; /* Application context */
  PetscViewer    viewer;

  PetscInitialize(&argc,&argv,"petscopt_ex7", help);

  /* Get physics and time parameters */
  ierr = Parameter_settings(&user);CHKERRQ(ierr);
  /* Create a 2D DA with dof = 1 */
  ierr = DMDACreate2d(PETSC_COMM_WORLD,user.bx,user.by,DMDA_STENCIL_STAR,-4,-4,PETSC_DECIDE,PETSC_DECIDE,1,user.st_width,PETSC_NULL,PETSC_NULL,&user.da);CHKERRQ(ierr);
  /* Set x and y coordinates */
  ierr = DMDASetUniformCoordinates(user.da,user.xmin,user.xmax,user.ymin,user.ymax,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  ierr = DMDASetCoordinateName(user.da,0,"X - the angle");
  ierr = DMDASetCoordinateName(user.da,1,"Y - the speed");

  /* Get global vector x from DM  */
  ierr = DMCreateGlobalVector(user.da,&x);CHKERRQ(ierr);

  ierr = ini_bou(x,&user);CHKERRQ(ierr);
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,"ini_x",FILE_MODE_WRITE,&viewer);CHKERRQ(ierr);
  ierr = VecView(x,viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);

  ierr = TSCreate(PETSC_COMM_WORLD,&ts);CHKERRQ(ierr);
  ierr = TSSetDM(ts,user.da);CHKERRQ(ierr);
  ierr = TSSetProblemType(ts,TS_NONLINEAR);CHKERRQ(ierr);
  ierr = TSSetType(ts,TSARKIMEX);CHKERRQ(ierr);
  ierr = TSSetIFunction(ts,PETSC_NULL,IFunction,&user);CHKERRQ(ierr);
  /*  ierr = TSSetIJacobian(ts,PETSC_NULL,PETSC_NULL,IJacobian,&user);CHKERRQ(ierr);  */
  ierr = TSSetApplicationContext(ts,&user);CHKERRQ(ierr);
  ierr = TSSetInitialTimeStep(ts,0.0,.005);CHKERRQ(ierr);
  ierr = TSSetFromOptions(ts);CHKERRQ(ierr);
  ierr = TSSetPostStep(ts,PostStep);CHKERRQ(ierr);
  ierr = TSSolve(ts,x);CHKERRQ(ierr);

  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,"fin_x",FILE_MODE_WRITE,&viewer);CHKERRQ(ierr);
  ierr = VecView(x,viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);

  ierr = VecDestroy(&x);CHKERRQ(ierr);
  ierr = DMDestroy(&user.da);CHKERRQ(ierr);
  ierr = TSDestroy(&ts);CHKERRQ(ierr);
  PetscFinalize();
  return 0;
}

#undef __FUNCT__
#define __FUNCT__ "PostStep"
PetscErrorCode PostStep(TS ts)
{
  PetscErrorCode ierr;
  Vec            X;
  AppCtx         *user;
  PetscScalar    sum;
  PetscReal      t;
  PetscFunctionBegin;
  ierr = TSGetApplicationContext(ts,&user);CHKERRQ(ierr);
  ierr = TSGetTime(ts,&t);CHKERRQ(ierr);
  ierr = TSGetSolution(ts,&X);CHKERRQ(ierr);
  ierr = VecSum(X,&sum);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"sum(p)*dw*dtheta at t = %f = %f\n",(double)t,(double)(sum*user->dx*user->dy));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "ini_bou"
PetscErrorCode ini_bou(Vec X,AppCtx* user)
{
  PetscErrorCode ierr;
  DM             cda;
  DMDACoor2d     **coors;
  PetscScalar    **p;
  Vec            gc;
  PetscInt       i,j;
  PetscInt       xs,ys,xm,ym,M,N;
  PetscScalar    xi,yi;
  PetscScalar    sigmax=user->sigmax,sigmay=user->sigmay;
  PetscScalar    rho   =user->rho;
  PetscScalar    mux   =user->mux,muy=user->muy;
  PetscMPIInt    rank;
  PetscScalar    sum;

  PetscFunctionBeginUser;
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = DMDAGetInfo(user->da,PETSC_NULL,&M,&N,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL);
  user->dx = (user->xmax - user->xmin)/(M-1); user->dy = (user->ymax - user->ymin)/(N-1);
  ierr = DMGetCoordinateDM(user->da,&cda);CHKERRQ(ierr);
  ierr = DMGetCoordinates(user->da,&gc);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(cda,gc,&coors);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(user->da,X,&p);CHKERRQ(ierr);
  ierr = DMDAGetCorners(cda,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);
  for(i=xs; i < xs+xm; i++) {
    for(j=ys; j < ys+ym; j++) {
      xi = coors[j][i].x; yi = coors[j][i].y;
      p[j][i] = (0.5/(PETSC_PI*sigmax*sigmay*PetscSqrtScalar(1.0-rho*rho)))*PetscExpScalar(-0.5/(1-rho*rho)*(PetscPowScalar((xi-mux)/sigmax,2) + PetscPowScalar((yi-muy)/sigmay,2) - 2*rho*(xi-mux)*(yi-muy)/(sigmax*sigmay)));
    }
  }
  ierr = DMDAVecRestoreArray(cda,gc,&coors);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArray(user->da,X,&p);CHKERRQ(ierr);
  ierr = VecSum(X,&sum);CHKERRQ(ierr);
  sum = 1.0/(sum*user->dx*user->dy);
  ierr = VecScale(X,sum);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* First advection term */
#undef __FUNCT__
#define __FUNCT__ "adv1"
PetscErrorCode adv1(PetscScalar **p,PetscScalar y,PetscInt i,PetscInt j,PetscInt M,PetscScalar *p1,AppCtx *user)
{
  PetscScalar f,fpos,fneg;
  PetscFunctionBegin;
  f   =  (y - user->ws);
  fpos = PetscMax(f,0);
  fneg = PetscMin(f,0);
  if (user->st_width == 1) {
    *p1 = fpos*(p[j][i] - p[j][i-1])/user->dx + fneg*(p[j][i+1] - p[j][i])/user->dx;
  } else if (user->st_width == 2) {
    *p1 = fpos*(3*p[j][i] - 4*p[j][i-1] + p[j][i-2])/(2*user->dx) + fneg*(-p[j][i+2] + 4*p[j][i+1] - 3*p[j][i])/(2*user->dx);
  } else if (user->st_width == 3) {
    *p1 = fpos*(2*p[j][i+1] + 3*p[j][i] - 6*p[j][i-1] + p[j][i-2])/(6*user->dx) + fneg*(-p[j][i+2] + 6*p[j][i+1] - 3*p[j][i] - 2*p[j][i-1])/(6*user->dx);
  }
  /* *p1 = f*(p[j][i+1] - p[j][i-1])/user->dx;*/
  PetscFunctionReturn(0);
}

/* Second advection term */
#undef __FUNCT__
#define __FUNCT__ "adv2"
PetscErrorCode adv2(PetscScalar **p,PetscScalar x,PetscInt i,PetscInt j,PetscInt N,PetscScalar *p2,AppCtx *user)
{
  PetscScalar f,fpos,fneg;
  PetscFunctionBegin;
  f   = (user->ws/(2*user->H))*(user->PM_min - user->Pmax*sin(x));
  fpos = PetscMax(f,0);
  fneg = PetscMin(f,0);
  if (user->st_width == 1) {
    *p2 = fpos*(p[j][i] - p[j-1][i])/user->dy + fneg*(p[j+1][i] - p[j][i])/user->dy;
  } else if (user->st_width ==2) {
    *p2 = fpos*(3*p[j][i] - 4*p[j-1][i] + p[j-2][i])/(2*user->dy) + fneg*(-p[j+2][i] + 4*p[j+1][i] - 3*p[j][i])/(2*user->dy);
  } else if (user->st_width == 3) {
    *p2 = fpos*(2*p[j+1][i] + 3*p[j][i] - 6*p[j-1][i] + p[j-2][i])/(6*user->dy) + fneg*(-p[j+2][i] + 6*p[j+1][i] - 3*p[j][i] - 2*p[j-1][i])/(6*user->dy);
  }

  /* *p2 = f*(p[j+1][i] - p[j-1][i])/user->dy;*/
  PetscFunctionReturn(0);
}

/* Diffusion term */
#undef __FUNCT__
#define __FUNCT__ "diffuse"
PetscErrorCode diffuse(PetscScalar **p,PetscInt i,PetscInt j,PetscReal t,PetscScalar *p_diff,AppCtx * user)
{
  PetscFunctionBeginUser;
  if (user->st_width == 1) {
    *p_diff = user->disper_coe*((p[j-1][i] - 2*p[j][i] + p[j+1][i])/(user->dy*user->dy));
  } else if (user->st_width == 2) {
    *p_diff = user->disper_coe*((-p[j-2][i] + 16*p[j-1][i] - 30*p[j][i] + 16*p[j+1][i] - p[j+2][i])/(12.0*user->dy*user->dy));
  } else if (user->st_width == 3) {
    *p_diff = user->disper_coe*((2*p[j-3][i] - 27*p[j-2][i] + 270*p[j-1][i] - 490*p[j][i] + 270*p[j+1][i] - 27*p[j+2][i] + 2*p[j+3][i])/(180.0*user->dy*user->dy));
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "IFunction"
PetscErrorCode IFunction(TS ts,PetscReal t,Vec X,Vec Xdot,Vec F,void *ctx)
{
  PetscErrorCode ierr;
  AppCtx         *user=(AppCtx*)ctx;
  DM             cda;
  DMDACoor2d     **coors;
  PetscScalar    **p,**f,**pdot;
  PetscInt       i,j;
  PetscInt       xs,ys,xm,ym,M,N;
  Vec            localX,gc,localXdot;
  PetscScalar    p_adv1,p_adv2,p_diff;

  PetscFunctionBeginUser;
  ierr = DMDAGetInfo(user->da,PETSC_NULL,&M,&N,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL);
  ierr = DMGetCoordinateDM(user->da,&cda);CHKERRQ(ierr);
  ierr = DMDAGetCorners(cda,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);

  ierr = DMGetLocalVector(user->da,&localX);CHKERRQ(ierr);
  ierr = DMGetLocalVector(user->da,&localXdot);CHKERRQ(ierr);

  ierr = DMGlobalToLocalBegin(user->da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(user->da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalBegin(user->da,Xdot,INSERT_VALUES,localXdot);CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(user->da,Xdot,INSERT_VALUES,localXdot);CHKERRQ(ierr);

  ierr = DMGetCoordinatesLocal(user->da,&gc);CHKERRQ(ierr);

  ierr = DMDAVecGetArray(cda,gc,&coors);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(user->da,localX,&p);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(user->da,localXdot,&pdot);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(user->da,F,&f);CHKERRQ(ierr);

  user->disper_coe = PetscPowScalar((user->lambda*user->ws)/(2*user->H),2)*user->q*(1.0-PetscExpScalar(-t/user->lambda));
  for(i=xs; i < xs+xm; i++) {
    for(j=ys; j < ys+ym; j++) {
      ierr = adv1(p,coors[j][i].y,i,j,M,&p_adv1,user);CHKERRQ(ierr);
      ierr = adv2(p,coors[j][i].x,i,j,N,&p_adv2,user);CHKERRQ(ierr);
      ierr = diffuse(p,i,j,t,&p_diff,user);CHKERRQ(ierr);
      f[j][i] = -p_adv1 - p_adv2  + p_diff - pdot[j][i];
    }
  }
  ierr = DMDAVecRestoreArray(user->da,localX,&p);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArray(user->da,localX,&pdot);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(user->da,&localX);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(user->da,&localXdot);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArray(user->da,F,&f);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArray(cda,gc,&coors);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "IJacobian"
PetscErrorCode IJacobian(TS ts,PetscReal t,Vec X,Vec Xdot,PetscReal a,Mat *J,Mat *Jpre,MatStructure *flg,void *ctx)
{
  PetscErrorCode ierr;
  AppCtx         *user=(AppCtx*)ctx;
  DM             cda;
  DMDACoor2d     **coors;
  PetscInt       i,j;
  PetscInt       xs,ys,xm,ym,M,N;
  Vec            gc;
  PetscScalar    val[5],xi,yi;
  MatStencil     row,col[5];
  PetscScalar    c1,c3,c5,c1pos,c1neg,c3pos,c3neg;

  PetscFunctionBeginUser;
  *flg = SAME_NONZERO_PATTERN;
  ierr = DMDAGetInfo(user->da,PETSC_NULL,&M,&N,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL,PETSC_NULL);
  ierr = DMGetCoordinateDM(user->da,&cda);CHKERRQ(ierr);
  ierr = DMDAGetCorners(cda,&xs,&ys,0,&xm,&ym,0);CHKERRQ(ierr);

  ierr = DMGetCoordinatesLocal(user->da,&gc);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(cda,gc,&coors);CHKERRQ(ierr);
  for (i=xs; i < xs+xm; i++) {
    for (j=ys; j < ys+ym; j++) {
      PetscInt nc = 0;
      xi = coors[j][i].x; yi = coors[j][i].y;
      row.i = i; row.j = j;
      c1        = (yi-user->ws)/user->dx;
      c1pos    = PetscMax(c1,0);
      c1neg    = PetscMin(c1,0);
      c3        = (user->ws/(2.0*user->H))*(user->PM_min - user->Pmax*sin(xi))/user->dy;
      c3pos    = PetscMax(c3,0);
      c3neg    = PetscMin(c3,0);
      c5        = (PetscPowScalar((user->lambda*user->ws)/(2*user->H),2)*user->q*(1.0-PetscExpScalar(-t/user->lambda)))/(user->dy*user->dy);
      col[nc].i = i-1; col[nc].j = j;   val[nc++] = c1pos;
      col[nc].i = i+1; col[nc].j = j;   val[nc++] = -c1neg;
      col[nc].i = i;   col[nc].j = j-1; val[nc++] = c3pos + c5;
      col[nc].i = i;   col[nc].j = j+1; val[nc++] = -c3neg + c5;
      col[nc].i = i;   col[nc].j = j;   val[nc++] = -c1pos + c1neg -c3pos + c3neg -2*c5 -a;
      ierr = MatSetValuesStencil(*Jpre,1,&row,nc,col,val,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = DMDAVecRestoreArray(cda,gc,&coors);CHKERRQ(ierr);

  ierr =  MatAssemblyBegin(*Jpre,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*Jpre,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  if (*J != *Jpre) {
    ierr = MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}



#undef __FUNCT__
#define __FUNCT__ "Parameter_settings"
PetscErrorCode Parameter_settings(AppCtx *user)
{
  PetscErrorCode ierr;
  PetscBool      flg;

  PetscFunctionBeginUser;

  /* Set default parameters */
  user->ws     = 1.0;
  user->H      = 5.0;
  user->Pmax   = 2.1;
  user->PM_min = 1.0;
  user->lambda = 0.1;
  user->q      = 1.0;
  user->mux    = asin(user->PM_min/user->Pmax);
  user->sigmax = 0.1;
  user->sigmay = 0.1;
  user->rho    = 0.0;
  user->xmin   = -PETSC_PI;
  user->xmax   =  PETSC_PI;
  user->bx     = DMDA_BOUNDARY_PERIODIC;
  user->by     = DMDA_BOUNDARY_MIRROR;

  /*
     ymin of -3 seems to let the unstable solution move up and leave a zero in its wake
     with an ymin of -1 the wake is never exactly zero
  */
  user->ymin   = -3.0;
  user->ymax   = 10.0;
  user->st_width = 1;

  ierr = PetscOptionsGetScalar(PETSC_NULL,"-ws",&user->ws,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-Inertia",&user->H,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-Pmax",&user->Pmax,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-PM_min",&user->PM_min,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-lambda",&user->lambda,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-q",&user->q,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-mux",&user->mux,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-sigmax",&user->sigmax,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-muy",&user->muy,&flg);CHKERRQ(ierr);
  if (flg == 0) {
    user->muy = user->ws;
  }
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-sigmay",&user->sigmay,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-rho",&user->rho,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-xmin",&user->xmin,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-xmax",&user->xmax,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-ymin",&user->ymin,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetScalar(PETSC_NULL,"-ymax",&user->ymax,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-stencil_width",&user->st_width,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetEnum("","-bx",BoundaryTypes,(PetscEnum*)&user->bx,&flg);CHKERRQ(ierr);
  ierr = PetscOptionsGetEnum("","-by",BoundaryTypes,(PetscEnum*)&user->by,&flg);CHKERRQ(ierr);

  PetscFunctionReturn(0);
}