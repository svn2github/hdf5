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

/****************/
/* Module Setup */
/****************/

#define H5SM_PACKAGE		/*suppress error about including H5SMpkg	  */
#define H5F_PACKAGE		/*suppress error about including H5Fpkg 	  */


/***********/
/* Headers */
/***********/
#include "H5private.h"		/* Generic Functions			*/
#include "H5ACprivate.h"	/* Metadata cache			*/
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5Fpkg.h"		/* File access                          */
#include "H5FLprivate.h"	/* Free Lists                           */
#include "H5MFprivate.h"        /* File memory management		*/
#include "H5MMprivate.h"	/* Memory management			*/
#include "H5SMpkg.h"            /* Shared object header messages        */


/****************/
/* Local Macros */
/****************/
/* Values used to create the SOHM heaps */
#define H5SM_FHEAP_MAN_WIDTH                     4
#define H5SM_FHEAP_MAN_START_BLOCK_SIZE          1024
#define H5SM_FHEAP_MAN_MAX_DIRECT_SIZE           (64 * 1024)
#define H5SM_FHEAP_MAN_MAX_INDEX                 32
#define H5SM_FHEAP_MAN_START_ROOT_ROWS           1
#define H5SM_FHEAP_CHECKSUM_DBLOCKS              TRUE
#define H5SM_FHEAP_MAX_MAN_SIZE                  (4 * 1024)


/******************/
/* Local Typedefs */
/******************/


/********************/
/* Local Prototypes */
/********************/
static herr_t H5SM_create_index(H5F_t *f, H5SM_index_header_t *header, hid_t dxpl_id);
static haddr_t H5SM_create_list(H5F_t *f, H5SM_index_header_t *header, hid_t dxpl_id);
static herr_t H5SM_write_mesg(H5F_t *f, hid_t dxpl_id, H5SM_index_header_t *header,
     unsigned type_id, void *mesg, unsigned *cache_flags_ptr);
static herr_t H5SM_delete_from_index(H5F_t *f, hid_t dxpl_id,
        H5SM_index_header_t *header, unsigned type_id, const H5O_shared_t * mesg,
        unsigned *cache_flags);
static hsize_t H5SM_find_in_list(H5F_t *f, H5SM_list_t *list, const H5SM_mesg_key_t *key);
static herr_t H5SM_type_to_flag(unsigned type_id, unsigned *type_flag);
static ssize_t H5SM_get_index(const H5SM_master_table_t *table, unsigned type_id);


/*********************/
/* Package Variables */
/*********************/

H5FL_DEFINE(H5SM_master_table_t);
H5FL_ARR_DEFINE(H5SM_index_header_t, H5SM_MAX_INDEXES);
H5FL_DEFINE(H5SM_list_t);
H5FL_ARR_DEFINE(H5SM_sohm_t, H5SM_MAX_LIST_ELEMS);


/*****************************/
/* Library Private Variables */
/*****************************/


/*******************/
/* Local Variables */
/*******************/


/*-------------------------------------------------------------------------
 * Function:    H5SM_init
 *
 * Purpose:     Initializes the Shared Message interface.
 *
 *              Creates a master SOHM table in the file and in the cache.
 *              This function should not be called for files that have
 *              SOHMs disabled in the FCPL.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  James Laird
 *              Tuesday, May 2, 2006
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5SM_init(H5F_t *f, H5P_genplist_t * fc_plist, hid_t dxpl_id)
{    
    H5SM_master_table_t *table = NULL;
    haddr_t table_addr = HADDR_UNDEF;
    unsigned num_indexes;
    unsigned list_to_btree, btree_to_list;
    unsigned index_type_flags[H5SM_MAX_NUM_INDEXES];
    unsigned minsizes[H5SM_MAX_NUM_INDEXES];
    unsigned type_flags_used;
    unsigned x;
    hsize_t table_size;
    herr_t ret_value=SUCCEED;

    FUNC_ENTER_NOAPI(H5SM_init, NULL)

    HDassert(f);
    /* File should not already have a SOHM table */
    HDassert(f->shared->sohm_addr == HADDR_UNDEF);

    /* Initialize master table */
    if(NULL == (table = H5FL_MALLOC(H5SM_master_table_t)))
	HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed for SOHM table")

    /* Get information from fcpl */
    if(H5P_get(fc_plist, H5F_CRT_SHMSG_NINDEXES_NAME, &num_indexes)<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't get number of indexes")
    if(H5P_get(fc_plist, H5F_CRT_SHMSG_INDEX_TYPES_NAME, &index_type_flags)<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't get SOHM type flags")
    if(H5P_get(fc_plist, H5F_CRT_SHMSG_LIST_MAX_NAME, &list_to_btree)<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't get SOHM list maximum")
    if(H5P_get(fc_plist, H5F_CRT_SHMSG_BTREE_MIN_NAME, &btree_to_list)<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't get SOHM btree minimum")
    if(H5P_get(fc_plist, H5F_CRT_SHMSG_INDEX_MINSIZE_NAME, &minsizes) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't get SOHM message min sizes")

    /* Verify that values are valid */
    if(num_indexes > H5SM_MAX_NUM_INDEXES)
        HGOTO_ERROR(H5E_PLIST, H5E_BADRANGE, FAIL, "number of indexes in property list is too large")

    /* Check that type flags weren't duplicated anywhere */
    type_flags_used = 0;
    for(x = 0; x < num_indexes; ++x) {
        if(index_type_flags[x] & type_flags_used)
            HGOTO_ERROR(H5E_PLIST, H5E_BADVALUE, FAIL, "the same shared message type flag is assigned to more than one index")
        type_flags_used |= index_type_flags[x];
    }

    /* Set version and number of indexes in table and in superblock.
     * Right now we just use one byte to hold the number of indexes.
     */
    HDassert(num_indexes < 256);
    table->num_indexes = num_indexes;
    table->version = H5SM_MASTER_TABLE_VERSION;

    f->shared->sohm_nindexes = table->num_indexes;
    f->shared->sohm_vers = table->version;

    /* Check that list and btree cutoffs make sense.  There can't be any
     * values greater than the list max but less than the btree min; the
     * list max has to be greater than or equal to one less than the btree
     * min.
     */
    HDassert(list_to_btree + 1 >= btree_to_list);
    HDassert(table->num_indexes > 0 && table->num_indexes <= H5SM_MAX_NUM_INDEXES);

    /* Allocate the SOHM indexes as an array. */
    if(NULL == (table->indexes = H5FL_ARR_MALLOC(H5SM_index_header_t, table->num_indexes)))
	HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed for SOHM indexes")

    /* Initialize all of the indexes, but don't allocate space for them to
     * hold messages until we actually need to write to them.
     */
    for(x=0; x<table->num_indexes; x++)
    {
        table->indexes[x].btree_to_list = btree_to_list;
        table->indexes[x].list_to_btree = list_to_btree;
        table->indexes[x].mesg_types = index_type_flags[x];
        table->indexes[x].min_mesg_size = minsizes[x];
        table->indexes[x].index_addr = HADDR_UNDEF;
        table->indexes[x].heap_addr = HADDR_UNDEF;
        table->indexes[x].num_messages = 0;
        /* Indexes start as lists unless the list-to-btree threshold is zero */
        if(table->indexes[x].list_to_btree > 0) {
            table->indexes[x].index_type = H5SM_LIST;
        } else {
            table->indexes[x].index_type = H5SM_BTREE;
        }
    } /* end for */

    /* Allocate space for the table on disk */
    table_size = (hsize_t) H5SM_TABLE_SIZE(f) + (hsize_t) (table->num_indexes * H5SM_INDEX_HEADER_SIZE(f));
    if(HADDR_UNDEF == (table_addr = H5MF_alloc(f, H5FD_MEM_SOHM_TABLE, dxpl_id, table_size)))
	HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "file allocation failed for SOHM table")

    /* Cache the new table */
    if(H5AC_set(f, dxpl_id, H5AC_SOHM_TABLE, table_addr, table, H5AC__NO_FLAGS_SET) < 0)
	HGOTO_ERROR(H5E_CACHE, H5E_CANTLOAD, FAIL, "can't add SOHM table to cache")

    /* Record the address of the master table in the file */
    f->shared->sohm_addr = table_addr;

done:
    if(ret_value < 0)
    {
        if(table_addr != HADDR_UNDEF)
            H5MF_xfree(f, H5FD_MEM_SOHM_TABLE, dxpl_id, table_addr, (hsize_t)H5SM_TABLE_SIZE(f));
        if(table != NULL)
            H5FL_FREE(H5SM_master_table_t, table);
    }

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_init() */


/*-------------------------------------------------------------------------
 * Function:    H5SM_type_to_flag
 *
 * Purpose:     Get the shared message flag for a given message type.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  James Laird
 *              Tuesday, October 10, 2006
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5SM_type_to_flag(unsigned type_id, unsigned *type_flag)
{
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_NOAPI_NOINIT(H5SM_type_to_flag)

    /* Translate the H5O type_id into an H5SM type flag */
    switch(type_id) {
        case H5O_SDSPACE_ID:
            *type_flag = H5O_MESG_SDSPACE_FLAG;
            break;
        case H5O_DTYPE_ID:
            *type_flag = H5O_MESG_DTYPE_FLAG;
            break;
        case H5O_FILL_NEW_ID:
            *type_flag = H5O_MESG_FILL_FLAG;
            break;
        case H5O_PLINE_ID:
            *type_flag = H5O_MESG_PLINE_FLAG;
            break;
        case H5O_ATTR_ID:
            *type_flag = H5O_MESG_ATTR_FLAG;
            break;
        default:
            HGOTO_ERROR(H5E_OHDR, H5E_BADTYPE, FAIL, "unknown message type ID")
    } /* end switch */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_type_to_flag() */


/*-------------------------------------------------------------------------
 * Function:    H5SM_get_index
 *
 * Purpose:     Get the index number for a given message type.
 *
 *              Returns the number of the index in the supplied table
 *              that holds messages of type type_id, or negative if
 *              there is no index for this message type.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  James Laird
 *              Tuesday, October 10, 2006
 *
 *-------------------------------------------------------------------------
 */
static ssize_t
H5SM_get_index(const H5SM_master_table_t *table, unsigned type_id)
{
    ssize_t x;
    unsigned type_flag;
    ssize_t ret_value = FAIL;

    FUNC_ENTER_NOAPI_NOINIT(H5SM_get_index)

    /* Translate the H5O type_id into an H5SM type flag */
    if(H5SM_type_to_flag(type_id, &type_flag) < 0)
        HGOTO_ERROR(H5E_OHDR, H5E_CANTGET, FAIL, "can't map message type to flag")

    /* Search the indexes until we find one that matches this flag or we've
     * searched them all.
     */
    for(x = 0; x < table->num_indexes; ++x)
    {
        if(table->indexes[x].mesg_types & type_flag)
        {
            ret_value = x;
            break;
        }
    }

    /* At this point, ret_value is either the location of the correct
     * index or it's still FAIL because we didn't find an index.
     */
done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_get_index() */


/*-------------------------------------------------------------------------
 * Function:    H5SM_type_shared
 *
 * Purpose:     Check if a given message type is shared in a file.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  Quincey Koziol
 *              Tuesday, December 12, 2006
 *
 *-------------------------------------------------------------------------
 */
htri_t
H5SM_type_shared(H5F_t *f, unsigned type_id, hid_t dxpl_id)
{
    H5SM_master_table_t *table = NULL;  /* Shared object master table */
    unsigned type_flag;                 /* Flag corresponding to message type */
    size_t u;                           /* Local index variable */
    htri_t ret_value = FALSE;           /* Return value */

    FUNC_ENTER_NOAPI_NOINIT(H5SM_type_shared)

    /* Translate the H5O type_id into an H5SM type flag */
    if(H5SM_type_to_flag(type_id, &type_flag) < 0)
        HGOTO_ERROR(H5E_OHDR, H5E_CANTGET, FAIL, "can't map message type to flag")

    /* Look up the master SOHM table */
    if(H5F_addr_defined(f->shared->sohm_addr)) {
        if(NULL == (table = H5AC_protect(f, dxpl_id, H5AC_SOHM_TABLE, f->shared->sohm_addr, NULL, NULL, H5AC_READ)))
            HGOTO_ERROR(H5E_OHDR, H5E_CANTPROTECT, FAIL, "unable to load SOHM master table")
    } /* end if */
    else
        /* No shared messages of any type */
        HGOTO_DONE(FALSE)       

    /* Search the indexes until we find one that matches this flag or we've
     * searched them all.
     */
    for(u = 0; u < table->num_indexes; u++)
        if(table->indexes[u].mesg_types & type_flag)
            HGOTO_DONE(TRUE)

done:
    /* Release the master SOHM table */
    if(table && H5AC_unprotect(f, dxpl_id, H5AC_SOHM_TABLE, f->shared->sohm_addr, table, H5AC__NO_FLAGS_SET) < 0)
	HDONE_ERROR(H5E_OHDR, H5E_CANTUNPROTECT, FAIL, "unable to close SOHM master table")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_type_shared() */


/*-------------------------------------------------------------------------
 * Function:    H5SM_get_fheap_addr
 *
 * Purpose:     Gets the address of the fractal heap used to store
 *              messages of type type_id.
 *
 * Return:      Non-negative on success/negative on failure
 *
 * Programmer:  James Laird
 *              Tuesday, October 3, 2006
 *
 *-------------------------------------------------------------------------
 */
haddr_t
H5SM_get_fheap_addr(H5F_t *f, unsigned type_id, hid_t dxpl_id)
{
    H5SM_master_table_t *table = NULL;  /* Shared object master table */
    ssize_t index_num;                  /* Which index */
    haddr_t ret_value;

    FUNC_ENTER_NOAPI(H5SM_get_fheap_addr, FAIL)

    /* Look up the master SOHM table */
    if(NULL == (table = H5AC_protect(f, dxpl_id, H5AC_SOHM_TABLE, f->shared->sohm_addr, NULL, NULL, H5AC_READ)))
	HGOTO_ERROR(H5E_CACHE, H5E_CANTPROTECT, HADDR_UNDEF, "unable to load SOHM master table")

    /* JAMES! */
    if((index_num = H5SM_get_index(table, type_id)) < 0)
	HGOTO_ERROR(H5E_SOHM, H5E_CANTPROTECT, HADDR_UNDEF, "unable to find correct SOHM index")

    ret_value = table->indexes[index_num].heap_addr;

done:
    /* Release the master SOHM table */
    if(table && H5AC_unprotect(f, dxpl_id, H5AC_SOHM_TABLE, f->shared->sohm_addr, table, H5AC__NO_FLAGS_SET) < 0)
	HDONE_ERROR(H5E_CACHE, H5E_CANTUNPROTECT, HADDR_UNDEF, "unable to close SOHM master table")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_get_fheap_addr() */


/*-------------------------------------------------------------------------
 * Function:    H5SM_create_index
 *
 * Purpose:     Allocates storage for an index.
 *
 * Return:      Non-negative on success/negative on failure
 *
 * Programmer:  James Laird
 *              Tuesday, May 2, 2006
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5SM_create_index(H5F_t *f, H5SM_index_header_t *header, hid_t dxpl_id)
{
    haddr_t list_addr=HADDR_UNDEF;   /* Address of SOHM list */
    haddr_t tree_addr=HADDR_UNDEF;   /* Address of SOHM B-tree */
    H5HF_create_t fheap_cparam;      /* Fractal heap creation parameters */
    H5HF_t *fheap = NULL;
#ifndef NDEBUG
    size_t fheap_id_len;             /* Size of a fractal heap ID */
#endif /* NDEBUG */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(H5SM_create_index, FAIL)

    HDassert(header);
    HDassert(header->index_addr == HADDR_UNDEF);
    HDassert(header->btree_to_list <= header->list_to_btree);

    /* In most cases, the index starts as a list */
    if(header->list_to_btree > 0)
    {
        header->index_type = H5SM_LIST;

        if((list_addr = H5SM_create_list(f, header, dxpl_id)) == HADDR_UNDEF)  /* JAMES: only allocate part of the list? */
            HGOTO_ERROR(H5E_SOHM, H5E_CANTCREATE, FAIL, "list creation failed for SOHM index")

        header->index_addr = list_addr;
    }
    else /* index is a B-tree */
    {
        header->index_type = H5SM_BTREE;

        if(H5B2_create(f, dxpl_id, H5SM_INDEX, H5SM_B2_NODE_SIZE,
                  H5SM_SOHM_ENTRY_SIZE(f), H5SM_B2_SPLIT_PERCENT,
                  H5SM_B2_MERGE_PERCENT, &tree_addr) <0)
            HGOTO_ERROR(H5E_BTREE, H5E_CANTCREATE, FAIL, "B-tree creation failed for SOHM index")

        header->index_addr = tree_addr;
    }

    /* Create a heap to hold the shared messages that the list or B-tree will index */
    /* JAMES: this should happen first, so that the list/btree size can scale depending
     * on how big a heap pointer is.
     */
    HDmemset(&fheap_cparam, 0, sizeof(fheap_cparam));
    fheap_cparam.managed.width = H5SM_FHEAP_MAN_WIDTH;
    fheap_cparam.managed.start_block_size = H5SM_FHEAP_MAN_START_BLOCK_SIZE;
    fheap_cparam.managed.max_direct_size = H5SM_FHEAP_MAN_MAX_DIRECT_SIZE;
    fheap_cparam.managed.max_index = H5SM_FHEAP_MAN_MAX_INDEX;
    fheap_cparam.managed.start_root_rows = H5SM_FHEAP_MAN_START_ROOT_ROWS;
    fheap_cparam.checksum_dblocks = H5SM_FHEAP_CHECKSUM_DBLOCKS;
    fheap_cparam.id_len = 0;
    fheap_cparam.max_man_size = H5SM_FHEAP_MAX_MAN_SIZE;
    if(NULL == (fheap = H5HF_create(f, dxpl_id, &fheap_cparam)))
        HGOTO_ERROR(H5E_HEAP, H5E_CANTINIT, FAIL, "unable to create fractal heap")

    if(H5HF_get_heap_addr(fheap, &(header->heap_addr )) < 0)
        HGOTO_ERROR(H5E_HEAP, H5E_CANTGETSIZE, FAIL, "can't get fractal heap address")

#ifndef NDEBUG
    /* Sanity check ID length */
    if(H5HF_get_id_len(fheap, &fheap_id_len) < 0)
        HGOTO_ERROR(H5E_HEAP, H5E_CANTGETSIZE, FAIL, "can't get fractal heap ID length")
    HDassert(fheap_id_len == H5SM_FHEAP_ID_LEN);
#endif /* NDEBUG */

done:
    /* Close the fractal heap if one has been created */
    if(fheap && H5HF_close(fheap, dxpl_id) < 0)
        HDONE_ERROR(H5E_HEAP, H5E_CLOSEERROR, FAIL, "can't close fractal heap")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_create_index */


/*-------------------------------------------------------------------------
 * Function:    H5SM_create_list
 *
 * Purpose:     Creates a list of SOHM messages.
 *
 *              Called when a new index is created from scratch or when a
 *              B-tree needs to be converted back into a list.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  James Laird
 *              Monday, August 28, 2006
 *
 *-------------------------------------------------------------------------
 */
static haddr_t
H5SM_create_list(H5F_t *f, H5SM_index_header_t * header, hid_t dxpl_id)
{
    H5SM_list_t *list = NULL; /* List of messages */
    hsize_t x;    /* Counter variable */
    hsize_t size;             /* Size of list on disk */
    size_t num_entries;      /* Number of messages to create in list */
    haddr_t addr = HADDR_UNDEF; /* Address of the list on disk */
    haddr_t ret_value;

    FUNC_ENTER_NOAPI(H5SM_create_list, HADDR_UNDEF)

    HDassert(f);
    HDassert(header);

    num_entries = header->list_to_btree;

    /* Allocate list in memory */
    if((list = H5FL_MALLOC(H5SM_list_t)) == NULL)
	HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, HADDR_UNDEF, "file allocation failed for SOHM list")
    if((list->messages = H5FL_ARR_MALLOC(H5SM_sohm_t, num_entries)) == NULL)
	HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, HADDR_UNDEF, "file allocation failed for SOHM list")

    /* Initialize messages in list */
    HDmemset(list->messages, 0, sizeof(H5SM_sohm_t) * num_entries);

    /* JAMES: would making fewer operations out of this make it faster? */
    for(x=0; x<num_entries; x++) {
        list->messages[x].hash=H5O_HASH_UNDEF;
    }

    list->header = header;

    /* Allocate space for the list on disk */
    size = H5SM_LIST_SIZE(f, num_entries);
    if(HADDR_UNDEF == (addr = H5MF_alloc(f, H5FD_MEM_SOHM_INDEX, dxpl_id, size)))
	HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, HADDR_UNDEF, "file allocation failed for SOHM list")

    /* Put the list into the cache */
    if(H5AC_set(f, dxpl_id, H5AC_SOHM_LIST, addr, list, H5AC__NO_FLAGS_SET) < 0)
	HGOTO_ERROR(H5E_CACHE, H5E_CANTINS, HADDR_UNDEF, "can't add SOHM list to cache")

    /* Set return value */
    ret_value = addr;

done:
    if(ret_value == HADDR_UNDEF)
    {
        if(list != NULL)
        {
            if(list->messages != NULL)
                H5FL_ARR_FREE(H5SM_sohm_t, list->messages);
            H5FL_FREE(H5SM_list_t, list);
        }
        if(addr != HADDR_UNDEF)
            H5MF_xfree(f, H5FD_MEM_SOHM_INDEX, dxpl_id, addr, size);
    }

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_create_list */


/*-------------------------------------------------------------------------
 * Function:    H5SM_try_share
 *
 * Purpose:     Attempts to share an object header message.  If the message
 *              should be shared (if sharing has been enabled and this
 *              message qualified), turns the message into a shared message.
 *
 *              If not, returns FALSE and does nothing.
 *
 *              If this message was already shared, increments its reference
 *              count and leaves it otherwise unchanged.
 *
 * Return:      TRUE if message is now a SOHM
 *              FALSE if this message is not a SOHM
 *              Negative on failure
 *
 * Programmer:  James Laird
 *              Tuesday, May 2, 2006
 *
 *-------------------------------------------------------------------------
 */
htri_t
H5SM_try_share(H5F_t *f, hid_t dxpl_id, unsigned type_id, void *mesg)
{
    size_t              mesg_size;
    htri_t              tri_ret;
    H5SM_master_table_t *table = NULL;
    unsigned            cache_flags = H5AC__NO_FLAGS_SET;
    ssize_t             index_num;
    herr_t              ret_value = TRUE;

    FUNC_ENTER_NOAPI(H5SM_try_share, FAIL)

    /* Check whether this message ought to be shared or not */
    /* If sharing is disabled in this file, don't share the message */
    if(f->shared->sohm_addr == HADDR_UNDEF)
        HGOTO_DONE(FALSE);

    /* Type-specific checks */
    /* JAMES: should this go here? Should there be a "can share" callback? */
    /* QAK: Yes, a "can share" callback would be very good here, this chunk of
     *          code is really violating the encapsulation of the datatype class
     */
    if(type_id == H5O_DTYPE_ID)
    {
        /* Don't share immutable datatypes */
        if((tri_ret = H5T_is_immutable((H5T_t*) mesg)) > 0)
        {
            HGOTO_DONE(FALSE);
        }
        else if(tri_ret <0)
            HGOTO_ERROR(H5E_OHDR, H5E_BADTYPE, FAIL, "can't tell if datatype is immutable")        
        /* Don't share committed datatypes */
        if((tri_ret = H5T_committed((H5T_t*) mesg)) > 0)
        {
            HGOTO_DONE(FALSE);
        }
        else if(tri_ret <0)
            HGOTO_ERROR(H5E_OHDR, H5E_BADTYPE, FAIL, "can't tell if datatype is comitted")        
    }

    /* Look up the master SOHM table */
    if (NULL == (table = H5AC_protect(f, dxpl_id, H5AC_SOHM_TABLE, f->shared->sohm_addr, NULL, NULL, H5AC_WRITE)))
	HGOTO_ERROR(H5E_CACHE, H5E_CANTPROTECT, FAIL, "unable to load SOHM master table")

    /* Find the right index for this message type.  If there is no such index
     * then this type of message isn't shareable
     */
    if((index_num = H5SM_get_index(table, type_id)) < 0)
    {
        H5E_clear_stack(NULL); /*ignore error*/
        HGOTO_DONE(FALSE);
    } /* end if */

    /* If the message isn't big enough, don't bother sharing it */
    if(0 == (mesg_size = H5O_msg_mesg_size(f, type_id, mesg, 0)))
	HGOTO_ERROR(H5E_OHDR, H5E_BADMESG, FAIL, "unable to get OH message size")
    if(mesg_size < table->indexes[index_num].min_mesg_size)
        HGOTO_DONE(FALSE);

    /* At this point, the message will be shared. */

    /* If the index hasn't been allocated yet, create it */
    if(table->indexes[index_num].index_addr == HADDR_UNDEF)
    {
        if(H5SM_create_index(f, &(table->indexes[index_num]), dxpl_id) < 0)
            HGOTO_ERROR(H5E_SOHM, H5E_CANTINIT, FAIL, "unable to create SOHM index")
        cache_flags |= H5AC__DIRTIED_FLAG;
    }

    /* Write the message as a shared message */
    if(H5SM_write_mesg(f, dxpl_id, &(table->indexes[index_num]), type_id, mesg, &cache_flags) < 0)
	HGOTO_ERROR(H5E_SOHM, H5E_CANTINSERT, FAIL, "can't write shared message")

done:
    /* Release the master SOHM table */
    if(table && H5AC_unprotect(f, dxpl_id, H5AC_SOHM_TABLE, f->shared->sohm_addr, table, cache_flags) < 0)
	HDONE_ERROR(H5E_CACHE, H5E_CANTRELEASE, FAIL, "unable to close SOHM master table")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_try_share() */


/*-------------------------------------------------------------------------
 * Function:    H5SM_write_mesg
 *
 * Purpose:     Writes a message to an existing index and change the message
 *              to reflect that it's now shared.
 *
 *              If the message is already in the index, increment its
 *              reference count instead of writing it again and make the
 *              user's copy point to the copy we wrote before.
 *
 *              The index could be a list or a B-tree.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  James Laird
 *              Tuesday, May 2, 2006
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5SM_write_mesg(H5F_t *f, hid_t dxpl_id, H5SM_index_header_t *header,
     unsigned type_id, void *mesg, unsigned *cache_flags_ptr)
{
    H5SM_list_t           *list = NULL;     /* List index */
    H5SM_mesg_key_t       key;      /* Key used to search the index */
    H5O_shared_t          shared;           /* Shared H5O message */
    hsize_t               list_pos;         /* Position in a list index */
    hbool_t               found = FALSE;    /* Was the message in the index? */
    H5HF_t                *fheap = NULL;         /* Fractal heap handle */
    size_t                buf_size;         /* Size of the encoded message */
    void *                encoding_buf=NULL; /* Buffer for encoded message */
    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(H5SM_write_mesg, FAIL)

    HDassert(cache_flags_ptr);
    HDassert(header);
    HDassert(header->index_type != H5SM_BADTYPE);

    /* Set up a shared message so that we can make this message shared once it's
     * written to the index.  This message is always stored to the heap, not to
     * an object header.
     */
    shared.flags = H5O_SHARED_IN_HEAP_FLAG;

    /* Encode the message to be written */
    if((buf_size = H5O_msg_raw_size(f, type_id, mesg)) <= 0)
	HGOTO_ERROR(H5E_OHDR, H5E_BADSIZE, FAIL, "can't find message size")
    if(NULL == (encoding_buf = H5MM_calloc(buf_size)))
	HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate buffer for encoding")
    if(H5O_msg_encode(f, type_id, encoding_buf, mesg) < 0)
	HGOTO_ERROR(H5E_OHDR, H5E_CANTENCODE, FAIL, "can't encode message to be shared")

    /* Open the fractal heap for this index */
    if(NULL == (fheap = H5HF_open(f, dxpl_id, header->heap_addr)))
	HGOTO_ERROR(H5E_HEAP, H5E_CANTOPENOBJ, FAIL, "unable to open fractal heap")

    /* Set up a key for the message to be written */
    key.hash = H5_checksum_lookup3(encoding_buf, buf_size, type_id);
    key.encoding = encoding_buf;
    key.encoding_size = buf_size;
    key.fheap = fheap;
    key.mesg_heap_id = 0; /* JAMES: Message doesn't yet have a heap ID */

    /* Assume the message is already in the index and try to increment its
     * reference count.  If this fails, the message isn't in the index after
     * all and we'll need to add it.
     */
    if(header->index_type == H5SM_LIST)
    {
        /* The index is a list; get it from the cache */
        if (NULL == (list = H5AC_protect(f, dxpl_id, H5AC_SOHM_LIST, header->index_addr, NULL, header, H5AC_WRITE)))
	    HGOTO_ERROR(H5E_CACHE, H5E_CANTPROTECT, FAIL, "unable to load SOHM index")

        /* JAMES: not very effecient (gets hash value twice, searches list twice).  Refactor. */
        /* See if the message is already in the index and get its location */
        /* JAMES: should return a pointer to the message */
        list_pos = H5SM_find_in_list(f, list, &key);
        if(list_pos != UFAIL)
        {
            /* The message was in the index.  Increment its reference count. */
            ++(list->messages[list_pos].ref_count);

            /* Set up the shared location to point to the shared location */
            shared.u.heap_id = list->messages[list_pos].fheap_id;
            found = TRUE;
        }
    }
    else /* Index is a B-tree */
    {
        HDassert(header->index_type == H5SM_BTREE);

        /* If this returns failure, it means that the message wasn't found. */
        /* If it succeeds, the heap_id in the shared struct will be set */
        if(H5B2_modify(f, dxpl_id, H5SM_INDEX, header->index_addr, &key, H5SM_incr_ref, &shared.u.heap_id) >= 0)
	    found = TRUE;
    }

    /* If the message isn't in the list, add it */
    if(!found)
    {
        hsize_t     x;                /* Counter variable */
        size_t      mesg_size;        /* Size of the message on disk */

        /* JAMES: wrap this in a function call? */

        /* Encode the message and get its size */ /* JAMES: already have this */
        if((mesg_size = H5O_msg_raw_size(f, type_id, mesg)) == 0)
	    HGOTO_ERROR(H5E_OHDR, H5E_BADMESG, FAIL, "unable to get size of message")

        /* JAMES: fix memory problem */
        shared.u.heap_id = 0;

        if(H5HF_insert(fheap, dxpl_id, mesg_size, key.encoding, &(shared.u.heap_id)) < 0)
            HGOTO_ERROR(H5E_HEAP, H5E_CANTINSERT, FAIL, "unable to insert message into fractal heap")

        /* Check whether the list has grown enough that it needs to become a B-tree */
        /* JAMES: make this a separate function */
        if(header->index_type == H5SM_LIST && header->num_messages >= header->list_to_btree)
        {
            hsize_t     list_size;        /* Size of list on disk */
            haddr_t     tree_addr;

            if(H5B2_create(f, dxpl_id, H5SM_INDEX, H5SM_B2_NODE_SIZE,
                  H5SM_SOHM_ENTRY_SIZE(f), H5SM_B2_SPLIT_PERCENT,
                  H5SM_B2_MERGE_PERCENT, &tree_addr) <0)
                HGOTO_ERROR(H5E_BTREE, H5E_CANTCREATE, FAIL, "B-tree creation failed for SOHM index")

            /* Insert each record into the new B-tree */
            for(x=0; x<header->list_to_btree; x++)
            {
                /* JAMES: I'd like to stop relying on H5O_HASH_UNDEF */
                if(list->messages[x].hash != H5O_HASH_UNDEF)
                {
                    if(H5B2_insert(f, dxpl_id, H5SM_INDEX, tree_addr, &(list->messages[x])) < 0)
                        HGOTO_ERROR(H5E_BTREE, H5E_CANTINSERT, FAIL, "couldn't add SOHM to B-tree")
                }
            }

            /* Delete the old list */
            HDassert(list);
            if(H5AC_unprotect(f, dxpl_id, H5AC_SOHM_LIST, header->index_addr, list, H5AC__DELETED_FLAG) < 0)
	        HGOTO_ERROR(H5E_CACHE, H5E_CANTUNPROTECT, FAIL, "unable to close SOHM index")
            list = NULL;

            list_size = H5SM_LIST_SIZE(f, header->list_to_btree);
            if(H5MF_xfree(f, H5FD_MEM_SOHM_INDEX, dxpl_id, header->index_addr, list_size) < 0)
	        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "unable to free shared message list")

            header->index_addr = tree_addr;
            header->index_type = H5SM_BTREE;
        }


        /* JAMES: should be H5SM_insert or something */
        /* Find an empty spot in the list for the message JAMES: combine this with the previous traversal */
        /* Insert the new message into the SOHM index */
        if(header->index_type == H5SM_LIST)
        {
            for(x=0; x<header->list_to_btree; x++)
            {
              if(list->messages[x].hash == H5O_HASH_UNDEF) /* JAMES: is this a valid test? */
              {
                  list->messages[x].fheap_id = shared.u.heap_id;
                  list->messages[x].hash = key.hash;
                  list->messages[x].ref_count = 1;
                  break;
              }
            }
        }
        else /* Index is a B-tree */
        {
            H5SM_sohm_t message;
            HDassert(header->index_type == H5SM_BTREE);
            message.fheap_id = shared.u.heap_id;
            message.hash = key.hash;
            message.ref_count = 1;

            if(H5B2_insert(f, dxpl_id, H5SM_INDEX, header->index_addr, &message) < 0)
                HGOTO_ERROR(H5E_BTREE, H5E_CANTINSERT, FAIL, "couldn't add SOHM to B-tree")
        }

        ++(header->num_messages);
        (*cache_flags_ptr) |= H5AC__DIRTIED_FLAG;
    }

    /* Change the original message passed in to reflect that it's now shared */
    if(H5O_msg_set_share(type_id, &shared, mesg) < 0)
        HGOTO_ERROR(H5E_OHDR, H5E_BADMESG, FAIL, "unable to set sharing information")

done:
    /* Release the fractal heap if we opened it */
    if(fheap && H5HF_close(fheap, dxpl_id) < 0)
        HDONE_ERROR(H5E_HEAP, H5E_CLOSEERROR, FAIL, "can't close fractal heap")

    /* If we got a list out of the cache, release it (it is always dirty after writing a message) */
    if(list && H5AC_unprotect(f, dxpl_id, H5AC_SOHM_LIST, header->index_addr, list, H5AC__DIRTIED_FLAG) < 0)
	HDONE_ERROR(H5E_CACHE, H5E_CANTUNPROTECT, FAIL, "unable to close SOHM index")

    if(encoding_buf)
        H5MM_free(encoding_buf);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_write_mesg() */


/*-------------------------------------------------------------------------
 * Function:    H5SM_try_delete
 *
 * Purpose:     Given an object header message that is being deleted,
 *              checks if it is a SOHM.  If so, decrements its reference
 *              count.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  James Laird
 *              Tuesday, May 2, 2006
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5SM_try_delete(H5F_t *f, hid_t dxpl_id, unsigned type_id, const H5O_shared_t *sh_mesg)
{
    H5SM_master_table_t  *table = NULL;
    unsigned              cache_flags = H5AC__NO_FLAGS_SET;
    ssize_t               index_num;
    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(H5SM_try_delete, FAIL)

    HDassert(f);
    HDassert(sh_mesg);

    /* Make sure SHARED_IN_HEAP flag is set; if not, there's no message to delete */
    if(0 == (sh_mesg->flags & H5O_SHARED_IN_HEAP_FLAG))
        HGOTO_DONE(SUCCEED);

    HDassert(f->shared->sohm_addr != HADDR_UNDEF);

    /* Look up the master SOHM table */
    if(NULL == (table = H5AC_protect(f, dxpl_id, H5AC_SOHM_TABLE, f->shared->sohm_addr, NULL, NULL, H5AC_WRITE)))
	HGOTO_ERROR(H5E_CACHE, H5E_CANTPROTECT, FAIL, "unable to load SOHM master table")

    /* Find the correct index and try to delete from it */
    if((index_num = H5SM_get_index(table, type_id)) < 0)
	HGOTO_ERROR(H5E_SOHM, H5E_NOTFOUND, FAIL, "unable to find correct SOHM index")

    /* JAMES: this triggers some warning on heping.  "overflow in implicit constant conversion" */
    if(H5SM_delete_from_index(f, dxpl_id, &(table->indexes[index_num]), type_id, sh_mesg, &cache_flags) < 0)
	HGOTO_ERROR(H5E_SOHM, H5E_CANTDELETE, FAIL, "unable to delete mesage from SOHM index")

done:
    /* Release the master SOHM table */
    if(table && H5AC_unprotect(f, dxpl_id, H5AC_SOHM_TABLE, f->shared->sohm_addr, table, cache_flags) < 0)
	HDONE_ERROR(H5E_CACHE, H5E_CANTRELEASE, FAIL, "unable to close SOHM master table")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_try_delete() */


/*-------------------------------------------------------------------------
 * Function:    H5SM_find_in_list
 *
 * Purpose:     Find a message's location in a list
 *
 * Return:      Number of messages remaining in the index on success
 *              UFAIL if message couldn't be found
 *
 * Programmer:  James Laird
 *              Tuesday, May 2, 2006
 *
 *-------------------------------------------------------------------------
 */
static hsize_t
H5SM_find_in_list(H5F_t *f, H5SM_list_t *list, const H5SM_mesg_key_t *key)
{
    hsize_t              x;
    hsize_t              ret_value = UFAIL;

    FUNC_ENTER_NOAPI_NOFUNC(H5SM_find_in_list)

    HDassert(f);
    HDassert(list);
    HDassert(key);

    for(x = 0; x < list->header->list_to_btree; x++)
    {
        if(0 == H5SM_message_compare(key, &(list->messages[x])))
        {
          ret_value = x;
          break;
        }
    }

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_find_in_list */


/*-------------------------------------------------------------------------
 * Function:    H5SM_delete_from_index
 *
 * Purpose:     Given a SOHM message, delete it from this index.
 * JAMES: is the message necessarily in the index?  Also, name this "dec ref count" or something.
 *
 * Return:      Non-negative on success
 *              Negative on failure
 *
 * Programmer:  James Laird
 *              Tuesday, May 2, 2006
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5SM_delete_from_index(H5F_t *f, hid_t dxpl_id, H5SM_index_header_t *header, unsigned type_id, const H5O_shared_t * mesg, unsigned *cache_flags)
{
    H5SM_list_t     *list = NULL;
    H5SM_mesg_key_t key;
    H5SM_sohm_t     message;
    unsigned char   *buf = NULL;
    size_t          buf_size;
    hsize_t         list_pos;     /* Position of the message in the list */
    H5HF_t         *fheap=NULL;   /* Fractal heap that contains the message */
    herr_t          ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(H5SM_delete_from_index, FAIL)

    HDassert(header);
    HDassert(mesg);
    HDassert(cache_flags);
    HDassert(mesg->flags & H5O_SHARED_IN_HEAP_FLAG);
    
    /* Open the heap that this message is in */
    if(NULL == (fheap=H5HF_open(f, dxpl_id, header->heap_addr)))
	HGOTO_ERROR(H5E_HEAP, H5E_CANTOPENOBJ, FAIL, "unable to open fractal heap")

        /* JAMES: use heap op command */
    /* Get the message size */
    if(H5HF_get_obj_len(fheap, dxpl_id, &(mesg->u.heap_id), &buf_size) < 0)
        HGOTO_ERROR(H5E_HEAP, H5E_CANTGET, FAIL, "can't get message size from fractal heap.")

    /* Allocate a buffer to hold the message */
    if(NULL == (buf = H5MM_malloc(buf_size)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "couldn't allocate memory")

    /* Read the message to get its hash value */
    if(H5HF_read(fheap, dxpl_id, &(mesg->u.heap_id), buf) < 0)
        HGOTO_ERROR(H5E_HEAP, H5E_CANTGET, FAIL, "can't read message from fractal heap.")


    /* Set up key for message to be deleted. */
    key.hash = H5_checksum_lookup3(buf, buf_size, type_id);
    key.encoding = NULL;
    key.encoding_size = 0;
    key.fheap = fheap;      /* JAMES: unused */
    key.mesg_heap_id = mesg->u.heap_id;

    /* Try to find the message in the index */
    if(header->index_type == H5SM_LIST)
    {
        /* If the index is stored as a list, get it from the cache */
        if (NULL == (list = H5AC_protect(f, dxpl_id, H5AC_SOHM_LIST, header->index_addr, NULL, header, H5AC_WRITE)))
	    HGOTO_ERROR(H5E_SOHM, H5E_CANTPROTECT, FAIL, "unable to load SOHM index")

        /* Find the message in the list */
        if((list_pos = H5SM_find_in_list(f, list, &key)) == UFAIL)
	    HGOTO_ERROR(H5E_SOHM, H5E_NOTFOUND, FAIL, "message not in index")

        --(list->messages[list_pos].ref_count);

        /* Copy the message */
        message = list->messages[list_pos];
    }
    else /* Index is a B-tree */
    {
        HDassert(header->index_type == H5SM_BTREE);

        /* If this returns failure, it means that the message wasn't found. 
         * If it succeeds, a copy of the modified message will be returned. */
        if(H5B2_modify(f, dxpl_id, H5SM_INDEX, header->index_addr, &key, H5SM_decr_ref, &message) <0)
	    HGOTO_ERROR(H5E_SOHM, H5E_NOTFOUND, FAIL, "message not in index")
    }

    /* If the ref count is zero, delete the message from the index */
    if(message.ref_count <= 0)
    {
        /* Remove the message from the heap */
        if(H5HF_remove(fheap, dxpl_id, &(message.fheap_id)) < 0)
            HGOTO_ERROR(H5E_SOHM, H5E_CANTREMOVE, FAIL, "unable to remove message from heap")

        if(header->index_type == H5SM_LIST)
        {
            list->messages[list_pos].hash = H5O_HASH_UNDEF;
            list->messages[list_pos].fheap_id = 0;
            list->messages[list_pos].ref_count = 0; /* Just in case */
        }
        else
        {
            if(H5B2_remove(f, dxpl_id, H5SM_INDEX, header->index_addr, &key, NULL, NULL) < 0)
                HGOTO_ERROR(H5E_BTREE, H5E_CANTREMOVE, FAIL, "unable to delete message")
        }

        /* Update the index header, so set its dirty flag */
        --header->num_messages;
        *cache_flags |= H5AC__DIRTIED_FLAG;

        /* If we've just passed the btree-to-list cutoff, convert this B-tree
         * into a list
         */
        /* JAMES: there's an off-by-one error here */
        /* JAMES: make this a separate function */
        if(header->index_type == H5SM_BTREE && header->num_messages < header->btree_to_list)
        {
            /* Remember the btree address for this index; we'll overwrite the
             * address in the index header
             */
            H5SM_index_header_t temp_header;

            /* The protect callback expects a header corresponding to the list
             * index.  Create a "temporary" header to hold the old B-tree
             * index and reset values in the "real" header to point to an
             * empty list index.
             */
            HDmemcpy(&temp_header, header, sizeof(H5SM_index_header_t));
            header->num_messages = 0;
            header->index_type = H5SM_LIST;

            /* Create a new list index */
            if(HADDR_UNDEF == (header->index_addr = H5SM_create_list(f, header, dxpl_id)))
                HGOTO_ERROR(H5E_SOHM, H5E_CANTINIT, FAIL, "unable to create shared message list")

            HDassert(NULL == list);
            if(NULL == (list = H5AC_protect(f, dxpl_id, H5AC_SOHM_LIST, header->index_addr, NULL, header, H5AC_WRITE)))
	        HGOTO_ERROR(H5E_SOHM, H5E_CANTPROTECT, FAIL, "unable to load SOHM index")

            /* Delete the B-tree and have messages copy themselves to the
             * list as they're deleted
             */
            if(H5B2_delete(f, dxpl_id, H5SM_INDEX, temp_header.index_addr, H5SM_convert_to_list_op, list) < 0)
                HGOTO_ERROR(H5E_BTREE, H5E_CANTDELETE, FAIL, "unable to delete B-tree")
        } /* end if */
    } /* end if */

done:
    /* Free the message buffer */
    if(buf)
        H5MM_xfree(buf);

    /* Release the SOHM list */
    if(list && H5AC_unprotect(f, dxpl_id, H5AC_SOHM_LIST, header->index_addr, list, H5AC__DIRTIED_FLAG) < 0)
	HDONE_ERROR(H5E_CACHE, H5E_CANTUNPROTECT, FAIL, "unable to close SOHM index")

    /* Release the fractal heap if we opened it */
    if(fheap && H5HF_close(fheap, dxpl_id) < 0)
        HDONE_ERROR(H5E_HEAP, H5E_CLOSEERROR, FAIL, "can't close fractal heap")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_delete_from_index() */


/*-------------------------------------------------------------------------
 * Function:    H5SM_get_info
 *
 * Purpose:     Get the list-to-btree and btree-to-list cutoff numbers for
 *              an index within the master table.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:  James Laird
 *              Thursday, May 11, 2006
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5SM_get_info(H5F_t *f, unsigned *index_flags, unsigned *minsizes,
    unsigned *list_to_btree, unsigned *btree_to_list, hid_t dxpl_id)
{
    H5SM_master_table_t *table = NULL;
    haddr_t table_addr;
    uint8_t i;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(H5SM_get_info, FAIL)

    HDassert(f);

    /* Convenience variables */
    table_addr = f->shared->sohm_addr;

    HDassert(table_addr != HADDR_UNDEF);
    HDassert(f->shared->sohm_nindexes > 0);

    /* Allocate and initialize the master table structure */
    if(NULL == (table = H5MM_calloc(sizeof(H5SM_master_table_t))))
	HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed")

    table->version = f->shared->sohm_vers;
    table->num_indexes = f->shared->sohm_nindexes;

    /* Read the rest of the SOHM table information from the cache */
    if (NULL == (table = H5AC_protect(f, dxpl_id, H5AC_SOHM_TABLE, table_addr, NULL, table, H5AC_READ)))
        HGOTO_ERROR(H5E_CACHE, H5E_CANTPROTECT, FAIL, "unable to load SOHM master table")

    /* Return info */
    *list_to_btree = table->indexes[0].list_to_btree;
    *btree_to_list = table->indexes[0].btree_to_list;

    /* Get information about the individual SOHM indexes */
    for(i=0; i<table->num_indexes; ++i) {
        index_flags[i] = table->indexes[i].mesg_types;
        minsizes[i] = table->indexes[i].min_mesg_size;
    }

done:
    /* Release the master SOHM table if we took it out of the cache */
    if(table && H5AC_unprotect(f, dxpl_id, H5AC_SOHM_TABLE, table_addr, table, H5AC__NO_FLAGS_SET) < 0)
	HDONE_ERROR(H5E_CACHE, H5E_CANTRELEASE, FAIL, "unable to close SOHM master table")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5SM_get_info() */

