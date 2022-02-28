/*
 * File: uservertcoordinate.c
 * Author: Yun Zhang
 * Institution: Stanford University
 * --------------------------------
 * This file include a function to user defined vertical coordinate
 * 
 */

#include "suntans.h"
#include "grid.h"
#include "phys.h"
#include "initialization.h"
#include "boundaries.h"
#include "util.h"
#include "tvd.h"
#include "mympi.h"
#include "scalars.h"
#include "vertcoordinate.h"
#include "physio.h"
#include "subgrid.h"

/*
 * Function: UserDefinedVerticalCoordinate
 * User define vertical coordinate 
 * basically it is a user-defined function to calculate the layer thickness based on 
 * different criterion
 * ----------------------------------------------------
 * the original code has already include 1 z-level, 2 isopycnal, 3 sigma, 4 variational 
 */
void UserDefinedVerticalCoordinate(gridT *grid, propT *prop, physT *phys,int myproc)
{
	// one for other update scheme
	
}

/*
 * Function: InitializeVerticalCoordinate
 * to setup the initial condition of dzz for user defined vertical coordinate
 * ----------------------------------------------------
 */
void InitializeVerticalCoordinate(gridT *grid, propT *prop, physT *phys,int myproc)
{
	// one for other update scheme
	
}

/*
 * Function: InitializeIsopycnalCoordinate
 * User define isopycnal coordinate 
 * define the initial dzz for each cell under isopycnal coordinate
 * ----------------------------------------------------
 */
void InitializeIsopycnalCoordinate(gridT *grid, propT *prop, physT *phys,int myproc)
{
  int i,k,Nkmax=grid->Nkmax;
  REAL ratio=1.0/Nkmax;
  for(i=0;i<grid->Nc;i++)
    for(k=0;k<grid->Nk[i];k++)
      grid->dzz[i][k]=ratio*(phys->h[i]+grid->dv[i]);
}

/*
 * Function: InitializeVariationalCoordinate
 * Initialize dzz for variational vertical coordinate
 * --------zz--------------------------------------------
 */
void InitializeVariationalCoordinate(gridT *grid, propT *prop, physT *phys,int myproc)
{
  int i,k;
  REAL ratio=1.0/grid->Nkmax;

  for(i=0;i<grid->Nc;i++)
  {
    for(k=grid->ctop[i];k<grid->Nk[i];k++)
    {
      grid->dzz[i][k]=ratio*(grid->dv[i]+phys->h[i]);
      grid->dzzold[i][k]=grid->dzz[i][k];
    }
  }
}

/*
 * Function: UserDefinedSigmaCoordinate
 * User define sigma coordinate 
 * basically to define the dsigma for each layer
 * ----------------------------------------------------
 */
void InitializeSigmaCoordinate(gridT *grid, propT *prop, physT *phys, int myproc)
{
  int i,k;
  for(k=0;k<grid->Nkmax;k++){

  	vert->dsigma[k]=1.0/grid->Nkmax;
  }

  for(i=0;i<grid->Nc;i++)
  {
  	for(k=grid->ctop[i];k<grid->Nk[i];k++)
  	{
  	  grid->dzz[i][k]=vert->dsigma[k]*(grid->dv[i]+phys->h[i]);
  	  grid->dzzold[i][k]=grid->dzz[i][k];
    }
  }
}

/*
 * Function: MonitorFunctionForVariationalMethod
 * calculate the value of monitor function for the variational approach
 * to update layer thickness when nonlinear==4
 * ----------------------------------------------------
 * Mii=sqrt(1-alphaM*(drhodz)^2)
 */
void MonitorFunctionForAverageMethod(gridT *grid, propT *prop, physT *phys, int myproc)
{
   int i,k;
   REAL alphaM=0,minM=0.15,max;
   // nonlinear=1 or 5 stable with alpham=320
   // nonlinear=2 stable with alpham=60
   // nonlinear=4 stable with alpham=60
 
   for(i=0;i<grid->Nc;i++)
   {
     max=0;
     vert->Msum[i]=0;
     for(k=grid->ctop[i]+1;k<grid->Nk[i]-1;k++){
       vert->Mc[i][k]=1000*(phys->rho[i][k-1]-phys->rho[i][k+1])/(0.5*grid->dzz[i][k-1]+grid->dzz[i][k]+0.5*grid->dzz[i][k+1]);
       if(fabs(vert->Mc[i][k])>max)
         max=fabs(vert->Mc[i][k]);
     }
     
     // top boundary
     k=grid->ctop[i];
     vert->Mc[i][k]=1000*(phys->rho[i][k]-phys->rho[i][k+1])/(0.5*grid->dzz[i][k]+0.5*grid->dzz[i][k+1]);
     if(fabs(vert->Mc[i][k])>max)
       max=fabs(vert->Mc[i][k]);   
     // bottom boundary
     k=grid->Nk[i]-1;
     vert->Mc[i][k]=1000*(phys->rho[i][k-1]-phys->rho[i][k])/(0.5*grid->dzz[i][k-1]+0.5*grid->dzz[i][k]);
     if(fabs(vert->Mc[i][k])>max)
       max=fabs(vert->Mc[i][k]);   
     if(max<1)
       max=1;
     
     for(k=grid->ctop[i];k<grid->Nk[i];k++){ 
       vert->Mc[i][k]=sqrt(1+alphaM*vert->Mc[i][k]/max*vert->Mc[i][k]/max);
       if(vert->Mc[i][k]<minM)
         vert->Mc[i][k]=minM;     
       vert->Msum[i]+=1/vert->Mc[i][k];
     }
   }
}

/*
 * Function: MonitorFunctionForVariationalMethod
 * calculate the value of monitor function for the variational approach
 * to update layer thickness when nonlinear==4
 * solve the elliptic equation using iteration method
 * ----------------------------------------------------
 * Mii=sqrt(1+alphaM*(drhodz)^2)
 * alpha_H define how much horizontal diffusion 
 * alphaH define how much horizontal density gradient 
 * alphaV define how much vertical density gradient
 */
void MonitorFunctionForVariationalMethod(gridT *grid, propT *prop, physT *phys, int myproc, int numprocs, MPI_Comm comm)
{
  int i,k,j,nf,neigh,ne,kk,nc1,nc2;
	REAL alphaV = 10e-5;
  REAL alpha_H = 1.0;
	REAL alphaH = 2*alphaV;
	REAL maxM = 2.0;
	REAL rScale = prop->grav/10.0;
	REAL normal,max,tmp;
  REAL max_gradient_v,max_gradient_h=0,max_gradient_h_global,H1,H2,rho1,rho2;
  // initialize everything zero

  for(i=0;i<grid->Nc;i++)
    for(k=0;k<grid->Nk[i];k++)
      vert->Mc[i][k]=0;
  
  for(j=0;j<grid->Ne;j++)
    for(k=0;k<grid->Nke[j]+1;k++)
      vert->Me_l[j][k]=0;

  // calculate Mc first
  for(i=0;i<grid->Nc;i++)
  {
    // calculate gradient
    max_gradient_v=0;
    for(k=grid->ctop[i]+1;k<grid->Nk[i]-1;k++){
      vert->Mc[i][k]=RHO0*rScale*(phys->rho[i][k-1]-phys->rho[i][k+1])/(0.5*grid->dzz[i][k-1]+grid->dzz[i][k]+0.5*grid->dzz[i][k+1]);
      if(fabs(vert->Mc[i][k])>max_gradient_v)
        max_gradient_v=fabs(vert->Mc[i][k]);
    }

    // top boundary
    k=grid->ctop[i];
    vert->Mc[i][k]=RHO0*rScale*(phys->rho[i][k]-phys->rho[i][k+1])/(0.5*grid->dzz[i][k]+0.5*grid->dzz[i][k+1]);
    if(fabs(vert->Mc[i][k])>max_gradient_v)
      max_gradient_v=fabs(vert->Mc[i][k]);   

    // bottom boundary
    k=grid->Nk[i]-1;
    vert->Mc[i][k]=RHO0*rScale*(phys->rho[i][k-1]-phys->rho[i][k])/(0.5*grid->dzz[i][k-1]+0.5*grid->dzz[i][k]);
    if(fabs(vert->Mc[i][k])>max_gradient_v)
      max_gradient_v=fabs(vert->Mc[i][k]);   
    if(max_gradient_v<1)
      max_gradient_v=1;
    // calculate monitor function value
    for(k=grid->ctop[i];k<grid->Nk[i];k++){ 


//if(k>0.85*grid->Nk[i]) {
//	vert->Mc[i][k] = 1.0+0.1*vert->Mc[i][k]/max_gradient_v;
//	continue;
//}



      if(alphaV!=0){

//        if(vert->Mc[i][k]/max_gradient_v>(maxM-1)/sqrt(alphaV))
//          vert->Mc[i][k]=maxM; 
//        else
//          vert->Mc[i][k]=sqrt(1+alphaV*vert->Mc[i][k]/max_gradient_v*vert->Mc[i][k]/max_gradient_v);

//        if(vert->Mc[i][k]/max_gradient_v > 0.9)
//          vert->Mc[i][k] = sqrt(1+alphaV*vert->Mc[i][k]*vert->Mc[i][k]/10.0);
//        else
          vert->Mc[i][k]=sqrt(1+alphaV*vert->Mc[i][k]*vert->Mc[i][k]);

      } else 
        vert->Mc[i][k]=1;
        
        
      if (vert->Mc[i][k]>maxM) {
      	vert->Mc[i][k] = maxM;
      }
        
        
    }
  }

  // calculate Me_l 
  // calculate gradient
  for(j=0;j<grid->Ne;j++)
  {
    nc1=grid->grad[2*j];
    nc2=grid->grad[2*j+1];
    if(nc1==-1)
      nc1=nc2;
    if(nc2==-1)
      nc2=nc1;   
    
    // interior layer
    for(k=grid->etop[j]+1;k<grid->Nke[j];k++)
    {
      rho1=grid->dzz[nc1][k-1]/(grid->dzz[nc1][k]+grid->dzz[nc1][k-1])*phys->rho[nc1][k]+
        grid->dzz[nc1][k]/(grid->dzz[nc1][k]+grid->dzz[nc1][k-1])*phys->rho[nc1][k-1];
      rho2=grid->dzz[nc2][k-1]/(grid->dzz[nc2][k]+grid->dzz[nc2][k-1])*phys->rho[nc2][k]+
        grid->dzz[nc2][k]/(grid->dzz[nc2][k]+grid->dzz[nc2][k-1])*phys->rho[nc2][k-1];
      vert->Me_l[j][k]=RHO0*rScale*(rho1-rho2)/grid->dg[j];
      if(fabs(vert->Me_l[j][k])>max_gradient_h)
        max_gradient_h=fabs(vert->Me_l[j][k]);
    }

    // top and bottom
    k=grid->etop[j];
    vert->Me_l[j][k]=RHO0*rScale*(phys->rho[nc1][k]-phys->rho[nc2][k])/grid->dg[j];
    if(fabs(vert->Me_l[j][k])>max_gradient_h)
      max_gradient_h=fabs(vert->Me_l[j][k]); 
    k=grid->Nke[j];
    vert->Me_l[j][k]=RHO0*rScale*(phys->rho[nc1][k-1]-phys->rho[nc2][k-1])/grid->dg[j];
    if(fabs(vert->Me_l[j][k])>max_gradient_h)
      max_gradient_h=fabs(vert->Me_l[j][k]);
  }

  // find max_global and normalize
  MPI_Reduce(&max_gradient_h,&max_gradient_h_global,1,MPI_DOUBLE,MPI_MAX,0,comm);
  MPI_Bcast(&max_gradient_h_global,1,MPI_DOUBLE,0,comm);
//  if(max_gradient_h_global<1){
    max_gradient_h_global=1.0;
//  }

  for(j=0;j<grid->Ne;j++)
    for(k=grid->etop[j];k<=grid->Nke[j];k++){
      vert->Me_l[j][k]=alpha_H*sqrt(1+alphaH*vert->Me_l[j][k]/max_gradient_h_global*
        vert->Me_l[j][k]/max_gradient_h_global);
    }













}
