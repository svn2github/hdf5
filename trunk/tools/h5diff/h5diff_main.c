/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdf.ncsa.uiuc.edu/HDF5/doc/Copyright.html.  If you do not have     *
 * access to either file, you may request a copy from hdfhelp@ncsa.uiuc.edu. *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "h5diff.h"
#include <stdlib.h>
#include <assert.h>
#include "h5diff_common.h"

/*-------------------------------------------------------------------------
 * Function: main
 *
 * Purpose: h5diff main program
 *
 * Return: An  exit status of 0 means no differences were found, 1 means some
 *   differences were found.
 *
 * Programmer: Pedro Vicente, pvn@ncsa.uiuc.edu
 *
 * Date: May 9, 2003
 *
 * Comments:
 *
 * Modifications: July 2004
 *  Introduced the four modes:
 *   Normal mode: print the number of differences found and where they occured
 *   Report mode: print the above plus the differences
 *   Verbose mode: print the above plus a list of objects and warnings
 *   Quiet mode: do not print output
 *
 * November 2004: Leon Arber (larber@uiuc.edu)
 * 		  Additions that allow h5diff to be run in parallel
 *
 * February 2005: Leon Arber (larber@uiuc.edu)
 * 		h5diff and ph5diff split into two files, one that is used
 * 		to build a serial h5diff and one used to build a parallel h5diff
 * 		Common functions have been moved to h5diff_common.c
 *-------------------------------------------------------------------------
 */


int main(int argc, const char *argv[])
{
    int        ret;
    const char *fname1 = NULL;
    const char *fname2 = NULL;
    const char *objname1  = NULL;
    const char *objname2  = NULL;
    hsize_t    nfound=0;
    diff_opt_t options;

    parse_input(argc, argv, &fname1, &fname2, &objname1, &objname2, &options);

    nfound = h5diff(fname1,fname2,objname1,objname2,&options);

    print_results(&options);

    /*-------------------------------------------------------------------------
     * exit code
     *   >0 if differences, 0 if no differences, <0 if error
     *-------------------------------------------------------------------------
     */

    ret= (nfound==0 ? 0 : 1 );
    if (options.err_stat)
	ret=-1;
    return ret;
}

