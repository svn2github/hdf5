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

/*-------------------------------------------------------------------------
 *
 * Created:		H5Glink.c
 *			Nov 13 2006
 *			Quincey Koziol <koziol@hdfgroup.org>
 *
 * Purpose:		Functions for handling links in groups.
 *
 *-------------------------------------------------------------------------
 */

/****************/
/* Module Setup */
/****************/

#define H5G_PACKAGE		/*suppress error about including H5Gpkg	  */


/***********/
/* Headers */
/***********/
#include "H5private.h"		/* Generic Functions			*/
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5Gpkg.h"		/* Groups		  		*/
#include "H5HLprivate.h"	/* Local Heaps				*/


/****************/
/* Local Macros */
/****************/


/******************/
/* Local Typedefs */
/******************/


/********************/
/* Package Typedefs */
/********************/


/********************/
/* Local Prototypes */
/********************/


/*********************/
/* Package Variables */
/*********************/


/*****************************/
/* Library Private Variables */
/*****************************/


/*******************/
/* Local Variables */
/*******************/



/*-------------------------------------------------------------------------
 * Function:	H5G_link_cmp_name_inc
 *
 * Purpose:	Callback routine for comparing two link names, in
 *              increasing alphabetic order
 *
 * Return:	An integer less than, equal to, or greater than zero if the
 *              first argument is considered to be respectively less than,
 *              equal to, or greater than the second.  If two members compare
 *              as equal, their order in the sorted array is undefined.
 *              (i.e. same as strcmp())
 *
 * Programmer:	Quincey Koziol
 *		koziol@ncsa.uiuc.edu
 *		Sep  5 2005
 *
 *-------------------------------------------------------------------------
 */
int
H5G_link_cmp_name_inc(const void *lnk1, const void *lnk2)
{
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5G_link_cmp_name_inc)

    FUNC_LEAVE_NOAPI(HDstrcmp(((const H5O_link_t *)lnk1)->name, ((const H5O_link_t *)lnk2)->name))
} /* end H5G_link_cmp_name_inc() */


/*-------------------------------------------------------------------------
 * Function:	H5G_link_cmp_name_dec
 *
 * Purpose:	Callback routine for comparing two link names, in
 *              decreasing alphabetic order
 *
 * Return:	An integer less than, equal to, or greater than zero if the
 *              second argument is considered to be respectively less than,
 *              equal to, or greater than the first.  If two members compare
 *              as equal, their order in the sorted array is undefined.
 *              (i.e. opposite strcmp())
 *
 * Programmer:	Quincey Koziol
 *		koziol@ncsa.uiuc.edu
 *		Sep 25 2006
 *
 *-------------------------------------------------------------------------
 */
int
H5G_link_cmp_name_dec(const void *lnk1, const void *lnk2)
{
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5G_link_cmp_name_dec)

    FUNC_LEAVE_NOAPI(HDstrcmp(((const H5O_link_t *)lnk2)->name, ((const H5O_link_t *)lnk1)->name))
} /* end H5G_link_cmp_name_dec() */


/*-------------------------------------------------------------------------
 * Function:	H5G_link_cmp_corder_inc
 *
 * Purpose:	Callback routine for comparing two link creation orders, in
 *              increasing order
 *
 * Return:	An integer less than, equal to, or greater than zero if the
 *              first argument is considered to be respectively less than,
 *              equal to, or greater than the second.  If two members compare
 *              as equal, their order in the sorted array is undefined.
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Nov  6 2006
 *
 *-------------------------------------------------------------------------
 */
int
H5G_link_cmp_corder_inc(const void *lnk1, const void *lnk2)
{
    int ret_value;              /* Return value */

    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5G_link_cmp_corder_inc)

    if(((const H5O_link_t *)lnk1)->corder < ((const H5O_link_t *)lnk2)->corder)
        ret_value = -1;
    else if(((const H5O_link_t *)lnk1)->corder > ((const H5O_link_t *)lnk2)->corder)
        ret_value = 1;
    else
        ret_value = 0;

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_link_cmp_corder_inc() */


/*-------------------------------------------------------------------------
 * Function:	H5G_link_cmp_corder_dec
 *
 * Purpose:	Callback routine for comparing two link creation orders, in
 *              decreasing order
 *
 * Return:	An integer less than, equal to, or greater than zero if the
 *              second argument is considered to be respectively less than,
 *              equal to, or greater than the first.  If two members compare
 *              as equal, their order in the sorted array is undefined.
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Nov  6 2006
 *
 *-------------------------------------------------------------------------
 */
int
H5G_link_cmp_corder_dec(const void *lnk1, const void *lnk2)
{
    int ret_value;              /* Return value */

    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5G_link_cmp_corder_dec)

    if(((const H5O_link_t *)lnk1)->corder < ((const H5O_link_t *)lnk2)->corder)
        ret_value = 1;
    else if(((const H5O_link_t *)lnk1)->corder > ((const H5O_link_t *)lnk2)->corder)
        ret_value = -1;
    else
        ret_value = 0;

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_link_cmp_corder_dec() */


/*-------------------------------------------------------------------------
 * Function:	H5G_link_convert
 *
 * Purpose:     Convert a symbol table entry to a link
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Sep 16 2006
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_link_convert(H5F_t *f, hid_t dxpl_id, H5O_link_t *lnk, haddr_t lheap_addr,
    const H5G_entry_t *ent, const char *name)
{
    herr_t     ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_NOAPI(H5G_link_convert, FAIL)

    /* check arguments */
    HDassert(f);
    HDassert(lnk);
    HDassert(ent);

    /* Set (default) common info for link */
    lnk->cset = H5F_DEFAULT_CSET;
    lnk->corder = 0;
    lnk->corder_valid = FALSE;       /* Creation order not valid for this link */
    lnk->name = H5MM_xstrdup(name);

    /* Object is a symbolic or hard link */
    if(ent->type == H5G_CACHED_SLINK) {
        const char *s;          /* Pointer to link value */
        const H5HL_t *heap;     /* Pointer to local heap for group */

        /* Lock the local heap */
        if(NULL == (heap = H5HL_protect(f, dxpl_id, lheap_addr)))
            HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to read protect link value")

        s = H5HL_offset_into(f, heap, ent->cache.slink.lval_offset);

        /* Copy the link value */
        lnk->u.soft.name = H5MM_xstrdup(s);

        /* Release the local heap */
        if(H5HL_unprotect(f, dxpl_id, heap, lheap_addr, H5AC__NO_FLAGS_SET) < 0)
            HGOTO_ERROR(H5E_SYM, H5E_NOTFOUND, FAIL, "unable to read unprotect link value")

        /* Set link type */
        lnk->type = H5L_TYPE_SOFT;
    } /* end if */
    else {
        /* Set address of object */
        lnk->u.hard.addr = ent->header;

        /* Set link type */
        lnk->type = H5L_TYPE_HARD;
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_link_convert() */


/*-------------------------------------------------------------------------
 * Function:	H5G_link_copy_file
 *
 * Purpose:     Copy a link and the object it points to from one file to
 *              another.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Sep 29 2006
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_link_copy_file(H5F_t *dst_file, hid_t dxpl_id, const H5O_link_t *_src_lnk,
    const H5O_loc_t *src_oloc, H5O_link_t *dst_lnk, H5O_copy_t *cpy_info)
{
    H5O_link_t tmp_src_lnk;             /* Temporary copy of src link, when needed */
    const H5O_link_t *src_lnk = _src_lnk; /* Source link */
    hbool_t dst_lnk_init = FALSE;       /* Whether the destination link is initialized */
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_NOAPI(H5G_link_copy_file, FAIL)

    /* check arguments */
    HDassert(dst_file);
    HDassert(src_lnk);
    HDassert(dst_lnk);
    HDassert(cpy_info);

    /* Expand soft link */
    if(H5L_TYPE_SOFT == src_lnk->type && cpy_info->expand_soft_link) {
        H5G_stat_t  statbuf;        /* Information about object pointed to by soft link */
        H5G_loc_t   grp_loc;        /* Group location holding soft link */
        H5G_name_t  grp_path;       /* Path for group holding soft link */

        /* Make a temporary copy, so that it will not change the info in the cache */
        if(NULL == H5O_copy(H5O_LINK_ID, src_lnk, &tmp_src_lnk))
            HGOTO_ERROR(H5E_OHDR, H5E_CANTCOPY, H5B2_ITER_ERROR, "unable to copy message")

        /* Set up group location for soft link to start in */
        H5G_name_reset(&grp_path);
        grp_loc.path = &grp_path;
        grp_loc.oloc = (H5O_loc_t *)src_oloc;    /* Casting away const OK -QAK */

        /* Check if the object pointed by the soft link exists in the source file */
        /* (It would be more efficient to make a specialized traversal callback,
         *      but this is good enough for now... -QAK)
         */
        if(H5G_get_objinfo(&grp_loc, tmp_src_lnk.u.soft.name, TRUE, &statbuf, dxpl_id) >= 0) {
            /* Convert soft link to hard link */
            tmp_src_lnk.u.soft.name = H5MM_xfree(tmp_src_lnk.u.soft.name);
            tmp_src_lnk.type = H5L_TYPE_HARD;
#if H5_SIZEOF_UINT64_T > H5_SIZEOF_LONG
            tmp_src_lnk.u.hard.addr = (((haddr_t)statbuf.objno[1]) << (8 * sizeof(long))) | (haddr_t)statbuf.objno[0];
#else
            tmp_src_lnk.u.hard.addr = statbuf.objno[0];
#endif
            src_lnk = &tmp_src_lnk;
        } /* end if */
        else {
            /* Discard any errors from a dangling soft link */
            H5E_clear_stack(NULL);

            /* Release any information copied for temporary src link */
            H5O_reset(H5O_LINK_ID, &tmp_src_lnk);
        } /* end else */
    } /* end if */

    /* Copy src link information to dst link information */
    if(NULL == H5O_copy(H5O_LINK_ID, src_lnk, dst_lnk))
        HGOTO_ERROR(H5E_OHDR, H5E_CANTCOPY, H5B2_ITER_ERROR, "unable to copy message")
    dst_lnk_init = TRUE;

    /* Check if object in source group is a hard link & copy it */
    if(H5L_TYPE_HARD == src_lnk->type) {
        H5O_loc_t new_dst_oloc;     /* Copied object location in destination */
        H5O_loc_t tmp_src_oloc;     /* Temporary object location for source object */

        /* Set up copied object location to fill in */
        H5O_loc_reset(&new_dst_oloc);
        new_dst_oloc.file = dst_file;

        /* Build temporary object location for source */
        H5O_loc_reset(&tmp_src_oloc);
        tmp_src_oloc.file = src_oloc->file;
        HDassert(H5F_addr_defined(src_lnk->u.hard.addr));
        tmp_src_oloc.addr = src_lnk->u.hard.addr;

        /* Copy the shared object from source to destination */
        if(H5O_copy_header_map(&tmp_src_oloc, &new_dst_oloc, dxpl_id, cpy_info, TRUE) < 0)
            HGOTO_ERROR(H5E_OHDR, H5E_CANTCOPY, H5B2_ITER_ERROR, "unable to copy object")

        /* Copy new destination object's information for eventual insertion */
        dst_lnk->u.hard.addr = new_dst_oloc.addr;
    } /* end if */

done:
    /* Check if we used a temporary src link */
    if(src_lnk != _src_lnk) {
        HDassert(src_lnk == &tmp_src_lnk);
        H5O_reset(H5O_LINK_ID, &tmp_src_lnk);
    } /* end if */
    if(ret_value < 0)
        if(dst_lnk_init)
            H5O_reset(H5O_LINK_ID, dst_lnk);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_link_copy_file() */


/*-------------------------------------------------------------------------
 * Function:	H5G_link_release_table
 *
 * Purpose:     Release table containing a list of links for a group
 *
 * Return:	Success:        Non-negative
 *		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *	        Sep  6, 2005
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5G_link_release_table(H5G_link_table_t *ltable)
{
    size_t      u;                      /* Local index variable */
    herr_t	ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_NOAPI_NOINIT(H5G_link_release_table)

    /* Sanity check */
    HDassert(ltable);

    /* Release link info, if any */
    if(ltable->nlinks > 0) {
        /* Free link message information */
        for(u = 0; u < ltable->nlinks; u++)
            if(H5O_reset(H5O_LINK_ID, &(ltable->lnks[u])) < 0)
                HGOTO_ERROR(H5E_SYM, H5E_CANTFREE, FAIL, "unable to release link message")

        /* Free table of links */
        H5MM_xfree(ltable->lnks);
    } /* end if */
    else
        HDassert(ltable->lnks == NULL);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5G_link_release_table() */

