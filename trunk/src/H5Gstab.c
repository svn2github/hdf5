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

/* Programmer: Robb Matzke <matzke@llnl.gov>
 *	       Friday, September 19, 1997
 *
 */
#define H5F_PACKAGE		/*suppress error about including H5Fpkg	  */
#define H5G_PACKAGE		/*suppress error about including H5Gpkg	  */


/* Packages needed by this file... */
#include "H5private.h"		/* Generic Functions			*/
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5Fpkg.h"		/* File access				*/
#include "H5Gpkg.h"		/* Groups		  		*/
#include "H5HLprivate.h"	/* Local Heaps				*/
#include "H5Iprivate.h"		/* IDs			  		*/
#include "H5MMprivate.h"	/* Memory management			*/

/* Private typedefs */
/* User data for finding a name in the link messages */
typedef struct {
    /* downward */
    H5F_t      *f;              /* Pointer to file for insertion */
    const char *name;           /* Name to search for */

    /* upward */
    H5G_entry_t *ent;           /* Entry to update when match found */
} H5G_stab_ud1_t;

/* User data for finding object info from B-tree */
typedef struct {
    /* downward */
    H5O_loc_t *grp_oloc;        /* Object location of group */
    hid_t       dxpl_id;        /* DXPL during operation */
    haddr_t     heap_addr;      /* Local heap address for group */

    /* upward */
    H5G_stat_t *statbuf;        /* Caller's statbuf */
} H5G_stab_fnd_ud1_t;

/* User data for finding link information from B-tree */
typedef struct {
    /* downward */
    H5F_t      *file;           /* Pointer to file for query */
    hid_t       dxpl_id;        /* DXPL during operation */
    const char *name;           /* Name to search for */
    haddr_t     heap_addr;      /* Local heap address for group */

    /* upward */
    H5O_link_t *lnk;            /* Caller's link location */
} H5G_stab_fnd_ud2_t;

/* User data for finding object location from B-tree */
typedef struct {
    /* downward */
    H5F_t      *file;           /* Pointer to file for query */
    size_t      size;           /* Buffer size for link value */
    haddr_t     heap_addr;      /* Local heap address for group */
    hid_t       dxpl_id;        /* DXPL during operation */

    /* upward */
    char *buf;                  /* Buffer to fill with link value */
} H5G_stab_fnd_ud3_t;

/* Private prototypes */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_create_components
 *
 * Purpose:	Creates the components for a new, empty, symbol table (name heap
 *		and B-tree).  The caller can specify an initial size for the
 *		name heap.
 *
 *		In order for the B-tree to operate correctly, the first
 *		item in the heap is the empty string, and must appear at
 *		heap offset zero.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@ncsa.uiuc.edu
 *		Nov  7 2005
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_create_components(H5F_t *f, H5O_stab_t *stab, size_t size_hint, hid_t dxpl_id)
{
    size_t	name_offset;	        /* Offset of "" name	*/
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_NOAPI(H5G_stab_create_components, FAIL)

    /*
     * Check arguments.
     */
    HDassert(f);
    HDassert(stab);
    HDassert(size_hint > 0);

    /* Create the B-tree */
    if(H5B_create(f, dxpl_id, H5B_SNODE, NULL, &(stab->btree_addr)/*out*/) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "can't create B-tree")

    /* Create symbol table private heap */
    if(H5HL_create(f, dxpl_id, size_hint, &(stab->heap_addr)/*out*/) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "can't create heap")
    name_offset = H5HL_insert(f, dxpl_id, stab->heap_addr, (size_t)1, "");
    if((size_t)(-1) == name_offset)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "can't initialize heap")

    /*
     * B-tree's won't work if the first name isn't at the beginning
     * of the heap.
     */
    HDassert(0 == name_offset);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_create_components() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_create
 *
 * Purpose:	Creates a new empty symbol table (object header, name heap,
 *		and B-tree).  The caller can specify an initial size for the
 *		name heap.  The object header of the group is opened for
 *		write access.
 *
 *		In order for the B-tree to operate correctly, the first
 *		item in the heap is the empty string, and must appear at
 *		heap offset zero.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Robb Matzke
 *		matzke@llnl.gov
 *		Aug  1 1997
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_create(H5O_loc_t *grp_oloc, hid_t dxpl_id, const H5O_ginfo_t *ginfo,
    H5O_stab_t *stab)
{
    size_t      heap_hint;              /* Local heap size hint */
    size_t      size_hint;              /* Local heap size hint */
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_NOAPI(H5G_stab_create, FAIL)

    /*
     * Check arguments.
     */
    HDassert(grp_oloc);
    HDassert(stab);

    /* Adjust the size hint, if necessary */
    if(ginfo->lheap_size_hint == 0)
        heap_hint = 8 +         /* "null" name inserted for B-tree */
                (ginfo->est_num_entries * H5HL_ALIGN(ginfo->est_name_len + 1)) +    /* estimated size of names for links, aligned for inserting into local heap */
                H5HL_SIZEOF_FREE(grp_oloc->file);       /* Free list entry in local heap */
    else
        heap_hint = ginfo->lheap_size_hint;
    size_hint = MAX(heap_hint, H5HL_SIZEOF_FREE(grp_oloc->file) + 2);

    /* Go create the B-tree & local heap */
    if(H5G_stab_create_components(grp_oloc->file, stab, size_hint, dxpl_id) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "can't create symbol table components")

    /*
     * Insert the symbol table message into the object header and the symbol
     * table entry.
     */
    if(H5O_modify(grp_oloc, H5O_STAB_ID, H5O_NEW_MESG, 0, H5O_UPDATE_TIME, stab, dxpl_id) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "can't create message")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_create() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_insert_real
 *
 * Purpose:	Insert a new symbol into a table.
 *		The name of the new symbol is NAME and its symbol
 *		table entry is OBJ_LNK.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@uiuc.edu
 *		Nov  7 2005
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_insert_real(H5F_t *f, H5O_stab_t *stab, const char *name,
    H5O_link_t *obj_lnk, hid_t dxpl_id)
{
    H5G_bt_ud1_t	udata;		/* Data to pass through B-tree	*/
    herr_t              ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_NOAPI(H5G_stab_insert_real, FAIL)

    /* check arguments */
    HDassert(f);
    HDassert(stab);
    HDassert(name && *name);
    HDassert(obj_lnk);

    /* Initialize data to pass through B-tree */
    udata.common.name = name;
    udata.common.heap_addr = stab->heap_addr;
    udata.lnk = obj_lnk;

    /* Insert into symbol table */
    if(H5B_insert(f, dxpl_id, H5B_SNODE, stab->btree_addr, &udata) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTINSERT, FAIL, "unable to insert entry")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_insert_real() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_insert
 *
 * Purpose:	Insert a new symbol into the table described by GRP_ENT in
 *		file F.	 The name of the new symbol is NAME and its symbol
 *		table entry is OBJ_ENT.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Robb Matzke
 *		matzke@llnl.gov
 *		Aug  1 1997
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_insert(H5O_loc_t *grp_oloc, const char *name, H5O_link_t *obj_lnk,
    hid_t dxpl_id)
{
    H5O_stab_t		stab;		/* Symbol table message		*/
    herr_t              ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_NOAPI(H5G_stab_insert, FAIL)

    /* check arguments */
    HDassert(grp_oloc && grp_oloc->file);
    HDassert(name && *name);
    HDassert(obj_lnk);

    /* Retrieve symbol table message */
    if(NULL == H5O_read(grp_oloc, H5O_STAB_ID, 0, &stab, dxpl_id))
        HGOTO_ERROR(H5E_SYM, H5E_BADMESG, FAIL, "not a symbol table")

    if(H5G_stab_insert_real(grp_oloc->file, &stab, name, obj_lnk, dxpl_id) < 0)
        HGOTO_ERROR(H5E_DATATYPE, H5E_CANTINIT, H5B_ITER_ERROR, "unable to insert the name")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_insert() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_remove
 *
 * Purpose:	Remove NAME from a symbol table.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Robb Matzke
 *              Thursday, September 17, 1998
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_remove(H5O_loc_t *loc, hid_t dxpl_id, H5RS_str_t *grp_full_path_r,
    const char *name)
{
    H5O_stab_t		stab;		/*symbol table message		*/
    H5G_bt_ud2_t	udata;		/*data to pass through B-tree	*/
    herr_t              ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_NOAPI(H5G_stab_remove, FAIL)

    HDassert(loc && loc->file);
    HDassert(name && *name);

    /* Read in symbol table message */
    if(NULL == H5O_read(loc, H5O_STAB_ID, 0, &stab, dxpl_id))
        HGOTO_ERROR(H5E_SYM, H5E_BADMESG, FAIL, "not a symbol table")

    /* Initialize data to pass through B-tree */
    udata.common.name = name;
    udata.common.heap_addr = stab.heap_addr;
    udata.adj_link = TRUE;
    udata.grp_full_path_r = grp_full_path_r;

    /* Remove from symbol table */
    if(H5B_remove(loc->file, dxpl_id, H5B_SNODE, stab.btree_addr, &udata) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to remove entry")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_remove() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_remove_by_idx
 *
 * Purpose:	Remove NAME from a symbol table, according to the name index.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *              Wednesday, November 15, 2006
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_remove_by_idx(H5O_loc_t *grp_oloc, hid_t dxpl_id, H5RS_str_t *grp_full_path_r,
    H5_iter_order_t order, hsize_t n)
{
    H5O_stab_t		stab;		/* Symbol table message		*/
    H5G_bt_ud2_t	udata;		/* Data to pass through B-tree	*/
    H5O_link_t          obj_lnk;        /* Object's link within group */
    hbool_t             lnk_copied = FALSE;         /* Whether the link was copied */
    herr_t              ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_NOAPI(H5G_stab_remove_by_idx, FAIL)

    HDassert(grp_oloc && grp_oloc->file);

    /* Look up name of link to remove, by index */
    if(H5G_stab_lookup_by_idx(grp_oloc, order, n, &obj_lnk, dxpl_id) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTGET, FAIL, "can't get link information")
    lnk_copied = TRUE;

    /* Read in symbol table message */
    if(NULL == H5O_read(grp_oloc, H5O_STAB_ID, 0, &stab, dxpl_id))
        HGOTO_ERROR(H5E_SYM, H5E_BADMESG, FAIL, "not a symbol table")

    /* Initialize data to pass through B-tree */
    udata.common.name = obj_lnk.name;
    udata.common.heap_addr = stab.heap_addr;
    udata.adj_link = TRUE;
    udata.grp_full_path_r = grp_full_path_r;

    /* Remove link from symbol table */
    if(H5B_remove(grp_oloc->file, dxpl_id, H5B_SNODE, stab.btree_addr, &udata) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to remove entry")

done:
    /* Reset the link information, if we have a copy */
    if(lnk_copied)
        H5O_reset(H5O_LINK_ID, &obj_lnk);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_remove() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_delete
 *
 * Purpose:	Delete entire symbol table information from file
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *              Thursday, March 20, 2003
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_delete(H5F_t *f, hid_t dxpl_id, const H5O_stab_t *stab, hbool_t adj_link)
{
    H5G_bt_ud2_t	udata;		/*data to pass through B-tree	*/
    herr_t  ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(H5G_stab_delete, FAIL);

    assert(f);
    assert(stab);
    assert(H5F_addr_defined(stab->btree_addr));
    assert(H5F_addr_defined(stab->heap_addr));

    /* Set up user data for B-tree deletion */
    udata.common.name = NULL;
    udata.common.heap_addr = stab->heap_addr;
    udata.adj_link = adj_link;

    /* Delete entire B-tree */
    if(H5B_delete(f, dxpl_id, H5B_SNODE, stab->btree_addr, &udata) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDELETE, FAIL, "unable to delete symbol table B-tree");

    /* Delete local heap for names */
    if(H5HL_delete(f, dxpl_id, stab->heap_addr) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTDELETE, FAIL, "unable to delete symbol table heap");

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5G_stab_delete() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_iterate
 *
 * Purpose:	Iterate over the objects in a group
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *              Monday, October  3, 2005
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_iterate(H5O_loc_t *oloc, H5_iter_order_t order, hid_t gid,
    hbool_t lib_internal, int skip, int *last_lnk, H5G_link_iterate_t op,
    void *op_data, hid_t dxpl_id)
{
    H5G_bt_it_ud1_t	udata;                  /* User data to pass to B-tree callback */
    H5O_stab_t		stab;		        /* Info about symbol table */
    herr_t		ret_value;

    FUNC_ENTER_NOAPI(H5G_stab_iterate, FAIL)

    /* Sanity check */
    HDassert(oloc);
    HDassert(lib_internal || H5I_GROUP == H5I_get_type(gid));
    HDassert(op.lib_op);

    /* Get the B-tree info */
    if(NULL == H5O_read(oloc, H5O_STAB_ID, 0, &stab, dxpl_id))
	HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to determine local heap address")

    /* Check on iteration order */
    /* ("native" iteration order is increasing for this link storage mechanism) */
    if(order != H5_ITER_DEC) {
        /* Build udata to pass through H5B_iterate() to H5G_node_iterate() */
        udata.group_id = gid;
        udata.skip = skip;
        udata.heap_addr = stab.heap_addr;
        udata.lib_internal = lib_internal;
        udata.op = op;
        udata.op_data = op_data;
        udata.final_ent = last_lnk;

        /* Iterate over the group members */
        if((ret_value = H5B_iterate(oloc->file, H5AC_dxpl_id, H5B_SNODE,
                  H5G_node_iterate, stab.btree_addr, &udata)) < 0)
            HERROR(H5E_SYM, H5E_CANTNEXT, "iteration operator failed");
    } /* end if */
    else {
HDfprintf(stderr, "%s: Decreasing iteration order for symbol table storage not supported yet!\n", FUNC);
HGOTO_ERROR(H5E_SYM, H5E_UNSUPPORTED, FAIL, "decreasing iteration order for symbol table storage not supported yet")
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_iterate() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_count
 *
 * Purpose:	Count the # of links in a group
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *              Tuesday, September  6, 2005
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_count(H5O_loc_t *oloc, hsize_t *num_objs, hid_t dxpl_id)
{
    H5O_stab_t		stab;		        /* Info about symbol table */
    herr_t		ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(H5G_stab_count, FAIL)

    /* Sanity check */
    HDassert(oloc);
    HDassert(num_objs);

    /* Reset the number of objects in the group */
    *num_objs = 0;

    /* Get the B-tree info */
    if(NULL == H5O_read(oloc, H5O_STAB_ID, 0, &stab, dxpl_id))
	HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to determine local heap address")

    /* Iterate over the group members */
    if(H5B_iterate(oloc->file, dxpl_id, H5B_SNODE, H5G_node_sumup, stab.btree_addr, num_objs) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "iteration operator failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_count() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_get_name_by_idx_cb
 *
 * Purpose:     Callback for B-tree iteration 'by index' info query to 
 *              retrieve the name of a link
 *
 * Return:	Success:        Non-negative
 *		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *	        Nov  7, 2006
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5G_stab_get_name_by_idx_cb(const H5G_entry_t *ent, void *_udata)
{
    H5G_bt_it_idx1_t	*udata = (H5G_bt_it_idx1_t *)_udata;
    H5HL_t *heap = NULL;                /* Pointer to local heap for group */
    size_t name_off;                    /* Offset of name in heap */
    const char *name;                   /* Pointer to name string in heap */
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_NOAPI_NOINIT(H5G_stab_get_name_by_idx_cb)

    /* Sanity check */
    HDassert(ent);
    HDassert(udata);

    /* Get name offset in heap */
    name_off = ent->name_off;

    /* Pin the heap down in memory */
    if(NULL == (heap = H5HL_protect(udata->common.f, udata->common.dxpl_id, udata->heap_addr)))
        HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to protect symbol name")

    name = H5HL_offset_into(udata->common.f, heap, name_off);
    HDassert(name);
    udata->name = H5MM_strdup(name);
    HDassert(udata->name);

done:
    if(heap && H5HL_unprotect(udata->common.f, udata->common.dxpl_id, heap, udata->heap_addr, H5AC__NO_FLAGS_SET) < 0)
        HDONE_ERROR(H5E_SYM, H5E_PROTECT, FAIL, "unable to unprotect symbol name")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_get_name_by_idx_cb */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_get_name_by_idx
 *
 * Purpose:     Returns the name of objects in the group by giving index.
 *
 * Return:	Success:        Non-negative, length of name
 *		Failure:	Negative
 *
 * Programmer:	Raymond Lu
 *	        Nov 20, 2002
 *
 *-------------------------------------------------------------------------
 */
ssize_t
H5G_stab_get_name_by_idx(H5O_loc_t *oloc, H5_iter_order_t order, hsize_t n,
    char* name, size_t size, hid_t dxpl_id)
{
    H5O_stab_t		stab;	        /* Info about local heap & B-tree */
    H5G_bt_it_idx1_t	udata;          /* Iteration information */
    ssize_t		ret_value;      /* Return value */

    FUNC_ENTER_NOAPI(H5G_stab_get_name_by_idx, FAIL)

    /* Sanity check */
    HDassert(oloc);

    /* Get the B-tree & local heap info */
    if(NULL == H5O_read(oloc, H5O_STAB_ID, 0, &stab, dxpl_id))
	HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to determine local heap address")

    /* Remap index for decreasing iteration order */
    if(order == H5_ITER_DEC) {
        hsize_t nlinks = 0;           /* Number of links in group */

        /* Iterate over the symbol table nodes, to count the links */
        if(H5B_iterate(oloc->file, dxpl_id, H5B_SNODE, H5G_node_sumup, stab.btree_addr, &nlinks) < 0)
            HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "iteration operator failed")

        /* Map decreasing iteration order index to increasing iteration order index */
        n = nlinks - (n + 1);
    } /* end if */

    /* Set iteration information */
    udata.common.f = oloc->file;
    udata.common.dxpl_id = dxpl_id;
    udata.common.idx = n;
    udata.common.num_objs = 0;
    udata.common.op = H5G_stab_get_name_by_idx_cb;
    udata.heap_addr = stab.heap_addr;
    udata.name = NULL;

    /* Iterate over the group members */
    if(H5B_iterate(oloc->file, dxpl_id, H5B_SNODE, H5G_node_by_idx, stab.btree_addr, &udata) < 0)
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "iteration operator failed")

    /* If we don't know the name now, we almost certainly went out of bounds */
    if(udata.name == NULL)
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "index out of bound")

    /* Get the length of the name */
    ret_value = (ssize_t)HDstrlen(udata.name);

    /* Copy the name into the user's buffer, if given */
    if(name) {
        HDstrncpy(name, udata.name, MIN((size_t)(ret_value + 1), size));
        if((size_t)ret_value >= size)
            name[size - 1]='\0';
    } /* end if */

done:
    /* Free the duplicated name */
    if(udata.name != NULL)
        H5MM_xfree(udata.name);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_get_name_by_idx() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_get_type_by_idx_cb
 *
 * Purpose:     Callback for B-tree iteration 'by index' info query to 
 *              retrieve the type of an object
 *
 * Return:	Success:        Non-negative
 *		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *	        Nov  7, 2006
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5G_stab_get_type_by_idx_cb(const H5G_entry_t *ent, void *_udata)
{
    H5G_bt_it_idx2_t	*udata = (H5G_bt_it_idx2_t *)_udata;
    H5O_loc_t tmp_oloc;                 /* Temporary object location */

    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5G_stab_get_type_by_idx_cb)

    /* Sanity check */
    HDassert(ent);
    HDassert(udata);

    /* Check for a soft link */
    switch(ent->type) {
        case H5G_CACHED_SLINK:
            udata->type = H5G_LINK;
            break;

        default:
            /* Build temporary object location */
            tmp_oloc.file = udata->common.f;
            HDassert(H5F_addr_defined(ent->header));
            tmp_oloc.addr = ent->header;

            udata->type = H5O_obj_type(&tmp_oloc, udata->common.dxpl_id);
            break;
    } /* end switch */

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* end H5G_stab_get_type_by_idx_cb */


/*-------------------------------------------------------------------------
 * Function:	H5G_get_objtype_by_idx
 *
 * Purpose:     Private function for H5Gget_objtype_by_idx.
 *              Returns the type of objects in the group by giving index.
 *
 * Return:	Success:        H5G_GROUP(1), H5G_DATASET(2), H5G_TYPE(3)
 *
 *		Failure:	UNKNOWN
 *
 * Programmer:	Raymond Lu
 *	        Nov 20, 2002
 *
 *-------------------------------------------------------------------------
 */
H5G_obj_t
H5G_stab_get_type_by_idx(H5O_loc_t *oloc, hsize_t idx, hid_t dxpl_id)
{
    H5O_stab_t		stab;	        /* Info about local heap & B-tree */
    H5G_bt_it_idx2_t	udata;          /* User data for B-tree callback */
    H5G_obj_t		ret_value;      /* Return value */

    FUNC_ENTER_NOAPI(H5G_stab_get_type_by_idx, H5G_UNKNOWN)

    /* Sanity check */
    HDassert(oloc);

    /* Get the B-tree & local heap info */
    if(NULL == H5O_read(oloc, H5O_STAB_ID, 0, &stab, dxpl_id))
	HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, H5G_UNKNOWN, "unable to determine local heap address")

    /* Set iteration information */
    udata.common.f = oloc->file;
    udata.common.dxpl_id = dxpl_id;
    udata.common.idx = idx;
    udata.common.num_objs = 0;
    udata.common.op = H5G_stab_get_type_by_idx_cb;
    udata.type = H5G_UNKNOWN;

    /* Iterate over the group members */
    if(H5B_iterate(oloc->file, dxpl_id, H5B_SNODE, H5G_node_by_idx, stab.btree_addr, &udata) < 0)
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5G_UNKNOWN, "iteration operator failed")

    /* If we don't know the type now, we almost certainly went out of bounds */
    if(udata.type == H5G_UNKNOWN)
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5G_UNKNOWN, "index out of bound")

    /* Set the return value */
    ret_value = udata.type;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_get_type_by_idx() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_lookup_cb
 *
 * Purpose:     B-tree 'find' callback to retrieve location for an object
 *
 * Return:	Success:        Non-negative
 *
 *		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *	        Sep 20, 2005
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5G_stab_lookup_cb(const H5G_entry_t *ent, void *_udata)
{
    H5G_stab_fnd_ud2_t *udata = (H5G_stab_fnd_ud2_t *)_udata;   /* 'User data' passed in */
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI_NOINIT(H5G_stab_lookup_cb)

    /* Set link info */
    if(udata->lnk) {
        /* Convert the entry to a link */
        if(H5G_link_convert(udata->file, udata->dxpl_id, udata->lnk, udata->heap_addr,
                ent, udata->name) < 0)
            HGOTO_ERROR(H5E_SYM, H5E_CANTCONVERT, FAIL, "unable to convert symbol table entry to link")
    } /* end if */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_lookup_cb() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_lookup
 *
 * Purpose:	Look up an object relative to a group, using symbol table
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@ncsa.uiuc.edu
 *		Sep 20 2005
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_lookup(H5O_loc_t *grp_oloc, const char *name, H5O_link_t *lnk,
    hid_t dxpl_id)
{
    H5G_bt_ud3_t bt_udata;      /* Data to pass through B-tree	*/
    H5G_stab_fnd_ud2_t udata;   /* 'User data' to give to callback */
    H5O_stab_t stab;		/* Symbol table message		*/
    herr_t     ret_value = SUCCEED;     /* Return value */

    FUNC_ENTER_NOAPI(H5G_stab_lookup, FAIL)

    /* check arguments */
    HDassert(grp_oloc && grp_oloc->file);
    HDassert(name && *name);
    HDassert(lnk);

    /* Set up user data to pass to 'find' operation callback */
    udata.file = grp_oloc->file;
    udata.dxpl_id = dxpl_id;
    udata.name = name;
    udata.lnk = lnk;

    /* Set up the user data for actual B-tree find operation */
    if(NULL == H5O_read(grp_oloc, H5O_STAB_ID, 0, &stab, dxpl_id))
        HGOTO_ERROR(H5E_SYM, H5E_BADMESG, FAIL, "can't read message")
    bt_udata.common.name = name;
    bt_udata.common.heap_addr = stab.heap_addr;
    bt_udata.op = H5G_stab_lookup_cb;
    bt_udata.op_data = &udata;

    /* Finish up user data to pass to 'find' operation callback */
    udata.heap_addr = stab.heap_addr;

    /* Search the B-tree */
    if(H5B_find(grp_oloc->file, dxpl_id, H5B_SNODE, stab.btree_addr, &bt_udata) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "not found")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_lookup() */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_lookup_by_idx_cb
 *
 * Purpose:     Callback for B-tree iteration 'by index' info query to 
 *              retrieve the link
 *
 * Return:	Success:        Non-negative
 *		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *	        Nov  9, 2006
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5G_stab_lookup_by_idx_cb(const H5G_entry_t *ent, void *_udata)
{
    H5G_bt_it_idx3_t *udata = (H5G_bt_it_idx3_t *)_udata;
    H5HL_t *heap;                       /* Pointer to local heap for group */
    size_t name_off;                    /* Offset of name in heap */
    const char *name;                   /* Pointer to name string in heap */
    size_t name_len;                    /* Length of link name */
    char buf[1024];                     /* Buffer for name */
    char *s = NULL;                     /* Pointer to copy of name string */
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_NOAPI_NOINIT(H5G_stab_lookup_by_idx_cb)

    /* Sanity check */
    HDassert(ent);
    HDassert(udata);

    /* Get name offset in heap */
    name_off = ent->name_off;

    /* Pin the heap down in memory */
    if(NULL == (heap = H5HL_protect(udata->common.f, udata->common.dxpl_id, udata->heap_addr)))
        HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to protect symbol name")

    /* Duplicate the link name */
    name = H5HL_offset_into(udata->common.f, heap, name_off);
    HDassert(name);
    name_len = HDstrlen(name);

    /* Allocate space or point to existing buffer */
    if((name_len + 1) > sizeof(buf)) {
        if(NULL == (s = H5MM_malloc(name_len + 1)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed")
    } /* end if */
    else
        s = buf;

    /* Make a copy of the name */
    HDstrcpy(s, name);

    /* Unlock the heap */
    if(H5HL_unprotect(udata->common.f, udata->common.dxpl_id, heap, udata->heap_addr, H5AC__NO_FLAGS_SET) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_PROTECT, FAIL, "unable to unprotect symbol name")
    heap = NULL; name = NULL;

    /* Convert the entry to a link */
    if(H5G_link_convert(udata->common.f, udata->common.dxpl_id, udata->lnk, udata->heap_addr, ent, s) < 0)
        HGOTO_ERROR(H5E_SYM, H5E_CANTCONVERT, FAIL, "unable to convert symbol table entry to link")
    udata->found = TRUE;

done:
    /* Free the memory for the name, if we used a dynamically allocated buffer */
    if(s && s != buf)
        H5MM_xfree(s);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_lookup_by_idx_cb */


/*-------------------------------------------------------------------------
 * Function:	H5G_stab_lookup_by_idx
 *
 * Purpose:	Look up an object in a group, according to the name index
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Nov  7 2006
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_stab_lookup_by_idx(H5O_loc_t *grp_oloc, H5_iter_order_t order, hsize_t n,
    H5O_link_t *lnk, hid_t dxpl_id)
{
    H5G_bt_it_idx3_t udata;             /* Iteration information */
    H5O_stab_t stab;		        /* Symbol table message */
    herr_t     ret_value = SUCCEED;     /* Return value */

    FUNC_ENTER_NOAPI(H5G_stab_lookup_by_idx, FAIL)

    /* check arguments */
    HDassert(grp_oloc && grp_oloc->file);
    HDassert(lnk);

    /* Get the B-tree & local heap info */
    if(NULL == H5O_read(grp_oloc, H5O_STAB_ID, 0, &stab, dxpl_id))
	HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to determine local heap address")

    /* Remap index for decreasing iteration order */
    if(order == H5_ITER_DEC) {
        hsize_t nlinks = 0;           /* Number of links in group */

        /* Iterate over the symbol table nodes, to count the links */
        if(H5B_iterate(grp_oloc->file, dxpl_id, H5B_SNODE, H5G_node_sumup, stab.btree_addr, &nlinks) < 0)
            HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "iteration operator failed")

        /* Map decreasing iteration order index to increasing iteration order index */
        n = nlinks - (n + 1);
    } /* end if */

    /* Set iteration information */
    udata.common.f = grp_oloc->file;
    udata.common.dxpl_id = dxpl_id;
    udata.common.idx = n;
    udata.common.num_objs = 0;
    udata.common.op = H5G_stab_lookup_by_idx_cb;
    udata.heap_addr = stab.heap_addr;
    udata.lnk = lnk;
    udata.found = FALSE;

    /* Iterate over the group members */
    if(H5B_iterate(grp_oloc->file, dxpl_id, H5B_SNODE, H5G_node_by_idx, stab.btree_addr, &udata) < 0)
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "iteration operator failed")

    /* If we didn't find the link, we almost certainly went out of bounds */
    if(!udata.found)
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "index out of bound")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_stab_lookup_by_idx() */

