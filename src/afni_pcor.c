#undef MAIN
#include "afni_pcor.h"

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/***
   create a new references data structure:
      input:  numref = number of reference vectors to allow for
      output: pointer to the data structure
***/

PCOR_references * new_PCOR_references(int numref)
{
   PCOR_references *ref ;
   int ii,jj , nbad ;

   /*** check input for reasonableness ***/

   if( numref < 1 ){
      fprintf( stderr , "new_PCOR_references called with numref=%d\n" , numref ) ;
      return NULL ;
   }

   /*** allocate storage for top level data ***/

   ref = (PCOR_references *) malloc( sizeof(PCOR_references) ) ;
   if( ref == NULL ){
      fprintf( stderr , "new_PCOR_references:  malloc error for base\n" ) ;
      return NULL ;
   }
   ref->nref    = numref ;
   ref->nupdate = 0 ;      /* June 1995: not updated at all yet */

   /*** allocate storage for Cholesky factor
        (an array of rows, row #ii is length ii+1, for ii=0..numref-1) ***/

   ref->chol = (float **) malloc( sizeof(float *) * numref ) ;
   if( ref->chol == NULL ){
      fprintf( stderr , "new_PCOR_references: malloc error for chol\n" ) ;
      free(ref) ; return NULL ;
   }

   for( ii=0,nbad=0 ; ii < numref ; ii++ ){
      ref->chol[ii] = (float *) malloc( sizeof(float) * (ii+1) ) ;
      if( ref->chol[ii] == NULL ) nbad++ ;
   }

   if( nbad > 0 ){
      fprintf( stderr , "new_PCOR_references: malloc error for chol[ii]\n" ) ;
      free_PCOR_references( ref ) ; return NULL ;
   }

   /*** allocate storage for vectors of alpha, f, g ***/

   ref->alp = (float *) malloc( sizeof(float) * numref ) ;
   ref->ff  = (float *) malloc( sizeof(float) * numref ) ;
   ref->gg  = (float *) malloc( sizeof(float) * numref ) ;

   if( ref->alp == NULL || ref->ff == NULL || ref->gg == NULL ){
      fprintf( stderr , "new_PCOR_references: malloc error for data\n" ) ;
      free_PCOR_references( ref ) ; return NULL ;
   }

   /*** initialize Cholesky factor ***/

   for( ii=0 ; ii < numref ; ii++ ){
      for( jj=0 ; jj < ii ; jj++ ) RCH(ref,ii,jj) = 0.0 ;
      RCH(ref,ii,ii) = REF_EPS ;
   }

   return ref ;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/***
   update a references structure:
      input:  vec = pointer to nref values of the reference vectors
                    at the new time point
              ref = references data structure
      output: ref is updated;
                the Cholesky factor is modified via the Carlson algorithm,
                the alpha, f, and g factors are saved for later use
***/

void update_PCOR_references(float * vec, PCOR_references * ref)
{
   int nr = ref->nref , jj,kk ;
   float bold , bnew , aaa,fff,ggg ;

   static float * zz = NULL ;      /* local storage for a copy of vec */
   static int     zz_size = -1 ;

   /*** copy vector data into local storage (will be altered below) ***/

   if( zz_size < nr ){   /* get new space, if not enough is present */

      if( zz != NULL ) free( zz ) ;
      zz      = (float *) malloc( sizeof(float) * nr ) ;
      zz_size = nr ;
      if( zz == NULL ){
         fprintf( stderr , "\nupdate_PCOR_references: cannot malloc!\n" ) ;
         exit(1) ;
      }
   }

   for( jj=0 ; jj < nr ; jj++) zz[jj] = vec[jj] ;

   /*** Carlson algorithm ***/

   bold = 1.0 ;

   for( jj=0 ; jj < nr ; jj++ ){

      aaa  = zz[jj] / RCH(ref,jj,jj) ;        /* alpha */
      bnew = sqrt( bold*bold + aaa*aaa ) ;    /* new beta */
      fff  = bnew / bold ;                    /* f factor */
      ggg  = aaa  / (bnew*bold) ;             /* g factor */
      bold = bnew ;                           /* new beta becomes old beta */

      ref->alp[jj] = aaa ;   /* save these for later use */
      ref->ff[jj]  = fff ;
      ref->gg[jj]  = ggg ;

      for( kk=jj ; kk < nr ; kk++ ){
         zz[kk]        -= aaa * RCH(ref,kk,jj) ;
         RCH(ref,kk,jj) = fff * RCH(ref,kk,jj) + ggg * zz[kk] ;
      }
   }

   ref->betasq = 1.0 / ( bold * bold ) ;  /* and save this too! */

   (ref->nupdate)++ ;  /* June 1995: another update! */
   return ;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/***
   create a new voxel partial correlation data structure
      inputs:  numvox = number of voxels in image
               numref = number of reference vectors to allow for
      output:  pointer to voxel_corr data structure
***/

PCOR_voxel_corr * new_PCOR_voxel_corr(int numvox, int numref)
{
   int vox , jj ;
   PCOR_voxel_corr *vc ;

   /*** check input for OK-osity ***/

   if( numvox < 1 ){
      fprintf( stderr , "new_PCOR_voxel_corr: numvox=%d\n" , numvox ) ;
      return NULL ;
   }

   /*** get the base storage ***/

   vc = (PCOR_voxel_corr *) malloc( sizeof(PCOR_voxel_corr) ) ;
   if( vc == NULL ){
      fprintf( stderr , "new_PCOR_voxel_corr:  cannot malloc base\n" ) ;
      return NULL ;
   }

   /*** setup the references common to all voxels ***/

   vc->nvox    = numvox ;
   vc->nref    = numref ;
   vc->nupdate = 0 ;      /* June 1995: not updated at all yet */

   /*** setup the storage of the last row for each voxel ***/

   vc->chrow = (float *)malloc( sizeof(float) * numvox*(numref+1) );
   if( vc->chrow == NULL ){
      fprintf( stderr , "new_PCOR_voxel_corr:  cannot malloc last rows\n" ) ;
      free( vc ) ; return NULL ;
   }

   /*** initialize each voxel ***/

   for( vox=0 ; vox < numvox ; vox++ ){
      for( jj=0 ; jj < numref ; jj++ ) VCH(vc,vox,jj) = 0 ;
      VCH(vc,vox,numref) = REF_EPS ;
   }

   return vc ;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*** de-allocate a references data structure ***/

void free_PCOR_references(PCOR_references * ref)
{
   int ii , nr ;

   if( ref == NULL ) return ;

   nr = ref->nref ; if( nr <= 0 ) return ;

   if( ref->alp != NULL ) free(ref->alp) ;
   if( ref->ff  != NULL ) free(ref->ff)  ;
   if( ref->gg  != NULL ) free(ref->gg)  ;

   if( ref->chol != NULL ){
      for( ii=0 ; ii < nr ; ii++ )
         if( ref->chol[ii] != NULL ) free( ref->chol[ii] ) ;
      free(ref->chol) ;
   }

   free(ref) ;
   return ;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

void free_PCOR_voxel_corr(PCOR_voxel_corr * vc)
{
   if( vc != NULL ){
      if( vc->chrow != NULL ) free( vc->chrow ) ;
      free( vc ) ;
   }
   return ;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*** compute the partial correlation coefficients ***/
/*** array pcor must be large enough to hold the results ***/

#define DENEPS 1.e-5

void PCOR_get_pcor(PCOR_references * ref, PCOR_voxel_corr * vc, float * pcor)
{
   int vox , nv = vc->nvox , nr = vc->nref ;
   float den ;

   /*** check inputs for OK-ness ***/

   if( vc->nref != ref->nref ){
      fprintf( stderr , "\nPCOR_get_pcor: reference size mismatch!\n" ) ;
      exit(1) ;
   }

   /*** Work ***/

   for( vox=0 ; vox < nv ; vox++ ){

      den = VCH(vc,vox,nr) ;
      if( den > DENEPS ){
         pcor[vox] = VCH(vc,vox,nr-1)
                      / sqrt( den + SQR(VCH(vc,vox,nr-1)) ) ;
      } else {
         pcor[vox] = 0.0 ;
      }

   }

   return ;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

void PCOR_get_mcor(PCOR_references * ref, PCOR_voxel_corr * vc, int m , float * pcor)
{
   int vox , nv = vc->nvox , nr = vc->nref , ii ;
   double den ;

   /*** check inputs for OK-ness ***/

   if( vc->nref != ref->nref ){
      fprintf( stderr , "\nPCOR_get_mcor: reference size mismatch!\n" ) ;
      exit(1) ;
   }
   if( m >= nr ){
      fprintf( stderr , "\nPCOR_get_mcor: m=%d but nref=%d\n",m,nr) ;
      exit(1) ;
   }

   /*** Work ***/

   for( vox=0 ; vox < nv ; vox++ ){
      den = VCH(vc,vox,nr) ;
      switch(m){
         default:
            for( ii=1 ; ii <= m ; ii++ ) den += SQR(VCH(vc,vox,nr-ii)) ;
         break ;

         case 1: den +=  SQR(VCH(vc,vox,nr-1)) ; break ;

         case 2: den +=  SQR(VCH(vc,vox,nr-1))
                       + SQR(VCH(vc,vox,nr-2)) ; break ;

         case 3: den +=  SQR(VCH(vc,vox,nr-1))
                       + SQR(VCH(vc,vox,nr-2))
                       + SQR(VCH(vc,vox,nr-3)) ; break ;

         case 4: den +=  SQR(VCH(vc,vox,nr-1))
                       + SQR(VCH(vc,vox,nr-2))
                       + SQR(VCH(vc,vox,nr-3))
                       + SQR(VCH(vc,vox,nr-4)) ; break ;
      }

      den = 1.0 - VCH(vc,vox,nr) / den ;
      pcor[vox] = (den > 0.0) ? sqrt(den) : 0.0 ;
   }

   return ;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*** get activation coefficient ***/
/*** array coef must be big enough to hold the results ***/

void PCOR_get_coef(PCOR_references * ref, PCOR_voxel_corr * vc, float * coef)
{
   int vox , nv = vc->nvox , nr = vc->nref ;
   float scale ;

   /*** check inputs for OK-ness ***/

   if( vc->nref != ref->nref ){
      fprintf( stderr , "\nPCOR_get_coef: reference size mismatch!\n" ) ;
      exit(1) ;
   }

   /*** Work ***/

   scale = 1.0 / RCH(ref,nr-1,nr-1) ;

   for( vox=0 ; vox < nv ; vox++ ){
      coef[vox] = scale * VCH(vc,vox,nr-1) ;
   }

   return ;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*** get variance estimate (June 1995) ***/

void PCOR_get_variance(PCOR_voxel_corr * vc, float * var)
{
   int vox , nv = vc->nvox , nr = vc->nref , nup = vc->nupdate ;
   float scale ;

   /*** check inputs for OK-ness ***/

   if( nup <= nr ){
      fprintf(stderr,"PCOR_get_variance: not enough data to compute!\n") ;
      for( vox=0 ; vox < nv ; vox++ ) var[vox] = 0.0 ;
      return ;
   }

   /*** Work ***/

   scale = 1.0 / ( nup - nr ) ;

   for( vox=0 ; vox < nv ; vox++ ){
      var[vox] = scale * VCH(vc,vox,nr) ;
   }

   return ;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*** Get least squares fit coefficients (all of them, Frank).
     "fit" is an array of pointers to floats, of length nref.
     If fit[j] != NULL, then it points to an array of size nvox that
     will get the coefficient for reference #j (j=0..nref-1).  [June 1995] ***/

void PCOR_get_lsqfit(PCOR_references * ref, PCOR_voxel_corr * vc, float *fit[] )
{
   int vox,jj,kk , nv = vc->nvox , nr = vc->nref ;
   float sum ;
   float * ff ;

   /*** check inputs for OK-ness ***/

   if( vc->nref != ref->nref ){
      fprintf( stderr , "\nPCOR_get_lsqfit: reference size mismatch!\n" ) ;
      exit(1) ;
   }

   kk = 0 ;
   for( jj=0 ; jj < nr ; jj++ ) kk += (fit[jj] != NULL) ;
   if( kk == 0 ){
      fprintf(stderr,"PCOR_get_lsqfit: NO OUTPUT REQUESTED!\n") ;
      return ;
   }

   ff = (float *) malloc( sizeof(float) * nr ) ;
   if( ff == NULL ){
      fprintf( stderr, "\nPCOR_get_lsqfit: cannot malloc workspace!\n") ;
      exit(1) ;
   }

   /*** for each voxel, compute the nr fit coefficients (backwards) ***/

   for( vox=0 ; vox < nv ; vox++ ){

      for( jj=nr-1 ; jj >=0 ; jj-- ){
         sum = VCH(vc,vox,jj) ;
         for( kk=jj+1 ; kk < nr ; kk++ ) sum -= ff[kk] * RCH(ref,kk,jj) ;
         ff[jj] = sum / RCH(ref,jj,jj) ;
      }

      for( jj=0 ; jj < nr ; jj++ )
         if( fit[jj] != NULL ) fit[jj][vox] = ff[jj] ;
   }

   free( ff ) ;
   return ;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*** get correlation and alpha:

     only |pcor| >= pcthresh will be computed;
     only those voxels will have coef computed;
     arrays pcor and coef must be big enough to hold the results.
***/

void PCOR_get_pcor_and_coef(PCOR_references * ref , PCOR_voxel_corr * vc ,
                            float pcthresh , float * pcor , float * coef )
{
   int vox , nv = vc->nvox , nr = vc->nref ;
   float den , num , scale ;
   float pc , co , thfac ;

   /*** check inputs for OK-ness ***/

   if( vc->nref != ref->nref ){
      fprintf( stderr , "\nPCOR_get_pcor_and_coef: reference size mismatch!\n" ) ;
      exit(1) ;
   }

   scale   = 1.0 / RCH(ref,nr-1,nr-1) ;      /* for coef calculation */
   thfac   = SQR(pcthresh)/(1.0-SQR(pcthresh)) ;

   /*** Compute pcor and coef, thresholded on pcthresh ***/

   if( pcthresh <= 0.0 ){
      for( vox=0 ; vox < nv ; vox++ ){
         den       = VCH(vc,vox,nr) ;
         num       = VCH(vc,vox,nr-1) ;
         pcor[vox] = num / sqrt(den+SQR(num)) ;
         coef[vox] = scale * num ;
      }
   } else {
      thfac   = SQR(pcthresh)/(1.0-SQR(pcthresh)) ;
      for( vox=0 ; vox < nv ; vox++ ){
         den = VCH(vc,vox,nr) ;
         num = VCH(vc,vox,nr-1) ;
         if( SQR(num) > thfac*den ){                 /* fancy threshold test */
            pcor[vox] = num / sqrt(den+SQR(num)) ;
            coef[vox] = scale * num ;
         } else {                                    /* fails pcor thresh */
            pcor[vox] = coef[vox] = 0.0 ;
         }
      }
   }

   return ;
}
