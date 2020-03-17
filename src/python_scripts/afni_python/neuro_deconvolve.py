#!/usr/bin/env python

import sys
from afnipy import option_list, afni_util as UTIL, afni_base as BASE
from afnipy import lib_afni1D as LD

g_help_string = """
===========================================================================
neuro_deconvolve.py:

Generate a script that would apply 3dTfitter to deconvolve an MRI signal
(BOLD response curve) into a neuro response curve.

Required parameters include an input dataset, a script name and an output
prefix.

----------------------------------------------------------------------
examples:

    1. deconvolve 3 seed time series

        The errts time series might be applied to the model, while the
        all_runs and fitts and for evaluation, along with the re-convolved
        time series generated by the script.

        Temporal partitioning is on the todo list.

           neuro_deconvolve.py                                      \\
              -infiles seed.all_runs.1D seed.errts.1D seed.fitts.1D \\
              -tr 2.0 -tr_nup 20 -kernel BLOCK                      \\
              -script script.neuro.txt

old examples:

    old 1. 3d+time example

           neuro_deconvolve.py         \\
              -input run1+orig         \\
              -script script.neuro     \\
              -mask_dset automask+orig \\
              -prefix neuro_resp

    old 2. 1D example

           neuro_deconvolve.py        \\
              -input epi_data.1D      \\
              -tr 2.0                 \\
              -script script.1d       \\
              -prefix neuro.1D


----------------------------------------------------------------------
informational arguments:

    -help                       : display this help
    -hist                       : display the modification history
    -show_valid_opts            : display all valid options (short format)
    -ver                        : display the version number

----------------------------------------
required arguments:

    -input INPUT_DATASET        : set the data to deconvolve

        e.g. -input epi_data.1D

    -prefix PREFIX              : set the prefix for output filenames

        e.g. -prefix neuro_resp

                --> might create: neuro_resp+orig.HEAD/.BRIK

    -script SCRIPT              : specify the name of the output script

        e.g. -script neuro.script

----------------------------------------
optional arguments:


    -kernel KERNEL              : set the response kernel

        default: -kernel GAM

    -kernel_file FILENAME       : set the filename to store the kernel in

        default: -kernel_file resp_kernel.1D

      * This data should be at the upsampled TR.

        See -tr_nup.

    -mask_dset DSET             : set a mask dataset for 3dTfitter to use

        e.g. -mask_dset automask+orig

    -old                        : make old-style script

        Make pre-2015.02.24 script for 1D case.

    -tr TR                      : set the scanner TR

        e.g. -tr 2.0

        The TR is needed for 1D formatted input files.  It is not needed
        for AFNI 3d+time datasets, since the TR is in the file.

    -tr_nup NUP                 : upsample factor for TR

        e.g. -tr_nup 25

        Deconvolution is generally done on an upsampled TR, which allows
        for sub-TR events and more accurate deconvolution.  NUP should be
        the number of pieces each original TR is divided into.  For example,
        to upsample a TR of 2.0 to one of 0.1, use NUP = 20.

        TR must be an integral multiple of TR_UP.

    -verb LEVEL                 : set the verbose level

        e.g. -verb 2


- R Reynolds  June 12, 2008
===========================================================================
"""

g_history = """
    neuro_deconvolve.py history:

    0.1  Jun 12, 2008: initial version
    0.2  Feb 24, 2015:
         - new method for deconvolution for multiple files
         - added reconvolution for comparison
         - replaced -input with -infiles
         - added options -old, -infiles, -tr_nup
    0.3  Jun 08, 2015: allow infiles to include paths
"""

g_version = "version 0.3, June 8, 2015"

g_todo = """
   - temporal partitioning based on stimulus timing and durations
   - handle multiple runs
   - maybe handle other kernel
"""

gDEF_VERB       = 1      # default verbose level

class Decon:
    def __init__(self, label):
        # general parameters
        self.label      = label
        self.verb       = gDEF_VERB
        self.valid_opts = None                  # OptionList
        self.user_opts  = None                  # OptionList
        self.aname      = None                  # afni_name from input[0]
        self.stype      = 'cur'

        # required user parameters
        self.infiles    = []                    # input dataset
        self.prefix     = 'test'                # for script output
        self.script     = 'script.neuro.txt'    # script name
        self.outdir     = 'decon.results'       # output directory

        # optional parameters
        self.kernel     = 'GAM'                 # response kernel
        self.kfile_in   = None                  # pre-generated kernel file
        self.kfile      = 'resp_kernel.1D'      # file for response kernel
        self.maskset    = None                  # mask dataset
        self.tr         = 1.0                   # TR for curve and dataset
        self.tr_nup     = 1                     # upsample factor

        # computed
        self.trup       = 1.0
        self.locfiles   = []                    # input files, no directory

    def init_opts(self):
        global g_help_string
        self.valid_opts = option_list.OptionList('for input')

        # short, terminal arguments
        self.valid_opts.add_opt('-help', 0, [],      \
                        helpstr='display program help')
        self.valid_opts.add_opt('-hist', 0, [],      \
                        helpstr='display the modification history')
        self.valid_opts.add_opt('-show_valid_opts', 0, [], \
                        helpstr='display all valid options')
        self.valid_opts.add_opt('-ver', 0, [],       \
                        helpstr='display the current version number')

        # required arguments
        self.valid_opts.add_opt('-infiles', -1, [], req=1,
                        helpstr='input files (containing BOLD curve)')

        # optional arguments
        self.valid_opts.add_opt('-kernel', 1, [],
                        helpstr='response kernel [GAM])');

        self.valid_opts.add_opt('-kernel_file', 1, [],
                        helpstr='file for reponse kernel [resp_kernel.1D]');

        self.valid_opts.add_opt('-mask_dset', 1, [],
                        helpstr='computational mask dataset');

        self.valid_opts.add_opt('-old', 0, [],
                        helpstr='generate script with old method');

        self.valid_opts.add_opt('-outdir', 1, [],
                        helpstr='specify output directory');

        self.valid_opts.add_opt('-prefix', 1, [],
                        helpstr='prefix for deconvolved dataset');

        self.valid_opts.add_opt('-script', 1, [], req=1,
                        helpstr='output script filename')

        self.valid_opts.add_opt('-tr', 1, [],
                        helpstr='specify TR for 3dDeconvolve command');
        self.valid_opts.add_opt('-tr_nup', 1, [],
                        helpstr='upsample TR factor, e.g. 20 (=2.0/0.1)');

        self.valid_opts.add_opt('-verb', 1, [],
                        helpstr='verbose level (0=quiet, 1=default, ...)')


    def read_opts(self):
        """check for terminal arguments, then read the user options"""

        # process any optlist_ options
        self.valid_opts.check_special_opts(sys.argv)

        # ------------------------------------------------------------
        # check for terminal args in argv (to ignore required args)

        # if argv has only the program name, or user requests help, show it
        if len(sys.argv) <= 1 or '-help' in sys.argv:
            print g_help_string
            return 0

        if '-hist' in sys.argv:
            print g_history
            return 0

        if '-show_valid_opts' in sys.argv:      # show all valid options
            self.valid_opts.show('', 1)
            return 0

        if '-ver' in sys.argv:
            print g_version
            return 0

        # ------------------------------------------------------------
        # now read user options

        self.user_opts = option_list.read_options(sys.argv, self.valid_opts)
        if not self.user_opts: return 1         # error condition

        return None     # normal completion

    def process_opts(self):
        """apply each option"""

        # ----------------------------------------
        # set verb first
        self.verb,err = self.user_opts.get_type_opt(int, '-verb')
        if err: return 1
        if self.verb == None: self.verb = gDEF_VERB

        # ----------------------------------------
        # required args

        self.infiles, err = self.user_opts.get_string_list('-infiles')
        if self.infiles == None or err: return 1

        # ----------------------------------------
        # optional arguments

        val, err = self.user_opts.get_string_opt('-kernel')
        if err: return 1
        if val != None: self.kernel = val

        val, err = self.user_opts.get_string_opt('-kernel_file')
        if err: return 1
        if val != None: self.kfile = val

        val, err = self.user_opts.get_string_opt('-mask_dset')
        if err: return 1
        if val != None: self.maskset = val

        # allow use of old method
        if self.user_opts.find_opt('-old'): self.stype = 'old'

        val, err = self.user_opts.get_string_opt('-outdir')
        if err: return 1
        if val != None: self.outdir = val

        val, err = self.user_opts.get_string_opt('-prefix')
        if err: return 1
        if val != None: self.prefix = val

        self.script, err = self.user_opts.get_string_opt('-script')
        if self.script == None or err: return 1

        val,err = self.user_opts.get_type_opt(float, '-tr')
        if err: return 1
        if val != None: self.tr = val

        val,err = self.user_opts.get_type_opt(int, '-tr_nup')
        if err: return 1
        if val != None: self.tr_nup = val

        # ----------------------------------------
        # check over the inputs

        if len(self.infiles) < 1:
           print '** missing option -infiles'
           return 1

        # check over -input as an AFNI dataset
        self.aname = BASE.afni_name(self.infiles[0])
        if self.aname == None: return 1
        if self.verb > 2: self.aname.show()

        if self.aname.type == '1D':
            if self.tr == None:
                print '** -tr is required if the input is in 1D format'
            self.reps = UTIL.max_dim_1D(self.infiles[0])
        else:
            if self.aname.type != 'BRIK':
                print "** unknown 'type' for -input '%s'" % self.infiles[0]
            err,self.reps,self.tr =     \
                UTIL.get_dset_reps_tr(self.aname.pv(), verb=self.verb)
            if err: return 1

        if self.verb > 1:
            print '-- using kernel %s, kfile %s, tr = %s, reps = %s' %  \
                  (self.kernel, self.kfile, self.tr, self.reps)

        return None

    def make_decon_script(self):
        """create the deconvolution (3dTfitter) script"""

        nups    = '%02d' % self.tr_nup
        trup    = self.tr / self.tr_nup
        penalty = '012'
        kernel  = self.kernel
        kfile   = self.kfile

        # todo: include run lengths

        # maybe call the 3D version
        if self.aname.type != '1D': return self.make_decon_script_3d()

        cmd  = '# ------------------------------------------------------\n'  \
               '# perform neuro deconvolution via 3dTfitter\n\n'

        cmd += '# make and copy files into output directory\n'          \
               'set outdir = %s\n'                                      \
               'if ( ! -d $outdir ) mkdir $outdir\n\n' % self.outdir

        # get a list of (hopefully shortened) file labels
        llist = UTIL.list_minus_glob_form(self.infiles)
        llen = len(self.infiles)
        fix = 0
        if len(llist) != llen: fix = 1
        else:
           if llist[0] == self.infiles[0]: fix = 1
        if fix: llist = ['%02d.ts'%(ind+1) for ind in range(llen)]
        else  : llist = ['%02d.%s'%(ind+1,llist[ind]) for ind in range(llen)]

        # when copying file in, use 1dtranspose if they are horizontal
        adata = LD.AfniData(self.infiles[0], verb=self.verb)
        if not adata: return 1
        if adata.nrows == 1:
           nt = adata.ncols/self.tr_nup
           trstr = '(no transpose on read)'
           trchr = ''
        else:
           nt = adata.nrows/self.tr_nup
           transp = 1
           trstr = '(transpose on read)'
           trchr = '\\\''

        cmd += 'set files = ( %s )\n'           \
               'set labels = ( %s )\n\n'        \
               'cp -pv $files $outdir\n'        \
               'cd $outdir\n\n'                 \
               % (' '.join(self.infiles), ' '.join(llist))

        if self.kfile_in: kfile = self.kfile_in
        else:
           # generate a response kernel using 3dDeconvolve
           if   kernel == 'GAM':   ntk = 12/trup
           elif kernel == 'BLOCK': ntk = 15/trup
           else:
              print '** only GAM/BLOCK basis functions are allowed now'
              return 1

           if kernel == 'BLOCK': kernel = 'BLOCK(0.1,1)'
           tmpc = '# create response kernel\n'                      \
                  '3dDeconvolve -nodata %d %g -polort -1 \\\n'      \
                  '   -num_stimts 1 -stim_times 1 "1D:0" "%s" \\\n' \
                  '   -x1D %s -x1D_stop\n\n' % (ntk, trup, kernel, kfile)
           cmd += UTIL.add_line_wrappers(tmpc)

        pind = 0
        cmd += '# process each input file\n'                      \
               'foreach findex ( `count -digits 2 1 $#files` )\n' \
               '   # no zero-padding in shell index\n'            \
               '   set ival   = `ccalc -i $findex`\n'             \
               '   set infile = $files[$ival]:t\n'                \
               '   set label  = $labels[$ival]\n\n'               \

        polort = UTIL.get_default_polort(self.tr, self.reps)
        olab = 'p%02d.det.$label.1D' % pind
        cmd += '   # detrend the input %s\n'                      \
               '   3dDetrend -polort %d -prefix %s $infile%s\n\n' \
               % (trstr, polort, olab, trchr)

        pind += 1
        ilab = olab
        olab = 'p%02d.det.tr.$label.1D' % pind
        cmd += '   # transpose the detrended dataset\n'        \
               '   1dtranspose %s > %s\n\n' % (ilab, olab)

        pind += 1
        ilab = olab
        olab = 'p%02d.up%s.$label.1D' % (pind, nups)
        cmd += '   # upsample by factor of %d\n'                \
               '   1dUpsample %d %s > %s\n\n'                   \
               % (self.tr_nup, self.tr_nup, ilab, olab)

        sfile_up = olab # save original detrended upsample label

        pind += 1
        ilab = olab
        olab = 'p%02d.neuro.up%s.$label.1D' % (pind, nups)
        tmpc = '   3dTfitter -RHS %s \\\n'              \
              '             -FALTUNG %s %s \\\n'        \
              '             012 -2 -l2lasso -6\n\n' % (ilab,kfile,olab)
        cmd += UTIL.add_line_wrappers(tmpc)

        pind += 1
        ilab = olab
        olab = 'p%02d.neuro.up%s.tr.$label.1D' % (pind, nups)
        cmd += '   # transpose the neuro signal\n'      \
               '   1dtranspose %s > %s\n\n' % (ilab, olab)

        pind += 1
        ilab = olab
        olab = 'p%02d.reconv.up%s.tr.$label.1D' % (pind, nups)
        cmd += '   # reconvolve the neuro signal to compare with orig\n'\
               '   set nt = `cat %s | wc -l`\n'                         \
               '   waver -FILE %g %s -input %s \\\n'                \
               '         -numout $nt > %s\n\n'                          \
               % (ilab, trup, kfile, ilab, olab)

        rfile_up = olab # save reconvolved label

        cmd += 'end\n\n\n'

        cmd += 'echo "compare upsample input to reconvolved result via:"\n' \
               'echo "set label = %s"\n'                                    \
               'echo "1dplot -one %s %s"\n\n'                                \
               % (llist[0], sfile_up, rfile_up)

        return UTIL.write_text_to_file(self.script, cmd, exe=1)

    def make_decon_script_3d(self):
        """create the deconvolution (3dTfitter) script"""

        if self.aname.type == '1D':
            print '** 3D decon routine with 1D data?'
            return 1

        cmd  = '# ------------------------------------------------------\n'  \
               '# perform neuro deconvolution via 3dTfitter\n\n'

        cmd += "# create response kernel\n"                             \
               "waver -%s -peak 1 -dt %s -inline 1@1 > %s\n\n" %        \
                (self.kernel, self.tr, self.kfile)

        prefix = 'detrend%s' % self.aname.view
        polort = UTIL.get_default_polort(self.tr, self.reps)
        cmd += '# detrend the input\n'                          \
               '3dDetrend -polort %d -prefix %s %s\n\n' %       \
                (polort,prefix,self.infiles[0])

        if self.maskset: mask = '          -mask %s \\\n' % self.maskset
        else:            mask = ''

        dname = 'detrend%s' % self.aname.view

        c2  = '3dTfitter -lsqfit -RHS %s \\\n'           \
              '%s'                                       \
              '          -FALTUNG %s %s \\\n'            \
              '          01 0.0\n\n' % (dname,mask,self.kfile,self.prefix)

        cmd += UTIL.add_line_wrappers(c2)

        return UTIL.write_text_to_file(self.script, cmd, exe=1)

    def make_decon_script_old(self):
        """OLD METHOD: create the deconvolution (3dTfitter) script"""

        cmd  = '# ------------------------------------------------------\n'  \
               '# perform neuro deconvolution via 3dTfitter\n\n'

        cmd += "# create response kernel\n"                             \
               "waver -%s -peak 1 -dt %s -inline 1@1 > %s\n\n" %        \
                (self.kernel, self.tr, self.kfile)

        if self.aname.type == '1D': prefix = 'detrend.1D'
        else:                       prefix = 'detrend%s' % self.aname.view

        polort = UTIL.get_default_polort(self.tr, self.reps)
        cmd += '# detrend the input\n'                          \
               '3dDetrend -polort %d -prefix %s %s\n\n' %       \
                (polort,prefix,self.infiles[0])

        if self.maskset: mask = '          -mask %s \\\n' % self.maskset
        else:            mask = ''

        if self.aname.type == '1D':
            cmd += '# transpose the detrended dataset\n'        \
                   '1dtranspose detrend.1D > detrend_tr.1D\n\n'
            dname = 'detrend_tr.1D'
        else:
            dname = 'detrend%s' % self.aname.view

        c2  = '3dTfitter -lsqfit -RHS %s \\\n'           \
              '%s'                                       \
              '          -FALTUNG %s %s \\\n'            \
              '          01 0.0\n\n' % (dname,mask,self.kfile,self.prefix)

        cmd += UTIL.add_line_wrappers(c2)
        return UTIL.write_text_to_file(self.script, cmd, exe=1)

    def make_script(self):
        if   self.stype == 'old': return self.make_decon_script_old()
        elif self.stype == 'cur': return self.make_decon_script()

def process():
    decon = Decon('deconvolve response curves')
    decon.init_opts()
    rv = decon.read_opts()
    if rv != None:      # 0 is okay, else error code
        if rv: UTIL.show_args_as_command(sys.argv,"** failed command (RO):")
        return rv

    rv = decon.process_opts()
    if rv != None:
        if rv: UTIL.show_args_as_command(sys.argv,"** failed command (PO):")
        return rv

    rv = decon.make_script()
    if rv != None:
        if rv: UTIL.show_args_as_command(sys.argv,"** failed command (MS):")
        return rv

    return 0

if __name__ == "__main__":
    rv = process()
    sys.exit(rv)

