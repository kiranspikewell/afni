/*
   This program calculates two-factor analysis of variance (ANOVA)
   for 3 dimensional AFNI data sets. 

   File:    3dANOVA2.c
   Author:  B. D. Ward
   Date:    09 December 1996

   Mod:     15 January 1997
            Incorporated header file 3dANOVA.h.

   Mod:     23 January 1997
            Major changes to sequence of calculations in order to reduce
            the disk space required to store temporary data files.
            Added option to check for required disk space.
          
*/

/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  This software is Copyright 1996, 1997 by

            Medical College of Wisconsin
            8701 Watertown Plank Road
            Milwaukee, WI 53226

  License is granted to use this program for nonprofit research purposes only.
  It is specifically against the license to use this program for any clinical
  application. The Medical College of Wisconsin makes no warranty of usefulness
  of this program for any particular purpose.  The redistribution of this
  program for a fee, or the derivation of for-profit works from this program
  is not allowed.
-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

#define PROGRAM_NAME "3dANOVA2"                      /* name of this program */
#define SUFFIX ".3danova2"                /* suffix for temporary data files */
#define LAST_MOD_DATE "4 February 1997" /* date of last program modification */

#include "3dANOVA.h"

/*---------------------------------------------------------------------------*/
/*
   Routine to display 3dANOVA2 help menu.
*/

void display_help_menu()
{
   printf 
      (
       "This program performs two-factor ANOVA on 3D data sets \n\n"
       "Usage: \n"
       "3dANOVA2 \n"
       "-type k          type of ANOVA model to be used:                  \n"
       "                    k=1  fixed effects model  (A and B fixed)     \n"
       "                    k=2  random effects model (A and B random)    \n"
       "                    k=3  mixed effects model  (A fixed, B random) \n"
       "                                                                  \n"
       "-alevels a                     a = number of levels of factor A   \n"
       "-blevels b                     b = number of levels of factor B   \n"
       "-dset 1 1 filename             data set for level 1 of factor A   \n"
       "                                        and level 1 of factor B   \n"
       " . . .                           . . .                            \n"
       "                                                                  \n"
       "-dset i j filename             data set for level i of factor A   \n"
       "                                        and level j of factor B   \n"
       " . . .                           . . .                            \n"
       "                                                                  \n"
       "-dset a b filename             data set for level a of factor A   \n"
       "                                        and level b of factor B   \n"
       "                                                                  \n"
       "[-voxel num]                   screen output for voxel # num      \n" 
       "[-diskspace]                   print out disk space required for  \n"
       "                                  program execution               \n"  
       "[-ftr filename]                F-statistic for treatment effect   \n"
       "                                  output is written to 'filename' \n"
       "[-fa filename]                 F-statistic for factor A effect    \n"
       "                                  output is written to 'filename' \n"
       "[-fb filename]                 F-statistic for factor B effect    \n"
       "                                  output is written to 'filename' \n"
       "[-fab filename]                F-statistic for interaction        \n"
       "                                  output is written to 'filename' \n"
       "[-amean i filename]            estimate of factor A level i mean  \n"
       "                                  output is written to 'filename' \n" 
       "[-bmean i filename]            estimate of factor B level i mean  \n"
       "                                  output is written to 'filename' \n" 
       "[-adiff i j filename]          difference between factor A levels \n"
       "                                  i and j, output to 'filename'   \n"
       "[-bdiff i j filename]          difference between factor B levels \n"
       "                                  i and j, output to 'filename'   \n"
       "[-acontr c1...cr filename]     contrast in factor A levels        \n"
       "                                  output is written to 'filename' \n"
       "[-bcontr c1...cr filename]     contrast in factor B levels        \n"
       "                                  output is written to 'filename' \n"
      );
 
   exit(0);
}


/*---------------------------------------------------------------------------*/
/*
   Routine to get user specified anova options.
*/

void get_options (int argc, char ** argv, anova_options * option_data)
{
  int nopt = 1;                  /* input option argument counter */
  int ival, jval;                /* integer input */
  int i, j, k;                   /* factor level counters */         
  int nij;                       /* number of data files in cell i,j */     
  float fval;                    /* float input */
  THD_3dim_dataset * dset=NULL;             /* test whether data set exists */
  char message[MAX_NAME_LENGTH];            /* error message */
  int n[MAX_LEVELS][MAX_LEVELS];            /* data file counters */


  /*----- does user request help menu? -----*/
  if (argc < 2 || strncmp(argv[1], "-help", 5) == 0)  display_help_menu();

  /*----- initialize the input options -----*/
   initialize_options (option_data);

  /*----- initialize data file counters -----*/
  for (i = 0;  i < MAX_LEVELS;  i++)
    for (j = 0;  j < MAX_LEVELS;  j++)
      n[i][j] = 0;
  

  /*----- main loop over input options -----*/
  while (nopt < argc)
    {

      /*----- allocate memory for storing data file names -----*/
      if ((option_data->xname == NULL) && (option_data->a > 0) &&
	  (option_data->b > 0))
	{
	  option_data->xname = 
	    (char *****) malloc (sizeof(char ****) * option_data->a);
	  for (i = 0;  i < option_data->a;  i++)
	    {
	      option_data->xname[i] =
		(char ****) malloc (sizeof(char ***) * option_data->b);
	      for (j = 0;  j < option_data->b;  j++)
		{
		  option_data->xname[i][j] =
		    (char ***) malloc (sizeof(char **) * 1);
		  for (k = 0;  k < 1;  k++)
		    {
		      option_data->xname[i][j][k] =
			(char **) malloc (sizeof(char *) * MAX_OBSERVATIONS);
		    }
		}
	    }
	}
	  

      /*-----   -diskspace   -----*/
      if( strncmp(argv[nopt],"-diskspace",5) == 0 )
	{
	  option_data->diskspace = 1;
	  nopt++ ; continue ;  /* go to next arg */
	}

      
      /*-----    -datum type   -----*/
      if( strncmp(argv[nopt],"-datum",5) == 0 ){
	if( ++nopt >= argc ) ANOVA_error("need an argument after -datum!") ;
	
	if( strcmp(argv[nopt],"short") == 0 ){
	  option_data->datum = MRI_short ;
	} else if( strcmp(argv[nopt],"float") == 0 ){
	  option_data->datum = MRI_float ;
	} else {
	  char buf[256] ;
	  sprintf(buf,"-datum of type '%s' is not supported in 3dANOVA2!",
		  argv[nopt] ) ;
	  ANOVA_error(buf) ;
	}
	nopt++ ; continue ;  /* go to next arg */
      }
      
      
      /*-----   -session dirname    -----*/
      if( strncmp(argv[nopt],"-session",5) == 0 ){
	nopt++ ;
	if( nopt >= argc ) ANOVA_error("need argument after -session!") ;
	strcpy(option_data->session , argv[nopt++]) ;
	continue ;
      }
      
      
      /*-----   -voxel num  -----*/
      if (strncmp(argv[nopt], "-voxel", 6) == 0)
	{
	  nopt++;
	  if (nopt >= argc)  ANOVA_error ("need argument after -voxel ");
	  sscanf (argv[nopt], "%d", &ival);
	  if (ival <= 0)
	    ANOVA_error ("illegal argument after -voxel ");
	  option_data->nvoxel = ival;
	  nopt++;
	  continue;
	}

      
      /*-----  -type k  -----*/
      if (strncmp(argv[nopt], "-type", 5) == 0)
	{
	  nopt++;
	  if (nopt >= argc)  ANOVA_error ("need argument after -type ");
	  sscanf (argv[nopt], "%d", &ival);
	  if ((ival < 1) || (ival > 3))
	    ANOVA_error ("illegal argument after -type ");
	  option_data->model = ival;
	  nopt++;
	  continue;
	}
      
      
      /*-----   -alevels a  -----*/
      if (strncmp(argv[nopt], "-alevels", 5) == 0)
	{
	  nopt++;
	  if (nopt >= argc)  ANOVA_error ("need argument after -alevels ");
	  sscanf (argv[nopt], "%d", &ival);
	  if ((ival <= 0) || (ival > MAX_LEVELS))
	    ANOVA_error ("illegal argument after -alevels ");
	  option_data->a = ival;
	  nopt++;
	  continue;
	}
      
      
      /*-----   -blevels b  -----*/
      if (strncmp(argv[nopt], "-blevels", 5) == 0)
	{
	  nopt++;
	  if (nopt >= argc)  ANOVA_error ("need argument after -blevels ");
	  sscanf (argv[nopt], "%d", &ival);
	  if ((ival <= 0) || (ival > MAX_LEVELS))
	    ANOVA_error ("illegal argument after -blevels ");
	  option_data->b = ival;
	  nopt++;
	  continue;
	}
      
      
      /*-----   -dset alevel blevel filename   -----*/
      if (strncmp(argv[nopt], "-dset", 5) == 0)
	{
	  nopt++;
	  if (nopt+2 >= argc)  ANOVA_error ("need 3 arguments after -dset ");
	  sscanf (argv[nopt], "%d", &ival);
	  if ((ival <= 0) || (ival > option_data->a))
	    ANOVA_error ("illegal argument after -dset ");
	  
	  nopt++;
	  sscanf (argv[nopt], "%d", &jval);
	  if ((jval <= 0) || (jval > option_data->b))
	    ANOVA_error ("illegal argument after -dset ");
	  
	  n[ival-1][jval-1] += 1;
	  nij = n[ival-1][jval-1];
	  if (nij > MAX_OBSERVATIONS)
	    ANOVA_error ("too many data files");
	  
	  /*--- check whether input files exist ---*/
	  nopt++;
	  dset = THD_open_one_dataset( argv[nopt] ) ;
	  if( ! ISVALID_3DIM_DATASET(dset) )
	    {
	      sprintf(message,"Unable to open dataset file %s\n", 
		      argv[nopt]);
	      ANOVA_error (message);
	    }
	  THD_delete_3dim_dataset( dset , False ) ; dset = NULL ;
	  
	  option_data->xname[ival-1][jval-1][0][nij-1] 
	    =  malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->xname[ival-1][jval-1][0][nij-1], argv[nopt]);
	  nopt++;
	  continue;
	}
      

      /*-----   -ftr filename   -----*/
      if (strncmp(argv[nopt], "-ftr", 5) == 0)
	{
	  nopt++;
	  if (nopt >= argc)  ANOVA_error ("need argument after -ftr ");
	  option_data->nftr = 1;
	  option_data->ftrname = malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->ftrname, argv[nopt]);
	  nopt++;
	  continue;
	}
      
      
      /*-----   -fa filename   -----*/
      if (strncmp(argv[nopt], "-fa", 5) == 0)
	{
	  nopt++;
	  if (nopt >= argc)  ANOVA_error ("need argument after -fa ");
	  option_data->nfa = 1;
	  option_data->faname = malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->faname, argv[nopt]);
	  nopt++;
	  continue;
	}
      
      
      /*-----   -fb filename   -----*/
      if (strncmp(argv[nopt], "-fb", 5) == 0)
	{
	  nopt++;
	  if (nopt >= argc)  ANOVA_error ("need argument after -fb ");
	  option_data->nfb = 1;
	  option_data->fbname = malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->fbname, argv[nopt]);
	  nopt++;
	  continue;
	}
      
      
      /*-----   -fab filename   -----*/
      if (strncmp(argv[nopt], "-fab", 5) == 0)
	{
	  nopt++;
	  if (nopt >= argc)  ANOVA_error ("need argument after -fab ");
	  option_data->nfab = 1;
	  option_data->fabname = malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->fabname, argv[nopt]);
	  nopt++;
	  continue;
	}
      
      
      /*-----   -amean level filename   -----*/
      if (strncmp(argv[nopt], "-amean", 5) == 0)
	{
	  nopt++;
	  if (nopt+1 >= argc)  ANOVA_error ("need 2 arguments after -amean ");
	  
	  option_data->num_ameans++;
	  if (option_data->num_ameans > MAX_LEVELS)
	    ANOVA_error ("too many factor A level mean estimates");
	  
	  sscanf (argv[nopt], "%d", &ival);
	  if ((ival <= 0) || (ival > option_data->a))
	    ANOVA_error ("illegal argument after -amean ");
	  option_data->ameans[option_data->num_ameans-1] = ival - 1;
	  nopt++;
	  
	  option_data->amname[option_data->num_ameans-1] 
	    =  malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->amname[option_data->num_ameans-1], argv[nopt]);
	  nopt++;
	  continue;
	}
      
      
      /*-----   -bmean level filename   -----*/
      if (strncmp(argv[nopt], "-bmean", 5) == 0)
	{
	  nopt++;
	  if (nopt+1 >= argc)  ANOVA_error ("need 2 arguments after -bmean ");
	  
	  option_data->num_bmeans++;
	  if (option_data->num_bmeans > MAX_LEVELS)
	    ANOVA_error ("too many factor B level mean estimates");
	  
	  sscanf (argv[nopt], "%d", &ival);
	  if ((ival <= 0) || (ival > option_data->b))
	    ANOVA_error ("illegal argument after -bmean ");
	  option_data->bmeans[option_data->num_bmeans-1] = ival - 1;
	  nopt++;
	  
	  option_data->bmname[option_data->num_bmeans-1] 
	    =  malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->bmname[option_data->num_bmeans-1], argv[nopt]);
	  nopt++;
	  continue;
	}


      /*-----   -adiff level1 level2 filename   -----*/
      if (strncmp(argv[nopt], "-adiff", 5) == 0)
	{
	  nopt++;
	  if (nopt+2 >= argc)  ANOVA_error ("need 3 arguments after -adiff ");
	  
	  option_data->num_adiffs++;
	  if (option_data->num_adiffs > MAX_DIFFS)
	    ANOVA_error ("too many factor A level differences");
	  
	  sscanf (argv[nopt], "%d", &ival);
	  if ((ival <= 0) || (ival > option_data->a))
	    ANOVA_error ("illegal argument after -adiff ");
	  option_data->adiffs[option_data->num_adiffs-1][0] = ival - 1;
	  nopt++;
	  
	  sscanf (argv[nopt], "%d", &ival);
	  if ((ival <= 0) || (ival > option_data->a))
	    ANOVA_error ("illegal argument after -adiff ");
	  option_data->adiffs[option_data->num_adiffs-1][1] = ival - 1;
	  nopt++;
	  
	  option_data->adname[option_data->num_adiffs-1] 
	    =  malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->adname[option_data->num_adiffs-1], argv[nopt]);
	  nopt++;
	  continue;
	}
      
      
      /*-----   -bdiff level1 level2 filename   -----*/
      if (strncmp(argv[nopt], "-bdiff", 5) == 0)
	{
	  nopt++;
	  if (nopt+2 >= argc)  ANOVA_error ("need 3 arguments after -bdiff ");
	  
	  option_data->num_bdiffs++;
	  if (option_data->num_bdiffs > MAX_DIFFS)
	    ANOVA_error ("too many factor B level differences");
	  
	  sscanf (argv[nopt], "%d", &ival);
	  if ((ival <= 0) || (ival > option_data->b))
	    ANOVA_error ("illegal argument after -bdiff ");
	  option_data->bdiffs[option_data->num_bdiffs-1][0] = ival - 1;
	  nopt++;
	  
	  sscanf (argv[nopt], "%d", &ival);
	  if ((ival <= 0) || (ival > option_data->b))
	    ANOVA_error ("illegal argument after -bdiff ");
	  option_data->bdiffs[option_data->num_bdiffs-1][1] = ival - 1;
	  nopt++;
	  
	  option_data->bdname[option_data->num_bdiffs-1] 
	    =  malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->bdname[option_data->num_bdiffs-1], argv[nopt]);
	  nopt++;
	  continue;
	}
      
      
      /*-----   -acontr c1 ... cr filename   -----*/
      if (strncmp(argv[nopt], "-acontr", 5) == 0)
	{
	  nopt++;
	  if (nopt + option_data->a >= argc)  
            ANOVA_error ("need a+1 arguments after -acontr ");
	  
	  option_data->num_acontr++;
	  if (option_data->num_acontr > MAX_CONTR)
	    ANOVA_error ("too many factor A level contrasts");
	  
	  for (i = 0;  i < option_data->a;  i++)
	    {
	      sscanf (argv[nopt], "%f", &fval); 
	      option_data->acontr[option_data->num_acontr - 1][i] = fval ;
	      nopt++;
	    }
	  
	  option_data->acname[option_data->num_acontr-1] 
	    =  malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->acname[option_data->num_acontr-1], argv[nopt]);
	  nopt++;
	  continue;
	}
      
      
      /*-----   -bcontr c1 ... cr filename   -----*/
      if (strncmp(argv[nopt], "-bcontr", 5) == 0)
	{
	  nopt++;
	  if (nopt + option_data->b >= argc)  
            ANOVA_error ("need b+1 arguments after -bcontr ");
	  
	  option_data->num_bcontr++;
	  if (option_data->num_bcontr > MAX_CONTR)
	    ANOVA_error ("too many factor B level contrasts");
	  	  
	  for (i = 0;  i < option_data->b;  i++)
	    {
	      sscanf (argv[nopt], "%f", &fval); 
	      option_data->bcontr[option_data->num_bcontr - 1][i] = fval ;
	      nopt++;
	    }
	  
	  option_data->bcname[option_data->num_bcontr-1] 
	    =  malloc (sizeof(char) * MAX_NAME_LENGTH);
	  strcpy (option_data->bcname[option_data->num_bcontr-1], argv[nopt]);
	  nopt++;
	  continue;
	}


      /*----- unknown command -----*/
      ANOVA_error ("unrecognized command line option ");
    }

  /*----- check that all treatment sample sizes are equal -----*/
  option_data->n = n[0][0];
  for (i = 0;  i < option_data->a;  i++)
    for (j = 0;  j < option_data->b;  j++)
      if (n[i][j] != option_data->n)
	ANOVA_error ("must have equal sample sizes for 3dANOVA2");
}


/*---------------------------------------------------------------------------*/
/*
   Routine to check whether temporary files already exist.
*/

void check_temporary_files (anova_options * option_data)
{

   check_one_temporary_file ("ss0");
   check_one_temporary_file ("ssi");
   check_one_temporary_file ("ssj");
   check_one_temporary_file ("ssij");
   check_one_temporary_file ("ssijk");

   check_one_temporary_file ("sse");
   check_one_temporary_file ("sstr");
   check_one_temporary_file ("ssa");
   check_one_temporary_file ("ssb");
   check_one_temporary_file ("ssab");
}


/*---------------------------------------------------------------------------*/
/*
   Routine to check whether output files already exist.
*/

void check_output_files (anova_options * option_data)
{
  int i;       /* index */

  if (option_data->nftr > 0)   
    check_one_output_file (option_data, option_data->ftrname);

  if (option_data->nfa > 0)   
    check_one_output_file (option_data, option_data->faname);

  if (option_data->nfb > 0)   
    check_one_output_file (option_data, option_data->fbname);

  if (option_data->nfab > 0)   
    check_one_output_file (option_data, option_data->fabname);

  if (option_data->num_ameans > 0)
    for (i = 0;  i < option_data->num_ameans;  i++)
      check_one_output_file (option_data, option_data->amname[i]);

  if (option_data->num_bmeans > 0)
    for (i = 0;  i < option_data->num_bmeans;  i++)
      check_one_output_file (option_data, option_data->bmname[i]);

  if (option_data->num_adiffs > 0)
    for (i = 0;  i < option_data->num_adiffs;  i++)
      check_one_output_file (option_data, option_data->adname[i]);

  if (option_data->num_bdiffs > 0)
    for (i = 0;  i < option_data->num_bdiffs;  i++)
      check_one_output_file (option_data, option_data->bdname[i]);

  if (option_data->num_acontr > 0)
    for (i = 0;  i < option_data->num_acontr;  i++)
      check_one_output_file (option_data, option_data->acname[i]);

  if (option_data->num_bcontr > 0)
    for (i = 0;  i < option_data->num_bcontr;  i++)
      check_one_output_file (option_data, option_data->bcname[i]);
}


/*---------------------------------------------------------------------------*/
/*
  Routine to check for valid inputs.
*/

void check_for_valid_inputs (anova_options * option_data)
{
   int a, b;                            /* number of factor levels */
   int n;                               /* number of observations per cell */


   /*----- initialize local variables  -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;


  /*----- check for valid inputs -----*/
   if (a < 2)  ANOVA_error ("must specify number of factor A levels (a>1) ");
   if (b < 2)  ANOVA_error ("must specify number of factor B levels (b>1) ");
   if (n < 1)  ANOVA_error ("sample size is too small");

   switch (option_data->model)
   {
      case 1:   /* fixed effects */  
	 if (n == 1)  
	    ANOVA_error ("sample size is too small for fixed effects model");
	 break;
      case 2:   /* random effects */
	 if (option_data->nftr > 0)
	    ANOVA_error ("-ftr is inappropriate for random effects model");
	 if (option_data->num_ameans > 0)
	    ANOVA_error ("-amean is inappropriate for random effects model");
	 if (option_data->num_bmeans > 0)
	    ANOVA_error ("-bmean is inappropriate for random effects model");
	 if (option_data->num_adiffs > 0)
	    ANOVA_error ("-adiff is inappropriate for random effects model");
	 if (option_data->num_bdiffs > 0)
	    ANOVA_error ("-bdiff is inappropriate for random effects model");
	 if (option_data->num_acontr > 0)
	    ANOVA_error ("-acontr is inappropriate for random effects model");
	 if (option_data->num_bcontr > 0)
	    ANOVA_error ("-bcontr is inappropriate for random effects model");
	 if ((n == 1) && (option_data->nfab > 0))
	    ANOVA_error ("sample size too small to calculate F-interaction");
	 break;
      case 3:   /* mixed effects */
	 if (option_data->nftr > 0)
	    ANOVA_error ("-ftr is inappropriate for mixed effects model");
	 if (option_data->num_bmeans > 0)
	    ANOVA_error ("-bmean is inappropriate for mixed effects model");
	 if (option_data->num_bdiffs > 0)
	    ANOVA_error ("-bdiff is inappropriate for mixed effects model");
	 if (option_data->num_bcontr > 0)
	    ANOVA_error ("-bcontr is inappropriate for mixed effects model");
	 if ((n == 1) && (option_data->nfab > 0))
	    ANOVA_error ("sample size too small to calculate F-interaction");
	 if ((n == 1) && (option_data->nfb > 0))
	    ANOVA_error ("sample size too small to calculate F for B effect");
	 break;
   } 

}


/*---------------------------------------------------------------------------*/
/*
  Routine to calculate the number of data files that have to be stored.
*/

int required_data_files (anova_options * option_data)
{
  int now;                         /* number of disk files just prior
				      to program termination */
  int nmax;                        /* maximum number of disk files */


  if (option_data->n != 1)
    {
      nmax = 6;  now = 5;
    }
  else
    {
      nmax = 5;  now = 5;
    }

  now = now + option_data->nftr + option_data->nfab
    + option_data->nfa + option_data->nfb 
    + option_data->num_ameans + option_data->num_bmeans 
    + option_data->num_adiffs + option_data->num_bdiffs 
    + option_data->num_acontr + option_data->num_bcontr;

  nmax = max (now, nmax);
  
  return (nmax);
}


/*---------------------------------------------------------------------------*/
/*
  Routine to perform all ANOVA initialization.
*/

void initialize (int argc,  char ** argv,  anova_options ** option_data)
{
  int i, j;                            /* factor level indices */
  int a, b;                            /* number of factor levels */
  int n;                               /* number of observations per cell */
  int nxyz;                            /* number of voxels */
  char message[MAX_NAME_LENGTH];       /* error message */
  
 
  /*----- allocate memory space for input data -----*/   
  *option_data = (anova_options *) malloc(sizeof(anova_options));
  if (*option_data == NULL)
    ANOVA_error ("memory allocation error");
  
  /*----- get command line inputs -----*/
  get_options(argc, argv, *option_data);

  /*----- use first data set to get data set dimensions -----*/
  (*option_data)->first_dataset = (*option_data)->xname[0][0][0][0];
  get_dimensions (*option_data);
  printf ("Data set dimensions:  nx = %d  ny = %d  nz = %d  nxyz = %d \n",
	  (*option_data)->nx, (*option_data)->ny,
	  (*option_data)->nz, (*option_data)->nxyz);
  if ((*option_data)->nvoxel > (*option_data)->nxyz)
    ANOVA_error ("argument of -voxel is too large");
  
  /*----- initialize local variables  -----*/
  a = (*option_data)->a;
  b = (*option_data)->b;
  n = (*option_data)->n;
  
  /*----- total number of observations -----*/
  (*option_data)->nt = n * a * b;
  
  /*----- check for valid inputs -----*/
  check_for_valid_inputs (*option_data);
  
  /*----- check whether temporary files already exist -----*/
  check_temporary_files (*option_data);
  
  /*----- check whether output files already exist -----*/
  check_output_files (*option_data);
  
  /*----- check whether there is sufficient disk space -----*/
  if ((*option_data)->diskspace)  check_disk_space (*option_data);
}


/*---------------------------------------------------------------------------*/
/*
  Routine to sum over the specified set of observations.
  The output is returned in ysum.
*/

void calculate_sum (anova_options * option_data,
		    int ii, int jj, float * ysum)
{
  float * y = NULL;                /* pointer to input data */
  int i, itop, ibot;               /* factor A level index */
  int j, jtop, jbot;               /* factor B level index */
  int m;                           /* observation number index */
  int a;                           /* number of levels for factor A */
  int b;                           /* number of levels for factor B */
  int n;                           /* number of observations per cell */
  int ixyz, nxyz;                  /* voxel counters */
  int nvoxel;                      /* output voxel # */
  char sum_label[MAX_NAME_LENGTH]; /* name of sum for print to screen */
  char str[MAX_NAME_LENGTH];       /* temporary string */
  
  
  /*----- initialize local variables -----*/
  a = option_data->a;
  b = option_data->b;
  n = option_data->n;
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  
  /*----- allocate memory space for calculations -----*/
  y = (float *) malloc(sizeof(float)*nxyz);
  if (y == NULL)  ANOVA_error ("unable to allocate sufficient memory");


  /*-----  set up summation limits -----*/
  if (ii < 0)
    { ibot = 0;   itop = a; }
  else
    { ibot = ii;  itop = ii+1; }

  if (jj < 0)
    { jbot = 0;   jtop = b; }
  else
    { jbot = jj;  jtop = jj+1; }


  volume_zero (ysum, nxyz);

  /*-----  loop over levels of factor A  -----*/
  for (i = ibot;  i < itop;  i++)
    {
      /*-----  loop over levels of factor B  -----*/
      for (j = jbot;  j < jtop;  j++)
	{
	  /*----- sum observations within this cell -----*/	     
	  for (m = 0;  m < n;  m++)
	    {  
	      read_afni_data (option_data, 
			      option_data->xname[i][j][0][m], y);
	      if (nvoxel > 0)
		printf ("y[%d][%d][%d] = %f \n", 
			i+1, j+1, m+1, y[nvoxel-1]);
	      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
		ysum[ixyz] += y[ixyz];
	    } /* m */
	}  /* j */ 
    }  /* i */


  /*----- print the sum for this cell -----*/
  if (nvoxel > 0)
    {
      strcpy (sum_label, "y");
      if (ii < 0)
	strcat (sum_label, "[.]");
      else
	{
	  sprintf (str, "[%d]", ii+1);
	  strcat (sum_label, str);
	}
      if (jj < 0)
	strcat (sum_label, "[.]");
      else
	{
	  sprintf (str, "[%d]", jj+1);
	  strcat (sum_label, str);
	}
      printf ("%s[.] = %f \n", sum_label, ysum[nvoxel-1]);
    }
 
  /*----- release memory -----*/
  free (y);     y = NULL;
  
}
  

/*---------------------------------------------------------------------------*/
/*
  Routine to calculate SS0.  Result is stored in temporary output file
  ss0.3danova2.
*/

void calculate_ss0 (anova_options * option_data)
{
  float * ss0 = NULL;              /* pointer to output data */
  float * ysum = NULL;             /* pointer to sum over all observations */
  int a;                           /* number of levels for factor A */
  int b;                           /* number of levels for factor B */
  int n;                           /* number of observations per cell */
  int ixyz, nxyz;                  /* voxel counters */
  int nvoxel;                      /* output voxel # */
  int nval;                        /* divisor of sum */
  char filename[MAX_NAME_LENGTH];  /* name of output file */
  
  
  /*----- initialize local variables -----*/
  a = option_data->a;
  b = option_data->b;
  n = option_data->n;
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  nval = a * b * n;
  
  /*----- allocate memory space for calculations -----*/
  ss0 = (float *) malloc(sizeof(float)*nxyz);
  ysum = (float *) malloc(sizeof(float)*nxyz);
  if ((ss0 == NULL) || (ysum == NULL))
    ANOVA_error ("unable to allocate sufficient memory");
  
  /*----- sum over all observations -----*/
  calculate_sum (option_data, -1, -1, ysum);

  /*----- calculate ss0 -----*/
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    ss0[ixyz] = ysum[ixyz] * ysum[ixyz] / nval;
  

  /*----- save the sum -----*/
  if (nvoxel > 0)
    printf ("SS0 = %f \n", ss0[nvoxel-1]); 
  strcpy (filename, "ss0");
  volume_write (filename, ss0, nxyz);
  

  /*----- release memory -----*/
  free (ysum);    ysum = NULL;
  free (ss0);     ss0 = NULL;
  
}

  
/*---------------------------------------------------------------------------*/
/*
  Routine to calculate SSI.  Result is stored in temporary output file
  ssi.3danova2.
*/

void calculate_ssi (anova_options * option_data)
{
  float * ssi = NULL;              /* pointer to output data */
  float * ysum = NULL;             /* pointer to sum over observations */
  int a;                           /* number of levels for factor A */
  int b;                           /* number of levels for factor B */
  int n;                           /* number of observations per cell */
  int i;                           /* index for factor A levels */
  int ixyz, nxyz;                  /* voxel counters */
  int nvoxel;                      /* output voxel # */
  int nval;                        /* divisor of sum */
  char filename[MAX_NAME_LENGTH];  /* name of output file */
  
  
  /*----- initialize local variables -----*/
  a = option_data->a;
  b = option_data->b;
  n = option_data->n;
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  nval = b * n;
  
  /*----- allocate memory space for calculations -----*/
  ssi = (float *) malloc(sizeof(float)*nxyz);
  ysum = (float *) malloc(sizeof(float)*nxyz);
  if ((ssi == NULL) || (ysum == NULL))
    ANOVA_error ("unable to allocate sufficient memory");
  
  volume_zero (ssi, nxyz);

  /*----- loop over levels of factor A -----*/
  for (i = 0;  i < a;  i++)
    {
      /*----- sum over observations -----*/
      calculate_sum (option_data, i, -1, ysum);

      /*----- add to ssi -----*/
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
	ssi[ixyz] += ysum[ixyz] * ysum[ixyz] / nval;
    }

  /*----- save the sum -----*/
  if (nvoxel > 0)
    printf ("SSI = %f \n", ssi[nvoxel-1]); 
  strcpy (filename, "ssi");
  volume_write (filename, ssi, nxyz);
  

  /*----- release memory -----*/
  free (ysum);    ysum = NULL;
  free (ssi);     ssi = NULL;
  
}

  
/*---------------------------------------------------------------------------*/
/*
  Routine to calculate SSJ.  Result is stored in temporary output file
  ssj.3danova2.
*/

void calculate_ssj (anova_options * option_data)
{
  float * ssj = NULL;              /* pointer to output data */
  float * ysum = NULL;             /* pointer to sum over observations */
  int a;                           /* number of levels for factor A */
  int b;                           /* number of levels for factor B */
  int n;                           /* number of observations per cell */
  int j;                           /* index for factor B levels */
  int ixyz, nxyz;                  /* voxel counters */
  int nvoxel;                      /* output voxel # */
  int nval;                        /* divisor of sum */
  char filename[MAX_NAME_LENGTH];  /* name of output file */
  
  
  /*----- initialize local variables -----*/
  a = option_data->a;
  b = option_data->b;
  n = option_data->n;
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  nval = a * n;
  
  /*----- allocate memory space for calculations -----*/
  ssj = (float *) malloc(sizeof(float)*nxyz);
  ysum = (float *) malloc(sizeof(float)*nxyz);
  if ((ssj == NULL) || (ysum == NULL))
    ANOVA_error ("unable to allocate sufficient memory");
  
  volume_zero (ssj, nxyz);

  /*----- loop over levels of factor B -----*/
  for (j = 0;  j < b;  j++)
    {
      /*----- sum over observations -----*/
      calculate_sum (option_data, -1, j, ysum);

      /*----- add to ssj -----*/
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
	ssj[ixyz] += ysum[ixyz] * ysum[ixyz] / nval;
    }

  /*----- save the sum -----*/
  if (nvoxel > 0)
    printf ("SSJ = %f \n", ssj[nvoxel-1]); 
  strcpy (filename, "ssj");
  volume_write (filename, ssj, nxyz);
  

  /*----- release memory -----*/
  free (ysum);    ysum = NULL;
  free (ssj);     ssj = NULL;
  
}

  
/*---------------------------------------------------------------------------*/
/*
  Routine to calculate SSIJ.  Result is stored in temporary output file
  ssij.3danova2.
*/

void calculate_ssij (anova_options * option_data)
{
  float * ssij = NULL;             /* pointer to output data */
  float * ysum = NULL;             /* pointer to sum over observations */
  int a;                           /* number of levels for factor A */
  int b;                           /* number of levels for factor B */
  int n;                           /* number of observations per cell */
  int i, j;                        /* indices for factor A and B levels */
  int ixyz, nxyz;                  /* voxel counters */
  int nvoxel;                      /* output voxel # */
  int nval;                        /* divisor of sum */
  char filename[MAX_NAME_LENGTH];  /* name of output file */
  
  
  /*----- initialize local variables -----*/
  a = option_data->a;
  b = option_data->b;
  n = option_data->n;
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  nval = n;
  
  /*----- allocate memory space for calculations -----*/
  ssij = (float *) malloc(sizeof(float)*nxyz);
  ysum = (float *) malloc(sizeof(float)*nxyz);
  if ((ssij == NULL) || (ysum == NULL))
    ANOVA_error ("unable to allocate sufficient memory");
  
  volume_zero (ssij, nxyz);

  /*----- loop over levels of factor A -----*/
  for (i = 0;  i < a;  i++)
    {
      /*----- loop over levels of factor B -----*/
      for (j = 0;  j < b;  j++)
	{
	  /*----- sum over observations -----*/
	  calculate_sum (option_data, i, j, ysum);
	  
	  /*----- add to ssij -----*/
	  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
	    ssij[ixyz] += ysum[ixyz] * ysum[ixyz] / nval;
	}
    }

  /*----- save the sum -----*/
  if (nvoxel > 0)
    printf ("SSIJ = %f \n", ssij[nvoxel-1]); 
  strcpy (filename, "ssij");
  volume_write (filename, ssij, nxyz);
  

  /*----- release memory -----*/
  free (ysum);    ysum = NULL;
  free (ssij);    ssij = NULL;
  
}

  
/*---------------------------------------------------------------------------*/
/*
  Routine to sum the squares of all observations.
  The sum is stored (temporarily) in disk file ssijk.3danova2.
*/

void calculate_ssijk (anova_options * option_data)
{
  float * ssijk = NULL;            /* pointer to output data */
  float * y = NULL;                 /* pointer to input data */
  int i;                            /* factor A level index */
  int j;                            /* factor B level index */
  int m;                            /* observation number index */
  int a;                            /* number of levels for factor A */
  int b;                            /* number of levels for factor B */
  int n;                            /* number of observations per cell */
  int ixyz, nxyz;                   /* voxel counters */
  int nvoxel;                       /* output voxel # */
  float yval;                       /* temporary float value */
  
  
  /*----- initialize local variables -----*/
  a = option_data->a;
  b = option_data->b;
  n = option_data->n;
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  
  /*----- allocate memory space for calculations -----*/
  ssijk = (float *) malloc(sizeof(float)*nxyz);
  y = (float *) malloc(sizeof(float)*nxyz);
  if ((ssijk == NULL) || (y == NULL))
    ANOVA_error ("unable to allocate sufficient memory");


  volume_zero (ssijk, nxyz);
  
  for (i = 0;  i < a;  i++)
    {
      for (j = 0;  j < b;  j++)
	{
	  for (m = 0;  m < n;  m++)
	    {
	      read_afni_data (option_data, 
			      option_data->xname[i][j][0][m], y);
	      
	      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
		ssijk[ixyz] += y[ixyz] * y[ixyz]; 
	    }
	}
    }
  
  
  /*----- save the sum -----*/  
  if (nvoxel > 0)
    printf ("SSIJK = %f \n", ssijk[nvoxel-1]); 
  volume_write ("ssijk", ssijk, nxyz);
  
  /*----- release memory -----*/
  free (y);       y = NULL;
  free (ssijk);   ssijk = NULL;
  
}


/*---------------------------------------------------------------------------*/
/*
  Routine to calculate the error sum of squares (SSE).
  The output is stored (temporarily) in file sse.3danova2.
*/

void calculate_sse (anova_options * option_data)
{
  float * y = NULL;                   /* input data pointer */
  float * sse = NULL;                 /* sse data pointer */
  int ixyz, nxyz;                     /* voxel counters */
  int nvoxel;                         /* output voxel # */
  
  
  /*----- assign local variables -----*/
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  
  /*----- allocate memory space for calculations -----*/
  sse = (float *) malloc (sizeof(float)*nxyz);
  y = (float *) malloc (sizeof(float)*nxyz);
  if ((y == NULL) || (sse == NULL))
    ANOVA_error ("unable to allocate sufficient memory");

 
  /*----- calculate SSE -----*/
  volume_read ("ssijk", sse, nxyz);

  volume_read ("ssij", y, nxyz);
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    sse[ixyz] -= y[ixyz]; 
  
  
  /*----- protection against round-off error -----*/
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    if (sse[ixyz] < 0.0)  sse[ixyz] = 0.0; 
  
  /*----- save error sum of squares -----*/
  if (nvoxel > 0)
    printf ("SSE = %f \n", sse[nvoxel-1]);
  volume_write ("sse", sse, nxyz);
  
  /*----- release memory -----*/
  free (y);      y = NULL;
  free (sse);    sse = NULL;
  
}


/*---------------------------------------------------------------------------*/
/*
  Routine to calculate the treatment sum of squares (SSTR).
  The output is stored (temporarily) in file sstr.3danova2.
*/

void calculate_sstr (anova_options * option_data)
{
  float * y = NULL;                   /* input data pointer */
  float * sstr = NULL;                /* sstr data pointer */
  int ixyz, nxyz;                     /* voxel counters */
  int nvoxel;                         /* output voxel # */
  
  
  /*----- assign local variables -----*/
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  
  /*----- allocate memory space for calculations -----*/
  sstr = (float *) malloc (sizeof(float)*nxyz);
  y = (float *) malloc (sizeof(float)*nxyz);
  if ((y == NULL) || (sstr == NULL))
    ANOVA_error ("unable to allocate sufficient memory");

 
  /*----- calculate SSTR -----*/
  volume_read ("ssij", sstr, nxyz);

  volume_read ("ss0", y, nxyz);
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    sstr[ixyz] -= y[ixyz]; 
  
  
  /*----- protection against round-off error -----*/
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    if (sstr[ixyz] < 0.0)  sstr[ixyz] = 0.0; 
  
  /*----- save error sum of squares -----*/
  if (nvoxel > 0)
    printf ("SSTR = %f \n", sstr[nvoxel-1]);
  volume_write ("sstr", sstr, nxyz);
  
  /*----- release memory -----*/
  free (y);      y = NULL;
  free (sstr);   sstr = NULL;
  
}


/*---------------------------------------------------------------------------*/
/*
  Routine to calculate the sum of squares due to factor A (SSA).
  The output is stored (temporarily) in file ssa.3danova2.
*/

void calculate_ssa (anova_options * option_data)
{
  float * y = NULL;                   /* input data pointer */
  float * ssa = NULL;                 /* output data pointer */
  int ixyz, nxyz;                     /* voxel counters */
  int nvoxel;                         /* output voxel # */


  /*----- assign local variables -----*/
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  
  /*----- allocate memory space for calculations -----*/
  ssa = (float *) malloc (sizeof(float)*nxyz);
  y = (float *) malloc (sizeof(float)*nxyz);
  if ((y == NULL) || (ssa == NULL))
    ANOVA_error ("unable to allocate sufficient memory");

 
  /*----- calculate SSA -----*/
  volume_read ("ssi", ssa, nxyz);

  volume_read ("ss0", y, nxyz);
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    ssa[ixyz] -= y[ixyz]; 
  
  
  /*----- protection against round-off error -----*/
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    if (ssa[ixyz] < 0.0)  ssa[ixyz] = 0.0; 
  
  /*----- save factor A sum of squares -----*/
  if (nvoxel > 0)
    printf ("SSA = %f \n", ssa[nvoxel-1]);
  volume_write ("ssa", ssa, nxyz);
  
  /*----- release memory -----*/
  free (y);     y = NULL;
  free (ssa);   ssa = NULL;
  
}


/*---------------------------------------------------------------------------*/
/*
  Routine to calculate the sum of squares due to factor B (SSB).
  The output is stored (temporarily) in file ssb.3danova2.
*/

void calculate_ssb (anova_options * option_data)
{
  float * y = NULL;                   /* input data pointer */
  float * ssb = NULL;                 /* output data pointer */
  int ixyz, nxyz;                     /* voxel counters */
  int nvoxel;                         /* output voxel # */


  /*----- assign local variables -----*/
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  
  /*----- allocate memory space for calculations -----*/
  ssb = (float *) malloc (sizeof(float)*nxyz);
  y = (float *) malloc (sizeof(float)*nxyz);
  if ((y == NULL) || (ssb == NULL))
    ANOVA_error ("unable to allocate sufficient memory");

 
  /*----- calculate SSB -----*/
  volume_read ("ssj", ssb, nxyz);

  volume_read ("ss0", y, nxyz);
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    ssb[ixyz] -= y[ixyz]; 
  
  
  /*----- protection against round-off error -----*/
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    if (ssb[ixyz] < 0.0)  ssb[ixyz] = 0.0; 
  
  /*----- save factor B sum of squares -----*/
  if (nvoxel > 0)
    printf ("SSB = %f \n", ssb[nvoxel-1]);
  volume_write ("ssb", ssb, nxyz);
  
  /*----- release memory -----*/
  free (y);     y = NULL;
  free (ssb);   ssb = NULL;
  
}


/*---------------------------------------------------------------------------*/
/*
  Routine to calculate the A*B interaction sum of squares (SSAB).
  The output is stored (temporarily) in file ssab.3danova2.
*/

void calculate_ssab (anova_options * option_data)
{
  float * y = NULL;                   /* input data pointer */
  float * ssab = NULL;                /* output data pointer */
  int ixyz, nxyz;                     /* voxel counters */
  int nvoxel;                         /* output voxel # */


  /*----- assign local variables -----*/
  nxyz = option_data->nxyz;
  nvoxel = option_data->nvoxel;
  
  /*----- allocate memory space for calculations -----*/
  ssab = (float *) malloc (sizeof(float)*nxyz);
  y = (float *) malloc (sizeof(float)*nxyz);
  if ((y == NULL) || (ssab == NULL))
    ANOVA_error ("unable to allocate sufficient memory");

 
  /*----- calculate SSAB -----*/
  volume_read ("sstr", ssab, nxyz);

  volume_read ("ssa", y, nxyz);
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    ssab[ixyz] -= y[ixyz];

  volume_read ("ssb", y, nxyz);
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    ssab[ixyz] -= y[ixyz];

  
  /*----- protection against round-off error -----*/
  for (ixyz = 0;  ixyz < nxyz;  ixyz++)
    if (ssab[ixyz] < 0.0)  ssab[ixyz] = 0.0; 
  
  /*----- save factor A*B sum of squares -----*/
  if (nvoxel > 0)
    printf ("SSAB = %f \n", ssab[nvoxel-1]);
  volume_write ("ssab", ssab, nxyz);
  
  /*----- release memory -----*/
  free (y);      y = NULL;
  free (ssab);   ssab = NULL;
  
}


/*---------------------------------------------------------------------------*/
/*
   Routine to calculate the F-statistic for treatment, F = MSTR / MSE,
      where MSTR = SSTR / (ab-1),
      and   MSE  = SSE  / (ab(n-1)).

   The output is stored as a 2 sub-brick AFNI data set.  The first sub-brick 
   contains the square root of MSTR (mean sum of squares due to treatment), 
   and the second sub-brick contains the corresponding F-statistic. 
*/

void calculate_ftr (anova_options * option_data)
{
   float * mstr = NULL;                 /* pointer to MSTR data */
   float * ftr = NULL;                  /* pointer to F due-to-treatment */
   int a;                               /* number of levels for factor A */
   int b;                               /* number of levels for factor B */
   int n;                               /* number of observations per cell */
   int ixyz, nxyz;                      /* voxel counters */
   int nvoxel;                          /* output voxel # */
   float fval;                          /* float value used in calculations */
   float mse;                           /* mean square error */
 


   /*----- initialize local variables -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;
   nxyz = option_data->nxyz;
   nvoxel = option_data->nvoxel;
   
   /*----- allocate memory space for calculations -----*/
   ftr = (float *) malloc(sizeof(float)*nxyz);
   mstr = (float *) malloc(sizeof(float)*nxyz);
   if ((ftr == NULL) || (mstr == NULL))
      ANOVA_error ("unable to allocate sufficient memory");

   /*----- calculate mean SS due to treatments -----*/
   volume_read ("sstr", mstr, nxyz); 
   fval = 1.0 / (a*b - 1.0); 
   for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      mstr[ixyz] = mstr[ixyz] * fval;     /*---   MSTR = SSTR / (ab-1)  ---*/
   if (nvoxel > 0)
      printf ("MSTR = %f \n", mstr[nvoxel-1]);
 
   /*----- calculate F-statistic    -----*/
   volume_read ("sse", ftr, nxyz); 
   fval = 1.0 / (a * b * (n-1));
   for (ixyz = 0;  ixyz < nxyz;  ixyz++)
     {
       mse = ftr[ixyz] * fval;            /*---  MSE = SSE / (ab(n-1))  ---*/
       ftr[ixyz] = mstr[ixyz] / mse;      /*---  F = MSTR / MSE  ---*/
     }
   if (nvoxel > 0)
      printf ("FTR = %f \n", ftr[nvoxel-1]);

   /*----- write out afni data file -----*/
   for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      mstr[ixyz] = sqrt(mstr[ixyz]);      /*-- mstr now holds square root --*/
   write_afni_data (option_data, option_data->ftrname, 
		    mstr, ftr, a*b-1, a*b*(n-1));

   /*----- this data file is no longer needed -----*/
   volume_delete ("sstr");

   /*----- release memory -----*/
   free (mstr);   mstr = NULL;
   free (ftr);    ftr = NULL;

}


/*---------------------------------------------------------------------------*/
/*
   Routine to calculate the F-statistic for factor A.

   For fixed effects model: 
      F = MSA / MSE,
         where MSA  = SSA / (a-1),
         and   MSE  = SSE / (ab(n-1)).

   For random effects model or mixed effects model:
      F = MSA / MSAB,
         where MSA  = SSA  / (a-1),
         and   MSAB = SSAB / ((a-1)(b-1)).

   The output is stored as a 2 sub-brick AFNI data set.  The first sub-brick 
   contains the square root of MSA (mean sum of squares due to factor A), 
   and the second sub-brick contains the corresponding F-statistic. 
*/

void calculate_fa (anova_options * option_data)
{
   float * msa = NULL;                  /* pointer to MSA data */
   float * fa = NULL;                   /* pointer to F due to factor A */
   int a;                               /* number of levels for factor A */
   int b;                               /* number of levels for factor B */
   int n;                               /* number of observations per cell */
   int ixyz, nxyz;                      /* voxel counters */
   int nvoxel;                          /* output voxel # */
   int numdf;                           /* numerator degrees of freedom */
   int dendf;                           /* denominator degrees of freedom */
   float mse;                           /* mean square error */
   float msab;                          /* mean square interaction */


   /*----- initialize local variables -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;
   nxyz = option_data->nxyz;
   nvoxel = option_data->nvoxel;
   
   /*----- allocate memory space for calculations -----*/
   fa = (float *) malloc(sizeof(float)*nxyz);
   msa = (float *) malloc(sizeof(float)*nxyz);
   if ((fa == NULL) || (msa == NULL))
      ANOVA_error ("unable to allocate sufficient memory");

   /*----- calculate mean SS due to factor A -----*/
   volume_read ("ssa", msa, nxyz); 
   numdf = a - 1; 
   for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      msa[ixyz] = msa[ixyz] / numdf;      /*---   MSA = SSA / (a-1)  ---*/
   if (nvoxel > 0)
      printf ("MSA = %f \n", msa[nvoxel-1]);

   /*----- calculate F-statistic    -----*/
   if (option_data->model == 1)
   {
      /*----- fixed effects model -----*/
      volume_read ("sse", fa, nxyz); 
      dendf = a * b * (n-1);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      {
          mse = fa[ixyz] / dendf;        /*---  MSE = SSE / (ab(n-1))  ---*/
          fa[ixyz] = msa[ixyz] / mse;    /*---  F = MSA / MSE  ---*/
      }
   }
   else
   {
      /*----- random or mixed effects model -----*/
      volume_read ("ssab", fa, nxyz);
      dendf = (a-1) * (b-1);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      {
          msab = fa[ixyz] / dendf;       /*---  MSAB = SSAB / (a-1)(b-1) ---*/
          fa[ixyz] = msa[ixyz] / msab;   /*---  F = MSA / MSAB  ---*/
      }
   }
      
   if (nvoxel > 0)
      printf ("FA = %f \n", fa[nvoxel-1]);

   /*----- write out afni data file -----*/
   for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      msa[ixyz] = sqrt(msa[ixyz]);        /*-- msa now holds square root --*/
   write_afni_data (option_data, option_data->faname, 
		    msa, fa, numdf, dendf);

   /*----- this data file is no longer needed -----*/
   volume_delete ("ssa");

   /*----- release memory -----*/
   free (msa);   msa = NULL;
   free (fa);    fa = NULL;

}


/*---------------------------------------------------------------------------*/
/*
   Routine to calculate the F-statistic for factor B.

   For fixed effects or mixed effects model: 
      F = MSB / MSE,
         where MSB  = SSB / (b-1),
         and   MSE  = SSE / (ab(n-1)).

   For random effects model:
      F = MSB / MSAB,
         where MSB  = SSB  / (b-1),
         and   MSAB = SSAB / ((a-1)(b-1)).

   The output is stored as a 2 sub-brick AFNI data set.  The first sub-brick 
   contains the square root of MSB (mean sum of squares due to factor B), 
   and the second sub-brick contains the corresponding F-statistic. 
*/

void calculate_fb (anova_options * option_data)
{
   float * msb = NULL;                  /* pointer to MSB data */
   float * fb = NULL;                   /* pointer to F due to factor B */
   int a;                               /* number of levels for factor A */
   int b;                               /* number of levels for factor B */
   int n;                               /* number of observations per cell */
   int ixyz, nxyz;                      /* voxel counters */
   int nvoxel;                          /* output voxel # */
   int numdf;                           /* numerator degrees of freedom */
   int dendf;                           /* denominator degrees of freedom */
   float mse;                           /* mean square error */
   float msab;                          /* mean square interaction */
 

   /*----- initialize local variables -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;
   nxyz = option_data->nxyz;
   nvoxel = option_data->nvoxel;
   
   /*----- allocate memory space for calculations -----*/
   fb = (float *) malloc(sizeof(float)*nxyz);
   msb = (float *) malloc(sizeof(float)*nxyz);
   if ((fb == NULL) || (msb == NULL))
      ANOVA_error ("unable to allocate sufficient memory");

   /*----- calculate mean SS due to factor B -----*/
   volume_read ("ssb", msb, nxyz); 
   numdf = b - 1; 
   for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      msb[ixyz] = msb[ixyz] / numdf;     /*---   MSB = SSB / (b-1)  ---*/
   if (nvoxel > 0)
      printf ("MSB = %f \n", msb[nvoxel-1]);

   /*----- calculate F-statistic    -----*/
   if ((option_data->model == 1) || (option_data->model == 3))
   {
      /*----- fixed effects model or mixed effects model -----*/
      volume_read ("sse", fb, nxyz); 
      dendf = a * b * (n-1);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      {
          mse = fb[ixyz] / dendf;        /*---  MSE = SSE / (ab(n-1))  ---*/
          fb[ixyz] = msb[ixyz] / mse;    /*---  F = MSB / MSE  ---*/
      }
   }
   else
   {
      /*----- random effects model -----*/
      volume_read ("ssab", fb, nxyz);
      dendf = (a-1) * (b-1);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      {
          msab = fb[ixyz] / dendf;       /*---  MSAB = SSAB / (a-1)(b-1) ---*/
          fb[ixyz] = msb[ixyz] / msab;   /*---  F = MSB / MSAB  ---*/
      }
   }
      
   if (nvoxel > 0)
      printf ("FB = %f \n", fb[nvoxel-1]);

   /*----- write out afni data file -----*/
   for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      msb[ixyz] = sqrt(msb[ixyz]);        /*-- msb now holds square root --*/
   write_afni_data (option_data, option_data->fbname, 
		    msb, fb, numdf, dendf);

   /*----- this data file is no longer needed -----*/
   volume_delete ("ssb");

   /*----- release memory -----*/
   free (msb);   msb = NULL;
   free (fb);    fb  = NULL;

}


/*---------------------------------------------------------------------------*/
/*
   Routine to calculate the F-statistic for interaction, F = MSAB / MSE,
      where MSAB = SSAB / ((a-1)(b-1)),
      and   MSE  = SSE  / (ab(n-1)).

   The output is stored as a 2 sub-brick AFNI data set.  The first sub-brick 
   contains the square root of MSAB (mean sum of squares due to interaction), 
   and the second sub-brick contains the corresponding F-statistic. 
*/

void calculate_fab (anova_options * option_data)
{
   float * msab = NULL;                 /* pointer to MSAB data */
   float * fab = NULL;                  /* pointer to F due to interaction */
   int a;                               /* number of levels for factor A */
   int b;                               /* number of levels for factor B */
   int n;                               /* number of observations per cell */
   int ixyz, nxyz;                      /* voxel counters */
   int nvoxel;                          /* output voxel # */
   float fval;                          /* float value used in calculations */
   float mse;                           /* mean square error */
 

   /*----- initialize local variables -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;
   nxyz = option_data->nxyz;
   nvoxel = option_data->nvoxel;
   
   /*----- allocate memory space for calculations -----*/
   fab = (float *) malloc(sizeof(float)*nxyz);
   msab = (float *) malloc(sizeof(float)*nxyz);
   if ((fab == NULL) || (msab == NULL))
      ANOVA_error ("unable to allocate sufficient memory");

   /*----- calculate mean SS due to interaction -----*/
   volume_read ("ssab", msab, nxyz); 
   fval = 1.0 / ((a - 1.0)*(b - 1.0)); 
   for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      msab[ixyz] = msab[ixyz] * fval;    /*---  MSAB = SSAB/((a-1)(b-1))  ---*/
   if (nvoxel > 0)
      printf ("MSAB = %f \n", msab[nvoxel-1]);

   /*----- calculate F-statistic    -----*/
   volume_read ("sse", fab, nxyz); 
   fval = 1.0 / (a * b * (n-1));
   for (ixyz = 0;  ixyz < nxyz;  ixyz++)
     {
       mse = fab[ixyz] * fval;          /*---  MSE = SSE / (ab(n-1))  ---*/
       fab[ixyz] = msab[ixyz] / mse;    /*---  F = MSAB / MSE  ---*/
     }
   if (nvoxel > 0)
      printf ("FAB = %f \n", fab[nvoxel-1]);

   /*----- write out afni data file -----*/
   for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      msab[ixyz] = sqrt(msab[ixyz]);      /*-- msab now holds square root --*/
   write_afni_data (option_data, option_data->fabname, 
		    msab, fab, (a-1)*(b-1), a*b*(n-1));

   /*----- this data file is no longer needed -----*/
   volume_delete ("ssab");

   /*----- release memory -----*/
   free (msab);   msab = NULL;
   free (fab);    fab  = NULL;

}


/*---------------------------------------------------------------------------*/
/*
   Routine to calculate the mean treatment effect for factor A at the user 
   specified treatment level.  The output is stored as a 2 sub-brick AFNI 
   data set. The first sub-brick contains the estimated mean at this treatment
   level, and the second sub-brick contains the corresponding t-statistic.
*/

void calculate_ameans (anova_options * option_data)
{
   const float  EPSILON = 1.0e-10;    /* protect against divide by zero */
   float * mean = NULL;               /* pointer to treatment mean data */
   float * tmean = NULL;              /* pointer to t-statistic data */
   int imean;                         /* output mean option index */
   int level;                         /* factor A level index */
   int j;                             /* factor B level index */
   int n;                             /* number of observations per cell */
   int ixyz, nxyz;                    /* voxel counters */
   int nvoxel;                        /* output voxel # */
   int a;                             /* number of levels for factor A */
   int b;                             /* number of levels for factor B */
   int nt;                            /* total number of observations */
   int num_means;                     /* number of user requested means */
   int df;                            /* degrees of freedom for t-test */
   float fval;                        /* for calculating std. dev. */
   float stddev;                      /* est. std. dev. of factor mean */
   char filename[MAX_NAME_LENGTH];    /* input data file name */
 

   /*----- initialize local variables -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;
   nt = option_data->nt;
   num_means = option_data->num_ameans;
   nxyz = option_data->nxyz;
   nvoxel = option_data->nvoxel;

   /*----- allocate memory space for calculations -----*/
   mean = (float *) malloc(sizeof(float)*nxyz);
   tmean = (float *) malloc(sizeof(float)*nxyz);
   if ((mean == NULL) || (tmean == NULL))  
      ANOVA_error ("unable to allocate sufficient memory");
   
   /*----- loop over user specified treatment means -----*/ 
   for (imean = 0;  imean < num_means;  imean++)
   {
      level = option_data->ameans[imean];
 
      /*----- estimate factor mean for this treatment level -----*/
      calculate_sum (option_data, level, -1, mean);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
	 mean[ixyz] = mean[ixyz] / (n*b);
      if (nvoxel > 0)
         printf ("Mean factor A level %d = %f \n", level+1, mean[nvoxel-1]);

      /*----- divide by estimated standard deviation of factor mean -----*/
      if (option_data->model == 1)
      {
   	 /*----- fixed effects model -----*/
         volume_read ("sse", tmean, nxyz); 
	 df = a*b*(n-1);
      }
      else
      {
	 /*----- mixed effects model -----*/
         volume_read ("ssab", tmean, nxyz); 
	 df = (a-1)*(b-1);
      }
      fval = (1.0 / df) * (1.0 / (b*n));
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      {
	 stddev =  sqrt(tmean[ixyz] * fval);
	 if (stddev < EPSILON)
	   tmean[ixyz] = 0.0;
	 else
	   tmean[ixyz] = mean[ixyz] / stddev;
      }
 
      if (nvoxel > 0)
         printf ("t for mean of factor A level %d = %f \n", 
		 level+1, tmean[nvoxel-1]);

      /*----- write out afni data file -----*/
      write_afni_data (option_data, option_data->amname[imean], 
                       mean, tmean, df, 0);

   }

      /*----- release memory -----*/
      free (mean);    mean = NULL;
      free (tmean);   tmean = NULL;
}

/*---------------------------------------------------------------------------*/
/*
   Routine to calculate the mean treatment effect for factor B at the user 
   specified treatment level.  The output is stored as a 2 sub-brick AFNI 
   data set. The first sub-brick contains the estimated mean at this treatment
   level, and the second sub-brick contains the corresponding t-statistic.
*/

void calculate_bmeans (anova_options * option_data)
{
   const float  EPSILON = 1.0e-10;    /* protect against divide by zero */
   float * mean = NULL;               /* pointer to treatment mean data */
   float * tmean = NULL;              /* pointer to t-statistic data */
   int imean;                         /* output mean option index */
   int level;                         /* factor B level index */
   int i;                             /* factor A level index */
   int n;                             /* number of observations per cell */
   int ixyz, nxyz;                    /* voxel counters */
   int nvoxel;                        /* output voxel # */
   int a;                             /* number of levels for factor A */
   int b;                             /* number of levels for factor B */
   int nt;                            /* total number of observations */
   int num_means;                     /* number of user requested means */
   float fval;                        /* for calculating std. dev. */
   float stddev;                      /* est. std. dev. of factor mean */
   char filename[MAX_NAME_LENGTH];    /* input data file name */
 

   /*----- initialize local variables -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;
   nt = option_data->nt;
   num_means = option_data->num_bmeans;
   nxyz = option_data->nxyz;
   nvoxel = option_data->nvoxel;

   /*----- allocate memory space for calculations -----*/
   mean = (float *) malloc(sizeof(float)*nxyz);
   tmean = (float *) malloc(sizeof(float)*nxyz);
   if ((mean == NULL) || (tmean == NULL))  
      ANOVA_error ("unable to allocate sufficient memory");
   
   /*----- loop over user specified treatment means -----*/ 
   for (imean = 0;  imean < num_means;  imean++)
   {
      level = option_data->bmeans[imean];

      /*----- estimate factor mean for this treatment level -----*/
      calculate_sum (option_data, -1, level, mean);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
	 mean[ixyz] = mean[ixyz] / (n*a);
      if (nvoxel > 0)
         printf ("Mean of factor B level %d = %f \n", level+1, mean[nvoxel-1]);

      /*----- divide by estimated standard deviation of factor mean -----*/
      volume_read ("sse", tmean, nxyz); 
      fval = (1.0 / (a*b*(n-1))) * (1.0 / (a*n));
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      {
	 stddev =  sqrt(tmean[ixyz] * fval);
	 if (stddev < EPSILON)
	   tmean[ixyz] = 0.0;
	 else
	   tmean[ixyz] = mean[ixyz] / stddev;
      }
      if (nvoxel > 0)
         printf ("t for mean of factor B level %d = %f \n", 
		 level+1, tmean[nvoxel-1]);

      /*----- write out afni data file -----*/
      write_afni_data (option_data, option_data->bmname[imean], 
                       mean, tmean, a*b*(n-1), 0);

   }

      /*----- release memory -----*/
      free (mean);    mean = NULL;
      free (tmean);   tmean = NULL;
}


/*---------------------------------------------------------------------------*/
/*
   Routine to estimate the difference in the means between two user specified
   treatment levels for factor A.  The output is a 2 sub-brick AFNI data set.
   The first sub-brick contains the estimated difference in the means.  
   The second sub-brick contains the corresponding t-statistic.
*/

void calculate_adifferences (anova_options * option_data)
{
   const float  EPSILON = 1.0e-10;     /* protect against divide by zero */
   float * diff = NULL;                /* pointer to est. diff. in means */
   float * tdiff = NULL;               /* pointer to t-statistic data */
   int a;                              /* number of levels for factor A */
   int b;                              /* number of levels for factor B */
   int ixyz, nxyz;                     /* voxel counters */
   int nvoxel;                         /* output voxel # */
   int num_diffs;                      /* number of user requested diffs. */
   int idiff;                          /* index for requested differences */
   int i, j;                           /* factor level indices */
   int n;                              /* number of observations per cell */
   int df;                             /* degrees of freedom for t-test */
   float fval;                         /* for calculating std. dev. */
   float stddev;                       /* est. std. dev. of difference */
   char filename[MAX_NAME_LENGTH];     /* input file name */


   /*----- initialize local variables -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;
   num_diffs = option_data->num_adiffs;
   nxyz = option_data->nxyz;
   nvoxel = option_data->nvoxel;
   
   /*----- allocate memory space for calculations -----*/
   diff = (float *) malloc(sizeof(float)*nxyz);
   tdiff = (float *) malloc(sizeof(float)*nxyz);
   if ((diff == NULL) || (tdiff == NULL))
      ANOVA_error ("unable to allocate sufficient memory");

   /*----- loop over user specified treatment differences -----*/
   for (idiff = 0;  idiff < num_diffs;  idiff++)
   {

      /*----- read first treatment level mean -----*/
      i = option_data->adiffs[idiff][0];
      calculate_sum (option_data, i, -1, diff);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
         diff[ixyz] = diff[ixyz] / (b*n);

      /*----- subtract second treatment level mean -----*/
      j = option_data->adiffs[idiff][1];
      calculate_sum (option_data, j, -1, tdiff);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
         diff[ixyz] -= tdiff[ixyz] / (b*n);
      if (nvoxel > 0)
         printf ("Difference of factor A level %d - level %d = %f \n", 
		 i+1, j+1, diff[nvoxel-1]);

      /*----- divide by estimated standard deviation of difference -----*/
      if (option_data->model == 1)
      {
   	 /*----- fixed effects model -----*/
         volume_read ("sse", tdiff, nxyz); 
	 df = a*b*(n-1);
      }
      else
      {
	 /*----- mixed effects model -----*/
         volume_read ("ssab", tdiff, nxyz); 
	 df = (a-1)*(b-1);
      }
      fval = (1.0 / df) * (2.0 / (b*n));
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
	{
	  stddev = sqrt (tdiff[ixyz] * fval);
	  if (stddev < EPSILON)
	    tdiff[ixyz] = 0.0;
	  else
	    tdiff[ixyz] = diff[ixyz] / stddev;
	} 
          
      if (nvoxel > 0)
         printf ("t for difference of factor A level %d - level %d = %f \n", 
		 i+1, j+1, tdiff[nvoxel-1]);

      /*----- write out afni data file -----*/
      write_afni_data (option_data, option_data->adname[idiff], 
                       diff, tdiff, df, 0);

   }

   /*----- release memory -----*/
   free (diff);    diff = NULL;
   free (tdiff);   tdiff = NULL;

}


/*---------------------------------------------------------------------------*/
/*
   Routine to estimate the difference in the means between two user specified
   treatment levels for factor B.  The output is a 2 sub-brick AFNI data set.
   The first sub-brick contains the estimated difference in the means.  
   The second sub-brick contains the corresponding t-statistic.
*/

void calculate_bdifferences (anova_options * option_data)
{
   const float  EPSILON = 1.0e-10;     /* protect against divide by zero */
   float * diff = NULL;                /* pointer to est. diff. in means */
   float * tdiff = NULL;               /* pointer to t-statistic data */
   int a;                              /* number of levels for factor A */
   int b;                              /* number of levels for factor B */
   int ixyz, nxyz;                     /* voxel counters */
   int nvoxel;                         /* output voxel # */
   int num_diffs;                      /* number of user requested diffs. */
   int idiff;                          /* index for requested differences */
   int i, j;                           /* factor level indices */
   int n;                              /* number of observations per cell */
   float fval;                         /* for calculating std. dev. */
   float stddev;                       /* est. std. dev. of difference */
   char filename[MAX_NAME_LENGTH];     /* input file name */


   /*----- initialize local variables -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;
   num_diffs = option_data->num_bdiffs;
   nxyz = option_data->nxyz;
   nvoxel = option_data->nvoxel;
   
   /*----- allocate memory space for calculations -----*/
   diff = (float *) malloc(sizeof(float)*nxyz);
   tdiff = (float *) malloc(sizeof(float)*nxyz);
   if ((diff == NULL) || (tdiff == NULL))
      ANOVA_error ("unable to allocate sufficient memory");

   /*----- loop over user specified treatment differences -----*/
   for (idiff = 0;  idiff < num_diffs;  idiff++)
   {

      /*----- read first treatment level mean -----*/
      i = option_data->bdiffs[idiff][0];
      calculate_sum (option_data, -1, i, diff);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
         diff[ixyz] = diff[ixyz] / (a*n);

      /*----- subtract second treatment level mean -----*/
      j = option_data->bdiffs[idiff][1];
      calculate_sum (option_data, -1, j, tdiff);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
         diff[ixyz] -= tdiff[ixyz] / (a*n);
      if (nvoxel > 0)
         printf ("Difference of factor B level %d - level %d = %f \n", 
		 i+1, j+1, diff[nvoxel-1]);

      /*----- divide by estimated standard deviation of difference -----*/
      volume_read ("sse", tdiff, nxyz); 
      fval = (1.0 / (a*b*(n-1))) * (2.0 / (a*n));
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
	{
	  stddev = sqrt (tdiff[ixyz] * fval);
	  if (stddev < EPSILON)
	    tdiff[ixyz] = 0.0;
	  else
	    tdiff[ixyz] = diff[ixyz] / stddev;
	} 

      if (nvoxel > 0)
         printf ("t for difference of factor B level %d - level %d = %f \n", 
		 i+1, j+1, tdiff[nvoxel-1]);

      /*----- write out afni data file -----*/
      write_afni_data (option_data, option_data->bdname[idiff], 
                       diff, tdiff, a*b*(n-1), 0);

   }

   /*----- release memory -----*/
   free (diff);    diff = NULL;
   free (tdiff);   tdiff = NULL;

}


/*---------------------------------------------------------------------------*/
/*
   Routine to estimate a user specified contrast in treatment levels for
   factor A.  The output is stored as a 2 sub-brick AFNI data set.  The first
   sub-brick contains the estimated contrast.  The second sub-brick contains 
   the corresponding t-statistic.
*/

void calculate_acontrasts (anova_options * option_data)
{
   const float  EPSILON = 1.0e-10;     /* protect against divide by zero */
   float * contr = NULL;               /* pointer to contrast estimate */
   float * tcontr = NULL;              /* pointer to t-statistic data */
   int a;                              /* number of levels for factor A */
   int b;                              /* number of levels for factor B */
   int ixyz, nxyz;                     /* voxel counters */
   int nvoxel;                         /* output voxel # */
   int num_contr;                      /* number of user requested contrasts */
   int icontr;                         /* index of user requested contrast */
   int level;                          /* factor level index */
   int n;                              /* number of observations per cell */
   int df;                             /* degrees of freedom for t-test */
   float fval;                         /* for calculating std. dev. */
   float c;                            /* contrast coefficient */
   float stddev;                       /* est. std. dev. of contrast */
   char filename[MAX_NAME_LENGTH];     /* input data file name */


   /*----- initialize local variables -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;
   num_contr = option_data->num_acontr;
   nxyz = option_data->nxyz;
   nvoxel = option_data->nvoxel;
   
   /*----- allocate memory space for calculations -----*/
   contr  = (float *) malloc(sizeof(float)*nxyz);
   tcontr = (float *) malloc(sizeof(float)*nxyz);
   if ((contr == NULL) || (tcontr == NULL))
      ANOVA_error ("unable to allocate sufficient memory");


   /*----- loop over user specified constrasts -----*/
   for (icontr = 0;  icontr < num_contr;  icontr++)
   {
      volume_zero (contr, nxyz);
      fval = 0.0;
 
      for (level = 0;  level < a;  level++)
      {
	 c = option_data->acontr[icontr][level]; 
	 if (c == 0.0) continue; 
    
	 /*----- add c * treatment level mean to contrast -----*/
	 calculate_sum (option_data, level, -1, tcontr);
         fval += c * c / (b*n);
	 for (ixyz = 0;  ixyz < nxyz;  ixyz++)
	    contr[ixyz] += c * tcontr[ixyz] / (b*n);
      }
      if (nvoxel > 0)
	 printf ("No.%d contrast for factor A = %f \n", 
		 icontr+1, contr[nvoxel-1]);

      /*----- standard deviation depends on model type -----*/
      if (option_data->model == 1)
      {
   	 /*----- fixed effects model -----*/
         volume_read ("sse", tcontr, nxyz); 
	 df = a*b*(n-1);
      }
      else
      {
	 /*----- mixed effects model -----*/
         volume_read ("ssab", tcontr, nxyz); 
	 df = (a-1)*(b-1);
      }

      /*----- divide by estimated standard deviation of the contrast -----*/
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      {
	 stddev = sqrt ((tcontr[ixyz] / df) * fval);
	 if (stddev < EPSILON)
	   tcontr[ixyz] = 0.0;
	 else
	   tcontr[ixyz] = contr[ixyz] / stddev;
      }   
   
      if (nvoxel > 0)
  	 printf ("t of No.%d contrast for factor A = %f \n", 
		 icontr+1, tcontr[nvoxel-1]);

      /*----- write out afni data file -----*/
      write_afni_data (option_data, option_data->acname[icontr], 
                       contr, tcontr, df, 0);

   }

   /*----- release memory -----*/
   free (contr);    contr = NULL;
   free (tcontr);   tcontr = NULL;

}

/*---------------------------------------------------------------------------*/
/*
   Routine to estimate a user specified contrast in treatment levels for
   factor B.  The output is stored as a 2 sub-brick AFNI data set.  The first
   sub-brick contains the estimated contrast.  The second sub-brick contains 
   the corresponding t-statistic.
*/

void calculate_bcontrasts (anova_options * option_data)
{
   const float  EPSILON = 1.0e-10;     /* protect against divide by zero */
   float * contr = NULL;               /* pointer to contrast estimate */
   float * tcontr = NULL;              /* pointer to t-statistic data */
   int a;                              /* number of levels for factor A */
   int b;                              /* number of levels for factor B */
   int ixyz, nxyz;                     /* voxel counters */
   int nvoxel;                         /* output voxel # */
   int num_contr;                      /* number of user requested contrasts */
   int icontr;                         /* index of user requested contrast */
   int level;                          /* factor level index */
   int df;                             /* degrees of freedom for t-test */
   int n;                              /* number of observations per cell */
   float fval;                         /* for calculating std. dev. */
   float c;                            /* contrast coefficient */
   float stddev;                       /* est. std. dev. of contrast */
   char filename[MAX_NAME_LENGTH];     /* input data file name */


   /*----- initialize local variables -----*/
   a = option_data->a;
   b = option_data->b;
   n = option_data->n;
   num_contr = option_data->num_bcontr;
   nxyz = option_data->nxyz;
   nvoxel = option_data->nvoxel;
   
   /*----- allocate memory space for calculations -----*/
   contr  = (float *) malloc(sizeof(float)*nxyz);
   tcontr = (float *) malloc(sizeof(float)*nxyz);
   if ((contr == NULL) || (tcontr == NULL))
      ANOVA_error ("unable to allocate sufficient memory");


   /*----- loop over user specified constrasts -----*/
   for (icontr = 0;  icontr < num_contr;  icontr++)
   {
      volume_zero (contr, nxyz);
      fval = 0.0;
      
      for (level = 0;  level < b;  level++)
      {
	 c = option_data->bcontr[icontr][level]; 
	 if (c == 0.0) continue; 
    
	 /*----- add c * treatment level mean to contrast -----*/
	 calculate_sum (option_data, -1, level, tcontr);
         fval += c * c / (a*n);
	 for (ixyz = 0;  ixyz < nxyz;  ixyz++)
	    contr[ixyz] += c * tcontr[ixyz] / (a*n);
      }
      if (nvoxel > 0)
	 printf ("No.%d contrast for factor B = %f \n", 
		 icontr+1, contr[nvoxel-1]);

      /*----- divide by estimated standard deviation of the contrast -----*/
      volume_read ("sse", tcontr, nxyz);
      df = a * b * (n-1);
      for (ixyz = 0;  ixyz < nxyz;  ixyz++)
      {
	  stddev = sqrt ((tcontr[ixyz] / df) * fval);
	  if (stddev < EPSILON)
	    tcontr[ixyz] = 0.0;
	  else
	    tcontr[ixyz] = contr[ixyz] / stddev;
      }   

      if (nvoxel > 0)
	printf ("t of No.%d contrast for factor B = %f \n", 
		icontr+1, tcontr[nvoxel-1]);

      /*----- write out afni data file -----*/
      write_afni_data (option_data, option_data->bcname[icontr], 
                       contr, tcontr, a*b*(n-1), 0);

   }

   /*----- release memory -----*/
   free (contr);    contr = NULL;
   free (tcontr);   tcontr = NULL;

}


/*---------------------------------------------------------------------------*/
/*
   Routine to calculate sums and sums of squares for two-factor ANOVA.
*/

void calculate_anova (anova_options * option_data)
{

  /*----- calculate various sum and sums of squares -----*/
  calculate_ss0 (option_data);
  calculate_ssi (option_data);
  calculate_ssj (option_data);
  calculate_ssij (option_data);
  if (option_data->n != 1)  calculate_ssijk (option_data);


  /*-----  calculate error sum of squares  -----*/
  if (option_data->n != 1)  
    {
      calculate_sse (option_data);
      volume_delete ("ssijk");
    }
  
  /*-----  calculate treatment sum of squares -----*/
  calculate_sstr (option_data);
  volume_delete ("ssij");

  /*-----  calculate sum of squares due to A effect  -----*/
  calculate_ssa (option_data);
  volume_delete ("ssi");
  
  /*-----  calculate sum of squares due to B effect  -----*/
  calculate_ssb (option_data);
  volume_delete ("ssj");

  volume_delete ("ss0");
  
  /*-----  calculate sum of squares due to A*B interaction  -----*/
  calculate_ssab (option_data);
  
}


/*---------------------------------------------------------------------------*/
/*
   Routine to analyze the results from a two-factor ANOVA.
*/

void analyze_results (anova_options * option_data)
{
  
   /*-----  calculate F-statistic for treatment effect  -----*/
   if (option_data->nftr > 0)  calculate_ftr (option_data);

   /*-----  calculate F-statistic for factor A effect  -----*/
   if (option_data->nfa > 0)  calculate_fa (option_data);

   /*-----  calculate F-statistic for factor B effect  -----*/
   if (option_data->nfb > 0)  calculate_fb (option_data);

   /*-----  calculate F-statistic for interaction effect  -----*/
   if (option_data->nfab > 0)  calculate_fab (option_data);

   /*-----  estimate level means for factor A  -----*/
   if (option_data->num_ameans > 0)  calculate_ameans (option_data);

   /*-----  estimate level means for factor B  -----*/
   if (option_data->num_bmeans > 0)  calculate_bmeans (option_data);

   /*-----  estimate level differences for factor A  -----*/
   if (option_data->num_adiffs > 0)  calculate_adifferences (option_data);

   /*-----  estimate level differences for factor B  -----*/
   if (option_data->num_bdiffs > 0)  calculate_bdifferences (option_data);

   /*-----  estimate level contrasts for factor A  -----*/
   if (option_data->num_acontr > 0)  calculate_acontrasts (option_data);

   /*-----  estimate level contrasts for factor B  -----*/
   if (option_data->num_bcontr > 0)  calculate_bcontrasts (option_data);

}


/*---------------------------------------------------------------------------*/
/*
   Routine to release memory and remove any remaining temporary data files.
*/

void terminate (anova_options * option_data)
{
   int i, j, k;
   char filename[MAX_NAME_LENGTH];

   /*----- deallocate memory -----*/
   for (i = 0;  i < option_data->a;  i++)
      for (j = 0;  j < option_data->b;  j++)
	 for (k = 0;  k < option_data->n;  k++)
	 {
	    free (option_data->xname[i][j][0][k]);
	    option_data->xname[i][j][0][k] = NULL;
	 }

   if (option_data->nftr != 0)   
   {
      free (option_data->ftrname);
      option_data->ftrname = NULL;
   }
   if (option_data->nfa != 0)    
   {
      free (option_data->faname);
      option_data->faname = NULL;
   }
   if (option_data->nfb != 0)
   {
      free (option_data->fbname);
      option_data->fbname = NULL;
   }
   if (option_data->nfab != 0)
   {
      free (option_data->fabname);
      option_data->fabname = NULL;
   }

   if (option_data->num_ameans > 0)
     for (i=0; i < option_data->num_ameans; i++)  
       {
	 free (option_data->amname[i]);
	 option_data->amname[i] = NULL;
       }

   if (option_data->num_bmeans > 0)
     for (i=0; i < option_data->num_bmeans; i++)  
       {
	 free (option_data->bmname[i]);
	 option_data->bmname[i] = NULL;
       }

   if (option_data->num_adiffs > 0)
     for (i=0; i < option_data->num_adiffs; i++)  
       {
	 free (option_data->adname[i]);
	 option_data->adname[i] = NULL;
       }

   if (option_data->num_bdiffs > 0)
     for (i=0; i < option_data->num_bdiffs; i++)  
       {
	 free (option_data->bdname[i]);
	 option_data->bdname[i] = NULL;
       }

   if (option_data->num_acontr > 0)
     for (i=0; i < option_data->num_acontr; i++)  
       {
	 free (option_data->acname[i]);
	 option_data->acname[i] = NULL;
       }

   if (option_data->num_bcontr > 0)
     for (i=0; i < option_data->num_bcontr; i++)  
       {
	 free (option_data->bcname[i]);
	 option_data->bcname[i] = NULL;
       }

   /*----- remove temporary data files -----*/
   volume_delete ("sstr");
   volume_delete ("sse");
   volume_delete ("ssa");
   volume_delete ("ssb");
   volume_delete ("ssab");
   for (i = 0;  i < option_data->a;  i++)
   {
      sprintf (filename, "ya.%d", i);
      volume_delete (filename);
   }
   for (j = 0;  j < option_data->b;  j++)
   {
      sprintf (filename, "yb.%d", j);
      volume_delete (filename);
   }

}


/*---------------------------------------------------------------------------*/
/*
  Two- factor analysis of variance (ANOVA).
*/

void main (int argc, char ** argv)
{
   anova_options * option_data = NULL;
 
   printf ("\n\nProgram 3dANOVA2 \n\n");
   printf ("Last revision: %s \n", LAST_MOD_DATE);

   /*----- program initialization -----*/
   initialize (argc, argv, &option_data);

   /*----- calculate sums of squares -----*/
   calculate_anova (option_data);

   /*----- generate requested output -----*/
   analyze_results (option_data);

   /*----- terminate program -----*/
   terminate (option_data);
   free (option_data);   option_data = NULL;

}

















