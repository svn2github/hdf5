/*
 * Copyright (c) 2001 National Center for Supercomputing Applications
 *                    All rights reserved.
 *
 * Programmer:  Bill Wendling <wendling@ncsa.uiuc.edu>
 *              Tuesday, 6. March 2001
 *
 * Purpose:     Support functions for the various tools.
 */
#ifndef H5TOOLS_UTILS_H__
#define H5TOOLS_UTILS_H__

#include "hdf5.h"

/*
 * begin get_option section
 */
extern int         opt_err;     /* getoption prints errors if this is on    */
extern int         opt_ind;     /* token pointer                            */
extern const char *opt_arg;     /* flag argument (or value)                 */

enum {
    no_arg = 0,         /* doesn't take an argument     */
    require_arg,        /* requires an argument	        */
    optional_arg        /* argument is optional         */
};

/*
 * get_option determines which options are specified on the command line and
 * returns a pointer to any arguments possibly associated with the option in
 * the ``opt_arg'' variable. get_option returns the shortname equivalent of
 * the option. The long options are specified in the following way:
 *
 * struct long_options foo[] = {
 * 	{ "filename", require_arg, 'f' },
 * 	{ "append", no_arg, 'a' },
 * 	{ "width", require_arg, 'w' },
 * 	{ NULL, 0, 0 }
 * };
 *
 * Long named options can have arguments specified as either:
 *
 * 	``--param=arg'' or ``--param arg''
 *
 * Short named options can have arguments specified as either:
 *
 * 	``-w80'' or ``-w 80''
 *
 * and can have more than one short named option specified at one time:
 *
 * 	-aw80
 * 
 * in which case those options which expect an argument need to come at the
 * end.
 */
typedef struct long_options {
    const char  *name;          /* name of the long option              */
    int          has_arg;       /* whether we should look for an arg    */
    char         shortval;      /* the shortname equivalent of long arg
                                 * this gets returned from get_option   */
} long_options;

extern int    get_option(int argc, const char **argv, const char *opt,
                         const struct long_options *l_opt);
/*
 * end get_option section
 */

/*struct taken from the dumper. needed in table struct*/
typedef struct obj_t {
    unsigned long objno[2];
    char objname[1024];
    int displayed;
    int recorded;
    int objflag;
} obj_t;

/*struct for the tables that the find_objs function uses*/
typedef struct table_t {
    int size;
    int nobjs;
    obj_t *objs;
} table_t;

/*this struct stores the information that is passed to the find_objs function*/
typedef struct find_objs_t {
    int prefix_len; 
    char *prefix;
    unsigned int threshold; /* should be 0 or 1 */
    table_t *group_table;
    table_t *type_table;
    table_t *dset_table;
    int status;
} find_objs_t;

extern int     nCols;               /*max number of columns for outputting  */

/* Definitions of useful routines */
extern void     indentation(int);
extern void     print_version(const char *progname);
extern void     error_msg(const char *progname, const char *fmt, ...);
extern void     warn_msg(const char *progname, const char *fmt, ...);
extern void     free_table(table_t **table);
extern void     dump_table(char *name, table_t *table);
extern int      get_table_idx(table_t *table, unsigned long *);
extern int      get_tableflag(table_t*, int);
extern int      set_tableflag(table_t*, int);
extern char    *get_objectname(table_t*, int);
extern herr_t   find_objs(hid_t group, const char *name, void *op_data);
extern int      search_obj(table_t *temp, unsigned long *);
extern void     init_table(table_t **tbl);
extern void     init_prefix(char **temp, int);

#endif	/* H5TOOLS_UTILS_H__ */
