#include "mrilib.h"
#include "remla.c"

int main( int argc , char *argv[] )
{
   MRI_IMAGE *inim , *outim ;
   int iarg , ii,jj , nreg , ntime , *tau=NULL , rnum ;
   NI_element *nelmat=NULL ; char *matname=NULL ;
   MTYPE rhomax=0.7 ; int rhonum=7 , delnum=10 ;
   char *cgl , *rst ;
   matrix X ;
   float cput ;

   if( argc < 2 || strcmp(argv[1],"-help") == 0 ){
     printf(
      "Usage: 1dREMLfit [option] file.1D\n"
      "Least squares fit with REML estimation of the ARMA(1,1) noise.\n"
      "\n"
      "Options (the first one is mandatory)\n"
      "------------------------------------\n"
      " -matrix mmm = Read the matrix 'mmm', which should have been\n"
      "                 output from 3dDeconvolve via the '-x1D' option.\n"
      " -MAXrho rm  = Set the max allowed rho parameter to 'rm' (default=0.7).\n"
      " -Nrho nr    = Use 'nr' values for the rho parameter (default=7).\n"
      " -Ndel nd    = Use 'nd' values for the del parameter (default=10).\n"
     ) ;
      PRINT_COMPILE_DATE ; exit(0) ;
   }

   iarg = 1 ;
   while( iarg < argc && argv[iarg][0] == '-' ){

      /** rho and del params **/

      if( strcmp(argv[iarg],"-MAXrho") == 0 ){
        rhomax = (MTYPE)strtod(argv[++iarg],NULL) ;
             if( rhomax < 0.3 ) rhomax = 0.3 ;
        else if( rhomax > 0.9 ) rhomax = 0.9 ;
        iarg++ ; continue ;
      }
      if( strcmp(argv[iarg],"-Nrho") == 0 ){
        rhonum = (int)strtod(argv[++iarg],NULL) ;
             if( rhonum <  2 ) rhonum =  2 ;
        else if( rhonum > 20 ) rhonum = 20 ;
        iarg++ ; continue ;
      }
      if( strcmp(argv[iarg],"-Ndel") == 0 ){
        delnum = (int)strtod(argv[++iarg],NULL) ;
             if( delnum <  2 ) delnum =  2 ;
        else if( delnum > 20 ) delnum = 20 ;
        iarg++ ; continue ;
      }

      /** -matrix **/

      if( strcmp(argv[iarg],"-matrix") == 0 ){
        if( nelmat != NULL ) ERROR_exit("More than 1 -matrix option!");
        nelmat = NI_read_element_fromfile( argv[++iarg] ) ; /* read NIML file */
        matname = argv[iarg];
        if( nelmat == NULL ){                     /* try to read as a 1D file */
          MRI_IMAGE *nim ; float *nar ;
          nim = mri_read_1D(argv[iarg]) ;
          if( nim != NULL ){              /* construct a minimal NIML element */
            nelmat = NI_new_data_element( "matrix" , nim->nx ) ;
            nar    = MRI_FLOAT_PTR(nim) ;
            for( jj=0 ; jj < nim->ny ; jj++ )
              NI_add_column( nelmat , NI_FLOAT , nar + nim->nx*jj ) ;
            mri_free(nim) ;
          }
        }
        if( nelmat == NULL || nelmat->type != NI_ELEMENT_TYPE )
          ERROR_exit("Can't process -matrix file!");
        iarg++ ; continue ;
      }

     ERROR_exit("Unknown option '%s'",argv[iarg]) ;
   }

   if( iarg >= argc ) ERROR_exit("No 1D file on command line?!") ;

   inim = mri_read_1D( argv[iarg] ) ;
   if( inim == NULL ) ERROR_exit("Can't read 1D file %s",argv[iarg]) ;

   nreg  = nelmat->vec_num ;
   ntime = nelmat->vec_len ;
   if( ntime != inim->nx )
     ERROR_exit("matrix vectors are %d long but input 1D file is %d long",
                ntime,inim->nx) ;

   cgl = NI_get_attribute( nelmat , "GoodList" ) ;
   if( cgl != NULL ){
     int Ngoodlist,*goodlist , Nruns,*runs ;
     NI_int_array *giar ;
     giar = NI_decode_int_list( cgl , ";," ) ;
     if( giar == NULL || giar->num < ntime )
       ERROR_exit("-matrix 'GoodList' badly formatted?") ;
     Ngoodlist = giar->num ; goodlist = giar->ar ;
     rst = NI_get_attribute( nelmat , "RunStart" ) ;
     if( rst != NULL ){
       NI_int_array *riar = NI_decode_int_list( rst , ";,") ;
       if( riar == NULL ) ERROR_exit("-matrix 'RunStart' badly formatted?") ;
       Nruns = riar->num ; runs = riar->ar ;
     } else {
       Nruns = 1 ; runs = calloc(sizeof(int),1) ;
     }
     rnum = 0 ; tau = (int *)malloc(sizeof(int)*ntime) ;
     for( ii=0 ; ii < ntime ; ii++ ){
       jj = goodlist[ii] ;
       for( ; rnum+1 < Nruns && jj >= runs[rnum+1] ; rnum++ ) ; /*nada*/
       tau[ii] = jj + 10000*rnum ;
     }
   }

   matrix_initialize( &X ) ;
   matrix_create( ntime , nreg , &X ) ;
   if( nelmat->vec_typ[0] == NI_FLOAT ){
     float *cd ;
     for( jj=0 ; jj < nreg ; jj++ ){
       cd = (float *)nelmat->vec[jj] ;
       for( ii=0 ; ii < ntime ; ii++ ) X.elts[ii][jj] = (MTYPE)cd[ii] ;
     }
   } else if( nelmat->vec_typ[0] == NI_DOUBLE ){
     double *cd ;
     for( jj=0 ; jj < nreg ; jj++ ){
       cd = (double *)nelmat->vec[jj] ;
       for( ii=0 ; ii < ntime ; ii++ ) X.elts[ii][jj] = (MTYPE)cd[ii] ;
     }
   } else {
     ERROR_exit("-matrix file stored will illegal data type!?") ;
   }

   cput = COX_cpu_time() ;
   REML_setup( &X , tau , rhonum,rhomax,delnum ) ;
   if( rrcol == NULL ) ERROR_exit("REML setup fails?" ) ;
   cput = COX_cpu_time() - cput ;
   INFO_message("REML setup: rows=%d cols=%d CPU=%.2f",ntime,nreg,cput) ;

   exit(0) ;
}
