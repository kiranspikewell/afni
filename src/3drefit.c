/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  This software is Copyright 1994-1997 by

            Medical College of Wisconsin
            8701 Watertown Plank Road
            Milwaukee, WI 53226

  License is granted to use this program for nonprofit research purposes only.
  The Medical College of Wisconsin makes no warranty of usefulness
  of this program for any particular purpose.  The redistribution of this
  program for a fee, or the derivation of for-profit works from this program
  is not allowed.
-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

#include "3ddata.h"

void Syntax(char * str)
{
   int ii ;

   if( str != NULL ){
      printf("\n*** Fatal error: %s\n\n*** Try '3drefit -help'\n",str) ;
      exit(1) ;
   }

   printf( "Changes some of the information inside a 3D dataset's header.\n"
           "Note that this program does NOT change the .BRIK file at all;\n"
           "the main purpose of 3drefit is to fix up errors made when\n"
           "using to3d.\n"
           "To see the current values stored in a .HEAD file, use the command\n"
           "'3dinfo dataset'.  Using 3dinfo both before and after 3drefit is\n"
           "a good idea to make sure the changes have been made correctly!\n\n"
         ) ;

   printf(
    "Usage: 3drefit [options] dataset ...\n"
    "where the options are\n"
    "  -orient code    Sets the orientation of the 3D volume(s) in the .BRIK.\n"
    "                  The code must be 3 letters, one each from the\n"
    "                  pairs {R,L} {A,P} {I,S}.  The first letter gives\n"
    "                  the orientation of the x-axis, the second the\n"
    "                  orientation of the y-axis, the third the z-axis:\n"
    "                     R = right-to-left         L = left-to-right\n"
    "                     A = anterior-to-posterior P = posterior-to-anterior\n"
    "                     I = inferior-to-superior  S = superior-to-inferior\n"
    "               ** WARNING: when changing the orientation, you must be sure\n"
    "                  to check the origins as well, to make sure that the volume\n"
    "                  is positioned correctly in space.\n"
    "\n"
    "  -xorigin distx  Puts the center of the edge voxel off at the given\n"
    "  -yorigin disty  distance, for the given axis (x,y,z); distances in mm.\n"
    "  -zorigin distz  (x=first axis, y=second axis, z=third axis).\n"
    "                  Usually, only -zorigin makes sense.  Note that this\n"
    "                  distance is in the direction given by the corresponding\n"
    "                  letter in the -orient code.  For example, '-orient RAI'\n"
    "                  would mean that '-zorigin 30' sets the center of the\n"
    "                  first slice at 30 mm Inferior.  See the to3d manual\n"
    "                  for more explanations of axes origins.\n"
    "               ** SPECIAL CASE: you can use the string 'cen' in place of\n"
    "                  a distance to force that axis to be re-centered.\n"
    "\n"
    "  -xdel dimx      Makes the size of the voxel the given dimension,\n"
    "  -ydel dimy      for the given axis (x,y,z); dimensions in mm.\n"
    "  -zdel dimz   ** WARNING: if you change a voxel dimension, you will\n"
    "                  probably have to change the origin as well.\n"
    "\n"
    "  -TR time        Changes the TR time to a new value (see 'to3d -help').\n"
    "               ** WARNING: this only applies to 3D+time datasets.\n"
    "\n"
    "  -newid          Changes the ID code of this dataset as well.\n"
    "\n"
    "  -statpar v ...  Changes the statistical parameters stored in this\n"
    "                  dataset.  See 'to3d -help' for more details.\n"
    "\n"
    "  -markers        Adds an empty set of AC-PC markers to the dataset,\n"
    "                  if it can handle them (is anatomical, doesn't already\n"
    "                  have markers, is in the +orig view, and isn't 3D+time).\n"
    "\n"
    "  -type           Changes the type of data that is declared for this\n"
    "                  dataset, where 'type' is chosen from the following:\n"
   ) ;

   printf("       ANATOMICAL TYPES\n") ;
   for( ii=FIRST_ANAT_TYPE ; ii <= LAST_ANAT_TYPE ; ii++ ){
      printf("     %8s == %-16.16s",ANAT_prefixstr[ii],ANAT_typestr[ii] ) ;
      if( (ii-FIRST_ANAT_TYPE)%2 == 1 ) printf("\n") ;
   }
   if( (ii-FIRST_ANAT_TYPE)%2 == 1 ) printf("\n") ;

   printf("       FUNCTIONAL TYPES\n") ;
   for( ii=FIRST_FUNC_TYPE ; ii <= LAST_FUNC_TYPE ; ii++ ){
      printf("     %8s == %-16.16s",FUNC_prefixstr[ii],FUNC_typestr[ii] ) ;
      if( (ii-FIRST_ANAT_TYPE)%2 == 1 ) printf("\n") ;
   }
   if( (ii-FIRST_ANAT_TYPE)%2 == 1 ) printf("\n") ;

   exit(0) ;
}

int main( int argc , char * argv[] )
{
   THD_3dim_dataset * dset ;
   THD_dataxes      * daxes ;
   int new_stuff = 0 ;
   int new_orient = 0 ; char orient_code[4] ; int xxor,yyor,zzor ;
   int new_xorg   = 0 ; float xorg ; int cxorg=0 ;
   int new_yorg   = 0 ; float yorg ; int cyorg=0 ;
   int new_zorg   = 0 ; float zorg ; int czorg=0 ;
   int new_xdel   = 0 ; float xdel ;
   int new_ydel   = 0 ; float ydel ;
   int new_zdel   = 0 ; float zdel ;
   int new_TR     = 0 ; float TR ;
   int new_tunits = 0 ; int tunits ;
   int new_idcode = 0 ;
   int new_stataux= 0 ; float stataux[MAX_STAT_AUX] ;
   int new_type   = 0 ; int dtype , ftype , nvals ;
   int new_markers= 0 ;
   char str[256] ;
   int  iarg , ii ;

   if( argc < 2 || strncmp(argv[1],"-help",4) == 0 ) Syntax(NULL) ;

   iarg = 1 ;

   while( iarg < argc && argv[iarg][0] == '-' ){

#if 0
      if( strcmp(argv[iarg],"-v") == 0 ){ verbose = 1 ; iarg++ ; continue ; }
#endif

      /*----- -orient code option -----*/

#define ORCODE(aa) \
  ( (aa)=='R' ? ORI_R2L_TYPE : (aa)=='L' ? ORI_L2R_TYPE : \
    (aa)=='P' ? ORI_P2A_TYPE : (aa)=='A' ? ORI_A2P_TYPE : \
    (aa)=='I' ? ORI_I2S_TYPE : (aa)=='S' ? ORI_S2I_TYPE : ILLEGAL_TYPE )

#define OR3OK(x,y,z) ( ((x)&6) + ((y)&6) + ((z)&6) == 6 )

      if( strncmp(argv[iarg],"-orient",4) == 0 ){
         char acod ;

         if( iarg+1 >= argc ) Syntax("need an argument after -orient!");

         MCW_strncpy(orient_code,argv[++iarg],4) ;
         if( strlen(orient_code) != 3 ) Syntax("Illegal -orient code") ;

         acod = toupper(orient_code[0]) ; xxor = ORCODE(acod) ;
         acod = toupper(orient_code[1]) ; yyor = ORCODE(acod) ;
         acod = toupper(orient_code[2]) ; zzor = ORCODE(acod) ;

        if( xxor<0 || yyor<0 || zzor<0 || ! OR3OK(xxor,yyor,zzor) )
           Syntax("Unusable -orient code!") ;

         new_orient = 1 ; new_stuff++ ;
         iarg++ ; continue ;  /* go to next arg */
      }

      /** -?origin dist **/

      if( strncmp(argv[iarg],"-xorigin",4) == 0 ){
         if( ++iarg >= argc ) Syntax("need an argument after -xorigin!");
         if( strncmp(argv[iarg],"cen",3) == 0 ) cxorg = 1 ;
         else                                   xorg  = strtod(argv[iarg],NULL) ;
         new_xorg = 1 ; new_stuff++ ;
         iarg++ ; continue ;  /* go to next arg */
      }

      if( strncmp(argv[iarg],"-yorigin",4) == 0 ){
         if( ++iarg >= argc ) Syntax("need an argument after -yorigin!");
         if( strncmp(argv[iarg],"cen",3) == 0 ) cyorg = 1 ;
         else                                   yorg  = strtod(argv[iarg],NULL) ;
         new_yorg = 1 ; new_stuff++ ;
         iarg++ ; continue ;  /* go to next arg */
      }

      if( strncmp(argv[iarg],"-zorigin",4) == 0 ){
         if( ++iarg >= argc ) Syntax("need an argument after -zorigin!");
         if( strncmp(argv[iarg],"cen",3) == 0 ) czorg = 1 ;
         else                                   zorg  = strtod(argv[iarg],NULL) ;
         new_zorg = 1 ; new_stuff++ ;
         iarg++ ; continue ;  /* go to next arg */
      }

      /** -?del dim **/

      if( strncmp(argv[iarg],"-xdel",4) == 0 ){
         if( iarg+1 >= argc ) Syntax("need an argument after -xdel!");
         xdel = strtod( argv[++iarg]  , NULL ) ;
         if( xdel <= 0.0 ) Syntax("argument after -xdel must be positive!") ;
         new_xdel = 1 ; new_stuff++ ;
         iarg++ ; continue ;  /* go to next arg */
      }

      if( strncmp(argv[iarg],"-ydel",4) == 0 ){
         if( iarg+1 >= argc ) Syntax("need an argument after -ydel!");
         ydel = strtod( argv[++iarg]  , NULL ) ;
         if( ydel <= 0.0 ) Syntax("argument after -ydel must be positive!") ;
         new_ydel = 1 ; new_stuff++ ;
         iarg++ ; continue ;  /* go to next arg */
      }

      if( strncmp(argv[iarg],"-zdel",4) == 0 ){
         if( iarg+1 >= argc ) Syntax("need an argument after -zdel!");
         zdel = strtod( argv[++iarg]  , NULL ) ;
         if( zdel <= 0.0 ) Syntax("argument after -zdel must be positive!") ;
         new_zdel = 1 ; new_stuff++ ;
         iarg++ ; continue ;  /* go to next arg */
      }

      /** -TR **/

      if( strncmp(argv[iarg],"-TR",3) == 0 ){
         char * eptr ;
         if( iarg+1 >= argc ) Syntax("need an argument after -TR!");
         TR = strtod( argv[++iarg]  , &eptr ) ;
         if( TR <= 0.0 ) Syntax("argument after -TR must be positive!") ;

         if( strcmp(eptr,"ms")==0 || strcmp(eptr,"msec")==0 ){
            new_tunits = 1 ; tunits = UNITS_MSEC_TYPE ;
         } else if( strcmp(eptr,"s")==0 || strcmp(eptr,"sec")==0 ){
            new_tunits = 1 ; tunits = UNITS_SEC_TYPE ;
         } else if( strcmp(eptr,"Hz")==0 || strcmp(eptr,"Hertz")==0 ){
            new_tunits = 1 ; tunits = UNITS_HZ_TYPE ;
         }

         new_TR = 1 ; new_stuff++ ;
         iarg++ ; continue ;  /* go to next arg */
      }

      /** -newid **/

      if( strncmp(argv[iarg],"-newid",4) == 0 ){
         new_idcode = 1 ; new_stuff++ ;
         iarg++ ; continue ;  /* go to next arg */
      }

      /** -statpar x x x **/

      if( strncmp(argv[iarg],"-statpar",4) == 0 ){
         float val ; char * ptr ;

         if( ++iarg >= argc ) Syntax("need an argument after -statpar!") ;

         for( ii=0 ; ii < MAX_STAT_AUX ; ii++ ) stataux[ii] = 0.0 ;

         ii = 0 ;
         do{
            val = strtod( argv[iarg] , &ptr ) ;
            if( *ptr != '\0' ) break ;
            stataux[ii++] = val ;
            iarg++ ;
         } while( iarg < argc ) ;

         if( ii == 0 ) Syntax("No numbers given after -statpar?") ;

         new_stataux = 1 ; new_stuff++ ;
         continue ;
      }

      /** -markers **/

      if( strncmp(argv[iarg],"-markers",4) == 0 ){
         new_markers = 1 ; new_stuff++ ;
         iarg++ ; continue ;  /* go to next arg */
      }

      /* -type from the anatomy prefixes */

      for( ii=FIRST_ANAT_TYPE ; ii <= LAST_ANAT_TYPE ; ii++ )
         if( strncmp( &(argv[iarg][1]) ,
                      ANAT_prefixstr[ii] , THD_MAX_PREFIX ) == 0 ) break ;

      if( ii <= LAST_ANAT_TYPE ){
         ftype = ii ;
         dtype = HEAD_ANAT_TYPE ;
         nvals = ANAT_nvals[ftype] ;
         new_type = 1 ; new_stuff++ ;
         iarg++ ; continue ;
      }

      /* -type from the function prefixes */

      for( ii=FIRST_FUNC_TYPE ; ii <= LAST_FUNC_TYPE ; ii++ )
         if( strncmp( &(argv[iarg][1]) ,
                      FUNC_prefixstr[ii] , THD_MAX_PREFIX ) == 0 ) break ;

      if( ii <= LAST_FUNC_TYPE ){
         ftype = ii ;
         dtype = HEAD_FUNC_TYPE ;
         nvals = FUNC_nvals[ftype] ;
         new_type = 1 ; new_stuff++ ;
         iarg++ ; continue ;
      }

      /** error **/

      { char str[256] ;
        sprintf(str,"Unknown option %s",argv[iarg]) ;
        Syntax(str) ;
      }

   }  /* end of loop over switches */

   if( new_stuff == 0 ) Syntax("No options given") ;

   /*--- process datasets ---*/

   for( ; iarg < argc ; iarg++ ){
      dset = THD_open_one_dataset( argv[iarg] ) ;
      if( dset == NULL ){
         fprintf(stderr,"** Can't open dataset %s\n",argv[iarg]) ;
         continue ;
      }
      printf("Processing dataset %s\n",argv[iarg]) ;

      daxes = dset->daxes ;

      if( new_orient ){
         daxes->xxorient = xxor ;
         daxes->yyorient = yyor ;
         daxes->zzorient = zzor ;
      }

      if( !new_xorg ) xorg = fabs(daxes->xxorg) ;
      if( !new_yorg ) yorg = fabs(daxes->yyorg) ;
      if( !new_zorg ) zorg = fabs(daxes->zzorg) ;


      if( !new_xdel ) xdel = fabs(daxes->xxdel) ;
      if( !new_ydel ) ydel = fabs(daxes->yydel) ;
      if( !new_zdel ) zdel = fabs(daxes->zzdel) ;

      if( cxorg ) xorg = 0.5 * (daxes->nxx - 1) * xdel ;
      if( cyorg ) yorg = 0.5 * (daxes->nyy - 1) * ydel ;
      if( czorg ) zorg = 0.5 * (daxes->nzz - 1) * zdel ;

      if( new_xorg || new_orient )
         daxes->xxorg = (ORIENT_sign[daxes->xxorient] == '+') ? (-xorg) : (xorg) ;

      if( new_yorg || new_orient )
         daxes->yyorg = (ORIENT_sign[daxes->yyorient] == '+') ? (-yorg) : (yorg) ;

      if( new_zorg || new_orient )
         daxes->zzorg = (ORIENT_sign[daxes->zzorient] == '+') ? (-zorg) : (zorg) ;

      if( new_xdel || new_orient )
         daxes->xxdel = (ORIENT_sign[daxes->xxorient] == '+') ? (xdel) : (-xdel) ;

      if( new_ydel || new_orient )
         daxes->yydel = (ORIENT_sign[daxes->yyorient] == '+') ? (ydel) : (-ydel) ;

      if( new_zdel || new_orient )
         daxes->zzdel = (ORIENT_sign[daxes->zzorient] == '+') ? (zdel) : (-zdel) ;

      if( new_TR ){
         if( dset->taxis == NULL ){
            printf("  ** can't process -TR for this dataset!\n") ;
         } else {
            float frac = TR / dset->taxis->ttdel ;
            int ii ;

            dset->taxis->ttdel = TR ;
            if( new_tunits ) dset->taxis->units_type = tunits ;
            if( dset->taxis->nsl > 0 ){
               for( ii=0 ; ii < dset->taxis->nsl ; ii++ )
                  dset->taxis->toff_sl[ii] *= frac ;
            }
         }
      }

      if( (new_orient || new_zorg) && dset->taxis != NULL && dset->taxis->nsl > 0 ){
         dset->taxis->zorg_sl = daxes->zzorg ;
      }

      if( (new_orient || new_zdel) && dset->taxis != NULL && dset->taxis->nsl > 0 ){
         dset->taxis->dz_sl = daxes->zzdel ;
      }

      if( new_idcode ) dset->idcode = MCW_new_idcode() ;

      if( new_type ){
         if( nvals > 1 && dset->taxis != NULL ){
            fprintf(stderr,"  ** can't change 3D+time dataset to new type:\n") ;
            fprintf(stderr,"     new type has more than one value per voxel!\n") ;
         } else if( dset->taxis == NULL && nvals != dset->dblk->nvals ){
            fprintf(stderr,"  ** can't change dataset to new type:\n") ;
            fprintf(stderr,"     mismatch in number of sub-bricks!\n") ;
         } else {
            dset->type      = dtype ;
            dset->func_type = ftype ;
         }
      }

      if( new_stataux ){
         for( ii=0 ; ii < MAX_STAT_AUX ; ii++ ){
            dset->stat_aux[ii] = stataux[ii] ;
         }
      }

      if( new_markers                           &&
          dset->type      == HEAD_ANAT_TYPE     &&
          dset->view_type == VIEW_ORIGINAL_TYPE &&
          dset->markers   == NULL               &&
          DSET_NUM_TIMES(dset) == 1                ){  /* code copied from to3d.c */

         THD_marker_set * markers ;
         int ii , jj ;

         markers = dset->markers = XtNew( THD_marker_set ) ;
         markers->numdef = 0 ;

         for( ii=0 ; ii < MARKS_MAXNUM ; ii++ ){       /* null all data out */
            markers->valid[ii] = 0 ;
            for( jj=0 ; jj < MARKS_MAXLAB  ; jj++ )
               markers->label[ii][jj] = '\0';
            for( jj=0 ; jj < MARKS_MAXHELP ; jj++ )
               markers->help[ii][jj]  = '\0';
         }

         for( ii=0 ; ii < NMARK_ALIGN ; ii++ ){       /* copy strings in */
            MCW_strncpy( &(markers->label[ii][0]) ,
                         THD_align_label[ii] , MARKS_MAXLAB ) ;
            MCW_strncpy( &(markers->help[ii][0]) ,
                         THD_align_help[ii] , MARKS_MAXHELP ) ;
         }

         for( ii=0 ; ii < MARKS_MAXFLAG ; ii++ )     /* copy flags in */
            markers->aflags[ii] = THD_align_aflags[ii] ;

      } else if( new_markers ){
            fprintf(stderr,"  ** can't add markers to this dataset\n") ;
      } /* end of markers */


      THD_write_3dim_dataset( NULL,NULL , dset , False ) ;
      THD_delete_3dim_dataset( dset , False ) ;
   }
   exit(0) ;
}
