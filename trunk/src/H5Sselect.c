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

/* Programmer:  Quincey Koziol <koziol@ncsa.uiuc.ued>
 *              Friday, May 29, 1998
 *
 * Purpose:	Dataspace selection functions.
 */

#define H5S_PACKAGE		/*suppress error about including H5Spkg	  */

#include "H5private.h"		/* Generic Functions			  */
#include "H5Dprivate.h"         /* Datasets (for their properties) */
#include "H5Eprivate.h"		/* Error handling		  */
#include "H5FLprivate.h"	/* Free Lists	  */
#include "H5Iprivate.h"		/* ID Functions		  */
#include "H5Spkg.h"		/* Dataspace functions			  */
#include "H5Vprivate.h"         /* Vector functions */

/* Interface initialization */
#define PABLO_MASK      H5Sselect_mask
#define INTERFACE_INIT  NULL
static int             interface_initialize_g = 0;

/* Declare external the free list for hssize_t arrays */
H5FL_ARR_EXTERN(hssize_t);

/* Declare a free list to manage arrays of size_t */
H5FL_ARR_DEFINE_STATIC(size_t,-1);

/* Declare a free list to manage arrays of hsize_t */
H5FL_ARR_EXTERN(hsize_t);

/* Declare a free list to manage blocks of single datatype element data */
H5FL_BLK_EXTERN(type_elem);


/*--------------------------------------------------------------------------
 NAME
    H5S_get_vector_size
 PURPOSE
    Gets the size of the I/O vector
 USAGE
    ssize_t H5S_get_vector_size(dxpl_id)
        hid_t dxpl_id;          IN: The dataset transfer property list to query
 RETURNS
    Non-negative number of entries in I/O vector on success, negative on failure
 DESCRIPTION
    Retrieves the number of I/O vector entries to use for a given dataset
    transfer.  If the default dataset property list is used, the default
    number of I/O vectors is returned.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static ssize_t
H5S_get_vector_size(hid_t dxpl_id)
{
    ssize_t ret_value;        /* return value */

    FUNC_ENTER_NOINIT(H5S_get_vector_size);

    if(dxpl_id==H5P_DATASET_XFER_DEFAULT) {
        ret_value=H5D_XFER_HYPER_VECTOR_SIZE_DEF;
    } /* end if */
    else {
        H5P_genplist_t *dx_plist;   /* Dataset transfer property list */

        /* Get the hyperslab vector size */
        if(NULL == (dx_plist = H5I_object(dxpl_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a dataset transfer property list");
        if (H5P_get(dx_plist,H5D_XFER_HYPER_VECTOR_SIZE_NAME,&ret_value)<0)
            HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "unable to get value");
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_get_vector_size() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_copy
 PURPOSE
    Copy a selection from one dataspace to another
 USAGE
    herr_t H5S_select_copy(dst, src)
        H5S_t *dst;  OUT: Pointer to the destination dataspace
        H5S_t *src;  IN: Pointer to the source dataspace
 RETURNS
    Non-negative on success/Negative on failure
 DESCRIPTION
    Copies all the selection information (include offset) from the source
    dataspace to the destination dataspace.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5S_select_copy (H5S_t *dst, const H5S_t *src)
{
    herr_t ret_value=SUCCEED;     /* return value */

    FUNC_ENTER_NOAPI(H5S_select_copy, FAIL);

    /* Check args */
    assert(dst);
    assert(src);

    /* Copy regular fields */
    dst->select=src->select;

/* Need to copy order information still */

    /* Copy offset information */
    if (NULL==(dst->select.offset = H5FL_ARR_CALLOC(hssize_t,src->extent.u.simple.rank)))
        HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed");
    if(src->select.offset!=NULL)
        HDmemcpy(dst->select.offset,src->select.offset,(src->extent.u.simple.rank*sizeof(hssize_t)));

    /* Perform correct type of copy based on the type of selection */
    switch (src->extent.type) {
        case H5S_SCALAR:
            /*nothing needed */
            break;

        case H5S_SIMPLE:
            /* Deep copy extra stuff */
            switch(src->select.type) {
                case H5S_SEL_NONE:
                case H5S_SEL_ALL:
                    /*nothing needed */
                    break;

                case H5S_SEL_POINTS:
                    ret_value=H5S_point_copy(dst,src);
                    break;

                case H5S_SEL_HYPERSLABS:
                    ret_value=H5S_hyper_copy(dst,src);
                    break;

                default:
                    assert("unknown selection type" && 0);
                    break;
            } /* end switch */
            break;

        case H5S_COMPLEX:
            /*void */
            break;

        default:
            assert("unknown data space type" && 0);
            break;
    } /* end switch */

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_copy() */


/*--------------------------------------------------------------------------
 NAME
    H5Sget_select_npoints
 PURPOSE
    Get the number of elements in current selection
 USAGE
    hssize_t H5Sget_select_npoints(dsid)
        hid_t dsid;             IN: Dataspace ID of selection to query
 RETURNS
    Non-negative on success/Negative on failure
 DESCRIPTION
    Returns the number of elements in current selection for dataspace.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
hssize_t
H5Sget_select_npoints(hid_t spaceid)
{
    H5S_t	*space = NULL;      /* Dataspace to modify selection of */
    hssize_t ret_value;         /* return value */

    FUNC_ENTER_API(H5Sget_select_npoints, FAIL);
    H5TRACE1("Hs","i",spaceid);

    /* Check args */
    if (NULL == (space=H5I_object_verify(spaceid, H5I_DATASPACE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a data space");

    ret_value = H5S_get_select_npoints(space);

done:
    FUNC_LEAVE_API(ret_value);
}   /* H5Sget_select_npoints() */


/*--------------------------------------------------------------------------
 NAME
    H5S_get_select_npoints
 PURPOSE
    Get the number of elements in current selection
 USAGE
    hssize_t H5Sget_select_npoints(space)
        H5S_t *space;             IN: Dataspace of selection to query
 RETURNS
    The number of elements in selection on success, 0 on failure
 DESCRIPTION
    Returns the number of elements in current selection for dataspace.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
hssize_t
H5S_get_select_npoints(const H5S_t *space)
{
    hssize_t ret_value;         /* return value */

    FUNC_ENTER_NOAPI(H5S_get_select_npoints, 0);

    /* Check args */
    assert(space);

    ret_value = (*space->select.get_npoints)(space);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_get_select_npoints() */


/*--------------------------------------------------------------------------
 NAME
    H5Sselect_valid
 PURPOSE
    Check whether the selection fits within the extent, with the current
    offset defined.
 USAGE
    htri_t H5Sselect_void(dsid)
        hid_t dsid;             IN: Dataspace ID to query
 RETURNS
    TRUE if the selection fits within the extent, FALSE if it does not and
        Negative on an error.
 DESCRIPTION
    Determines if the current selection at the current offet fits within the
    extent for the dataspace.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
htri_t
H5Sselect_valid(hid_t spaceid)
{
    H5S_t	*space = NULL;      /* Dataspace to modify selection of */
    htri_t ret_value;     /* return value */

    FUNC_ENTER_API(H5Sselect_valid, 0);
    H5TRACE1("b","i",spaceid);

    /* Check args */
    if (NULL == (space=H5I_object_verify(spaceid, H5I_DATASPACE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, 0, "not a data space");

    ret_value = H5S_select_valid(space);

done:
    FUNC_LEAVE_API(ret_value);
}   /* H5Sselect_valid() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_valid
 PURPOSE
    Check whether the selection fits within the extent, with the current
    offset defined.
 USAGE
    htri_t H5S_select_void(space)
        H5S_t *space;           IN: Dataspace to query
 RETURNS
    TRUE if the selection fits within the extent, FALSE if it does not and
        Negative on an error.
 DESCRIPTION
    Determines if the current selection at the current offet fits within the
    extent for the dataspace.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
htri_t
H5S_select_valid(const H5S_t *space)
{
    htri_t ret_value;     /* return value */

    FUNC_ENTER_NOAPI(H5S_select_valid, 0);

    assert(space);

    ret_value = (*space->select.is_valid)(space);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_valid() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_deserialize
 PURPOSE
    Deserialize the current selection from a user-provided buffer into a real
        selection in the dataspace.
 USAGE
    herr_t H5S_select_deserialize(space, buf)
        H5S_t *space;           IN/OUT: Dataspace pointer to place selection into
        uint8 *buf;             IN: Buffer to retrieve serialized selection from
 RETURNS
    Non-negative on success/Negative on failure
 DESCRIPTION
    Deserializes the current selection into a buffer.  (Primarily for retrieving
    from disk).  This routine just hands off to the appropriate routine for each
    type of selection.  The format of the serialized information is shown in
    the H5S_select_serialize() header.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5S_select_deserialize (H5S_t *space, const uint8_t *buf)
{
    const uint8_t *tbuf;    /* Temporary pointer to the selection type */
    uint32_t sel_type;       /* Pointer to the selection type */
    herr_t ret_value=FAIL;  /* return value */

    FUNC_ENTER_NOAPI(H5S_select_deserialize, FAIL);

    assert(space);

    tbuf=buf;
    UINT32DECODE(tbuf, sel_type);
    switch(sel_type) {
        case H5S_SEL_POINTS:         /* Sequence of points selected */
            ret_value=H5S_point_deserialize(space,buf);
            break;

        case H5S_SEL_HYPERSLABS:     /* Hyperslab selection defined */
            ret_value=H5S_hyper_deserialize(space,buf);
            break;

        case H5S_SEL_ALL:            /* Entire extent selected */
            ret_value=H5S_all_deserialize(space,buf);
            break;

        case H5S_SEL_NONE:           /* Nothing selected */
            ret_value=H5S_none_deserialize(space,buf);
            break;

        default:
            break;
    }

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_deserialize() */


/*--------------------------------------------------------------------------
 NAME
    H5Sget_select_bounds
 PURPOSE
    Gets the bounding box containing the selection.
 USAGE
    herr_t H5S_get_select_bounds(space, start, end)
        hid_t dsid;             IN: Dataspace ID of selection to query
        hsize_t *start;         OUT: Starting coordinate of bounding box
        hsize_t *end;           OUT: Opposite coordinate of bounding box
 RETURNS
    Non-negative on success, negative on failure
 DESCRIPTION
    Retrieves the bounding box containing the current selection and places
    it into the user's buffers.  The start and end buffers must be large
    enough to hold the dataspace rank number of coordinates.  The bounding box
    exactly contains the selection, ie. if a 2-D element selection is currently
    defined with the following points: (4,5), (6,8) (10,7), the bounding box
    with be (4, 5), (10, 8).  Calling this function on a "none" selection
    returns fail.
        The bounding box calculations _does_ include the current offset of the
    selection within the dataspace extent.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5Sget_select_bounds(hid_t spaceid, hsize_t *start, hsize_t *end)
{
    H5S_t	*space = NULL;      /* Dataspace to modify selection of */
    herr_t ret_value;        /* return value */

    FUNC_ENTER_API(H5Sget_select_bounds, FAIL);
    H5TRACE3("e","i*h*h",spaceid,start,end);

    /* Check args */
    if(start==NULL || end==NULL)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid pointer");
    if (NULL == (space=H5I_object_verify(spaceid, H5I_DATASPACE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a data space");

    ret_value = (*space->select.bounds)(space,start,end);

done:
    FUNC_LEAVE_API(ret_value);
}   /* H5Sget_select_bounds() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_iter_init
 PURPOSE
    Initializes iteration information for a selection.
 USAGE
    herr_t H5S_select_iter_init(sel_iter, space, elmt_size)
        H5S_sel_iter_t *sel_iter; OUT: Selection iterator to initialize.
        H5S_t *space;           IN: Dataspace object containing selection to
                                    iterate over
        size_t elmt_size;       IN: Size of elements in the selection
 RETURNS
     Non-negative on success, negative on failure.
 DESCRIPTION
    Initialize the selection iterator object to point to the first element
    in the dataspace's selection.
--------------------------------------------------------------------------*/
herr_t
H5S_select_iter_init(H5S_sel_iter_t *sel_iter, const H5S_t *space, size_t elmt_size)
{
    herr_t ret_value=SUCCEED;   /* Return value */

    FUNC_ENTER_NOAPI(H5S_select_iter_init, FAIL);

    /* Check args */
    assert(sel_iter);
    assert(space);
    assert(elmt_size>0);

    /* Initialize common information */

    /* Save the dataspace's rank */
    sel_iter->rank=space->extent.u.simple.rank;

    /* Allocate room for the dataspace dimensions */
    sel_iter->dims = H5FL_ARR_MALLOC(hsize_t,sel_iter->rank);
    assert(sel_iter->dims);

    /* Keep a copy of the dataspace dimensions */
    HDmemcpy(sel_iter->dims,space->extent.u.simple.size,sel_iter->rank*sizeof(hsize_t));

    /* Call initialization routine for selection type */
    ret_value= (*space->select.iter_init)(sel_iter, space, elmt_size);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_iter_init() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_iter_coords
 PURPOSE
    Get the coordinates of the current iterator position
 USAGE
    herr_t H5S_select_iter_coords(sel_iter,coords)
        H5S_sel_iter_t *sel_iter; IN: Selection iterator to query
        hssize_t *coords;         OUT: Array to place iterator coordinates in
 RETURNS
    Non-negative on success, negative on failure.
 DESCRIPTION
    The current location of the iterator within the selection is placed in
    the COORDS array.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5S_select_iter_coords (const H5S_sel_iter_t *sel_iter, hssize_t *coords)
{
    herr_t ret_value;         /* return value */

    FUNC_ENTER_NOAPI(H5S_select_iter_coords, FAIL);

    /* Check args */
    assert(sel_iter);
    assert(coords);

    /* Call iter_coords routine for selection type */
    ret_value = (*sel_iter->iter_coords)(sel_iter,coords);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_iter_coords() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_iter_nelmts
 PURPOSE
    Get the number of elements left to iterate over in selection
 USAGE
    hssize_t H5S_select_iter_nelmts(sel_iter)
        H5S_sel_iter_t *sel_iter; IN: Selection iterator to query
 RETURNS
    The number of elements in selection on success, 0 on failure
 DESCRIPTION
    Returns the number of elements in current selection for dataspace.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
hsize_t
H5S_select_iter_nelmts (const H5S_sel_iter_t *sel_iter)
{
    hsize_t ret_value;         /* return value */

    FUNC_ENTER_NOAPI(H5S_select_iter_nelmts, 0);

    /* Check args */
    assert(sel_iter);

    /* Call iter_nelmts routine for selection type */
    ret_value = (*sel_iter->iter_nelmts)(sel_iter);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_iter_nelmts() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_iter_next
 PURPOSE
    Advance seletion iterator
 USAGE
    herr_t H5S_select_iter_next(iter, nelem)
        H5S_sel_iter_t *iter;   IN/OUT: Selection iterator to change
        size_t nelem;           IN: Number of elements to advance by
 RETURNS
    Non-negative on success, negative on failure.
 DESCRIPTION
    Move the current element for the selection iterator to the NELEM'th next
    element in the selection.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5S_select_iter_next(H5S_sel_iter_t *iter, size_t nelem)
{
    herr_t ret_value;         /* return value */

    FUNC_ENTER_NOAPI(H5S_select_iter_next, FAIL);

    /* Check args */
    assert(iter);
    assert(nelem>0);

    /* Call iter_coords routine for selection type */
    ret_value = (*iter->iter_next)(iter,nelem);

    /* Decrement the number of elements left in selection */
    iter->elmt_left-=nelem;

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_iter_next() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_iter_release
 PURPOSE
    Release a selection iterator's resources.
 USAGE
    hssize_t H5S_select_iter_release(sel_iter)
        H5S_sel_iter_t *sel_iter; IN: Selection iterator to query
 RETURNS
    The number of elements in selection on success, 0 on failure
 DESCRIPTION
    Returns the number of elements in current selection for dataspace.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5S_select_iter_release(H5S_sel_iter_t *sel_iter)
{
    herr_t ret_value;         /* return value */

    FUNC_ENTER_NOAPI(H5S_select_iter_release, FAIL);

    /* Check args */
    assert(sel_iter);

    /* Release the array of dimensions common to all iterators */
    H5FL_ARR_FREE(hsize_t,sel_iter->dims);

    /* Call selection type-specific release routine */
    ret_value = (*sel_iter->iter_release)(sel_iter);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_iter_release() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_iterate
 PURPOSE
    Iterate over the selected elements in a memory buffer.
 USAGE
    herr_t H5S_select_iterate(buf, type_id, space, operator, operator_data)
        void *buf;      IN/OUT: Buffer containing elements to iterate over
        hid_t type_id;  IN: Datatype ID of BUF array.
        H5S_t *space;   IN: Dataspace object containing selection to iterate over
        H5D_operator_t op; IN: Function pointer to the routine to be
                                called for each element in BUF iterated over.
        void *operator_data;    IN/OUT: Pointer to any user-defined data
                                associated with the operation.
 RETURNS
    Returns the return value of the last operator if it was non-zero, or zero
    if all elements were processed. Otherwise returns a negative value.
 DESCRIPTION
    Iterates over the selected elements in a memory buffer, calling the user's
    callback function for each element.  The selection in the dataspace is
    modified so that any elements already iterated over are removed from the
    selection if the iteration is interrupted (by the H5D_operator_t function
    returning non-zero) in the "middle" of the iteration and may be re-started
    by the user where it left off.

    NOTE: Until "subtracting" elements from a selection is implemented,
        the selection is not modified.
--------------------------------------------------------------------------*/
herr_t
H5S_select_iterate(void *buf, hid_t type_id, const H5S_t *space, H5D_operator_t op,
        void *operator_data)
{
    H5T_t *dt;                  /* Datatype structure */
    H5S_sel_iter_t iter;        /* Selection iteration info */
    hbool_t iter_init=0;        /* Selection iteration info has been initialized */
    uint8_t *loc;               /* Current element location in buffer */
    hssize_t coords[H5O_LAYOUT_NDIMS];  /* Coordinates of element in dataspace */
    hssize_t nelmts;            /* Number of elements in selection */
    hsize_t space_size[H5O_LAYOUT_NDIMS]; /* Dataspace size */
    hsize_t *off=NULL;          /* Array to store sequence offsets */
    hsize_t curr_off;           /* Current offset within sequence */
    hsize_t tmp_off;            /* Temporary offset within sequence */
    size_t *len=NULL;           /* Array to store sequence lengths */
    size_t curr_len;            /* Length of bytes left to process in sequence */
    ssize_t vector_size;        /* Value for vector size */
    size_t nseq;                /* Number of sequences generated */
    size_t curr_seq;            /* Current sequnce being worked on */
    size_t nbytes;              /* Number of bytes used in sequences */
    size_t max_bytes;           /* Maximum number of bytes allowed in sequences */
    size_t elmt_size;           /* Datatype size */
    int ndims;                  /* Number of dimensions in dataspace */
    int	i;			/* Local Index variable */
    herr_t user_ret=0;          /* User's return value */
    herr_t ret_value=SUCCEED;   /* Return value */

    FUNC_ENTER_NOAPI(H5S_select_iterate, FAIL);

    /* Check args */
    assert(buf);
    assert(H5I_DATATYPE == H5I_get_type(type_id));
    assert(space);
    assert(op);

    /* Get the hyperslab vector size */
    /* (from the default data transfer property list, for now) */
    if((vector_size=H5S_get_vector_size(H5P_DATASET_XFER_DEFAULT))<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "unable to get I/O vector size");

    /* Allocate the vector I/O arrays */
    if((len = H5FL_ARR_MALLOC(size_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O length vector array");
    if((off = H5FL_ARR_MALLOC(hsize_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O offset vector array");

    /* Get the datatype size */
    if (NULL==(dt=H5I_object_verify(type_id,H5I_DATATYPE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not an valid base datatype");
    if((elmt_size=H5T_get_size(dt))==0)
        HGOTO_ERROR(H5E_DATATYPE, H5E_BADSIZE, FAIL, "datatype size invalid");

    /* Initialize iterator */
    if (H5S_select_iter_init(&iter, space, elmt_size)<0)
        HGOTO_ERROR (H5E_DATASPACE, H5E_CANTINIT, FAIL, "unable to initialize selection iterator");
    iter_init=1;	/* Selection iteration info has been initialized */

    /* Get the number of elements in selection */
    if((nelmts = (*space->select.get_npoints)(space))<0)
        HGOTO_ERROR (H5E_DATASPACE, H5E_CANTCOUNT, FAIL, "can't get number of elements selected");

    /* Get the rank of the dataspace */
    ndims=space->extent.u.simple.rank;

    if (ndims > 0){
	/* Copy the size of the space */
	assert(space->extent.u.simple.size);
	HDmemcpy(space_size, space->extent.u.simple.size, ndims*sizeof(hsize_t));
    }
    space_size[ndims]=elmt_size;

    /* Compute the maximum number of bytes required */
    H5_ASSIGN_OVERFLOW(max_bytes,nelmts*elmt_size,hsize_t,size_t);

    /* Loop, while elements left in selection */
    while(max_bytes>0 && user_ret==0) {
        /* Get the sequences of bytes */
        if((*space->select.get_seq_list)(space,0,&iter,elmt_size,(size_t)vector_size,max_bytes,&nseq,&nbytes,off,len)<0)
            HGOTO_ERROR (H5E_INTERNAL, H5E_UNSUPPORTED, FAIL, "sequence length generation failed");

        /* Loop, while sequences left to process */
        for(curr_seq=0; curr_seq<nseq && user_ret==0; curr_seq++) {
            /* Get the current offset */
            curr_off=off[curr_seq];

            /* Get the number of bytes in sequence */
            curr_len=len[curr_seq];

            /* Loop, while bytes left in sequence */
            while(curr_len>0 && user_ret==0) {
                /* Compute the coordinate from the offset */
                for(i=ndims, tmp_off=curr_off; i>=0; i--) {
                    coords[i]=tmp_off%space_size[i];
                    tmp_off/=space_size[i];
                } /* end for */

                /* Get the location within the user's buffer */
                loc=(unsigned char *)buf+curr_off;

                /* Call user's callback routine */
                user_ret=(*op)(loc,type_id,(hsize_t)ndims,coords,operator_data);

                /* Increment offset in dataspace */
                curr_off+=elmt_size;

                /* Decrement number of bytes left in sequence */
                curr_len-=elmt_size;
            } /* end while */
        } /* end for */

        /* Decrement number of elements left to process */
        assert((nbytes%elmt_size)==0);
        max_bytes-=nbytes;
    } /* end while */

    /* Set return value */
    ret_value=user_ret;

done:
    /* Release selection iterator */
    if(iter_init) {
        if (H5S_select_iter_release(&iter)<0)
            HDONE_ERROR (H5E_DATASPACE, H5E_CANTRELEASE, FAIL, "unable to release selection iterator");
    } /* end if */

    /* Release length & offset vectors */
    if(len!=NULL)
        H5FL_ARR_FREE(size_t,len);
    if(off!=NULL)
        H5FL_ARR_FREE(hsize_t,off);

    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5S_select_iterate() */


/*--------------------------------------------------------------------------
 NAME
    H5Sget_select_type
 PURPOSE
    Retrieve the type of selection in a dataspace
 USAGE
    H5S_sel_type H5Sget_select_type(space_id)
        hid_t space_id;	        IN: Dataspace object to query
 RETURNS
    Non-negative on success/Negative on failure.  Return value is from the
    set of values in the H5S_sel_type enumerated type.
 DESCRIPTION
	This function retrieves the type of selection currently defined for
    a dataspace.
--------------------------------------------------------------------------*/
H5S_sel_type
H5Sget_select_type(hid_t space_id)
{
    H5S_t		   *space = NULL;	/* dataspace to modify */
    H5S_sel_type        ret_value;       /* Return value */

    FUNC_ENTER_API(H5Sget_select_type, H5S_SEL_ERROR);
    H5TRACE1("St","i",space_id);

    /* Check args */
    if (NULL == (space = H5I_object_verify(space_id, H5I_DATASPACE)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, H5S_SEL_ERROR, "not a data space");

    /* Set return value */
    ret_value=H5S_get_select_type(space);

done:
    FUNC_LEAVE_API(ret_value);
}   /* end H5Sget_select_type() */


/*--------------------------------------------------------------------------
 NAME
    H5S_get_select_type
 PURPOSE
    Retrieve the type of selection in a dataspace
 USAGE
    H5S_sel_type H5Sget_select_type(space)
        const H5S_t *space;	        IN: Dataspace object to query
 RETURNS
    Non-negative on success/Negative on failure.  Return value is from the
    set of values in the H5S_sel_type enumerated type.
 DESCRIPTION
	This function retrieves the type of selection currently defined for
    a dataspace.
--------------------------------------------------------------------------*/
H5S_sel_type
H5S_get_select_type(const H5S_t *space)
{
    H5S_sel_type        ret_value;       /* Return value */

    FUNC_ENTER_NOAPI(H5S_get_select_type, H5S_SEL_ERROR);

    /* Check args */
    assert(space);

    /* Set return value */
    ret_value=space->select.type;

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5S_get_select_type() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_shape_same
 PURPOSE
    Check if two selections are the same shape
 USAGE
    htri_t H5S_select_shape_same(space1, space2)
        const H5S_t *space1;         IN: 1st Dataspace pointer to compare
        const H5S_t *space2;         IN: 2nd Dataspace pointer to compare
 RETURNS
    TRUE/FALSE/FAIL
 DESCRIPTION
    Checks to see if the current selection in the dataspaces are the same
    dimensionality and shape.
    This is primarily used for reading the entire selection in one swoop.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    Assumes that there is only a single "block" for hyperslab selections.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
htri_t
H5S_select_shape_same(const H5S_t *space1, const H5S_t *space2)
{
    H5S_hyper_span_t *span1=NULL,*span2=NULL;   /* Hyperslab span node */
    hsize_t	elmts1=0,elmts2=0;              /* Number of elements in each dimension of selection */
    unsigned	u;                              /* Index variable */
    htri_t ret_value=TRUE;  /* return value */

    FUNC_ENTER_NOAPI(H5S_select_shape_same, FAIL);

    /* Check args */
    assert(space1);
    assert(space2);

    if (space1->extent.u.simple.rank!=space2->extent.u.simple.rank)
        HGOTO_DONE(FALSE);

    /* Get information about memory and file */
    for (u=0; u<space1->extent.u.simple.rank; u++) {
        switch(space1->select.type) {
            case H5S_SEL_HYPERSLABS:
                /* Check size hyperslab selection in this dimension */
                if(space1->select.sel_info.hslab.diminfo != NULL) {
                    elmts1=space1->select.sel_info.hslab.diminfo[u].block;
                } /* end if */
                else {
                    /* Check for the first dimension */
                    if(span1==NULL)
                        span1=space1->select.sel_info.hslab.span_lst->head;

                    /* Get the number of elements in the span */
                    elmts1=(span1->high-span1->low)+1;

                    /* Advance to the next dimension */
                    span1=span1->down->head;
                } /* end else */
                break;

            case H5S_SEL_ALL:
                elmts1=space1->extent.u.simple.size[u];
                break;

            case H5S_SEL_POINTS:
                elmts1=1;
                break;

            case H5S_SEL_NONE:
                elmts1=0;
                break;

            default:
                assert(0 && "Invalid selection type!");
        } /* end switch */

        switch(space2->select.type) {
            case H5S_SEL_HYPERSLABS:
                /* Check size hyperslab selection in this dimension */
                if(space2->select.sel_info.hslab.diminfo != NULL) {
                    elmts2=space2->select.sel_info.hslab.diminfo[u].block;
                } /* end if */
                else {
                    /* Check for the first dimension */
                    if(span2==NULL)
                        span2=space2->select.sel_info.hslab.span_lst->head;

                    /* Get the number of elements in the span */
                    elmts2=(span2->high-span2->low)+1;

                    /* Advance to the next dimension */
                    span2=span2->down->head;
                } /* end else */
                break;

            case H5S_SEL_ALL:
                elmts2=space2->extent.u.simple.size[u];
                break;

            case H5S_SEL_POINTS:
                elmts2=1;
                break;

            case H5S_SEL_NONE:
                elmts2=0;
                break;

            default:
                assert(0 && "Invalid selection type!");
        } /* end switch */

        /* Make certaint the selections have the same number of elements in this dimension */
        if (elmts1!=elmts2)
            HGOTO_DONE(FALSE);
    } /* end for */

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_shape_same() */


/*--------------------------------------------------------------------------
 NAME
    H5S_select_fill
 PURPOSE
    Fill a selection in memory with a value
 USAGE
    herr_t H5S_select_fill(fill,fill_size,space,buf)
        const void *fill;       IN: Pointer to fill value to use
        size_t fill_size;       IN: Size of elements in memory buffer & size of
                                    fill value
        H5S_t *space;           IN: Dataspace describing memory buffer &
                                    containing selection to use.
        void *buf;              IN/OUT: Memory buffer to fill selection in
 RETURNS
    Non-negative on success/Negative on failure.
 DESCRIPTION
    Use the selection in the dataspace to fill elements in a memory buffer.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
    The memory buffer elements are assumed to have the same datatype as the
    fill value being placed into them.
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
herr_t
H5S_select_fill(void *_fill, size_t fill_size, const H5S_t *space, void *_buf)
{
    H5S_sel_iter_t iter;        /* Selection iteration info */
    hbool_t iter_init=0;        /* Selection iteration info has been initialized */
    uint8_t *buf;               /* Current location in buffer */
    void *fill=_fill;           /* Alias for fill-value buffer */
    hssize_t nelmts;            /* Number of elements in selection */
    hsize_t *off=NULL;          /* Array to store sequence offsets */
    size_t *len=NULL;           /* Array to store sequence lengths */
    ssize_t vector_size;        /* Value for vector size */
    size_t nseq;                /* Number of sequences generated */
    size_t curr_seq;            /* Current sequnce being worked on */
    size_t nbytes;              /* Number of bytes used in sequences */
    size_t max_bytes;           /* Total number of bytes in selection */
    herr_t ret_value=SUCCEED;   /* return value */

    FUNC_ENTER_NOAPI(H5S_select_fill, FAIL);

    /* Check args */
    assert(fill_size>0);
    assert(space);
    assert(_buf);

    /* Check if we need a temporary fill value buffer */
    if(fill==NULL) {
        if (NULL==(fill = H5FL_BLK_CALLOC(type_elem,fill_size)))
            HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, FAIL, "fill value buffer allocation failed");
    } /* end if */

    /* Get the hyperslab vector size */
    /* (from the default data transfer property list, for now) */
    if((vector_size=H5S_get_vector_size(H5P_DATASET_XFER_DEFAULT))<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "unable to get I/O vector size");

    /* Allocate the vector I/O arrays */
    if((len = H5FL_ARR_MALLOC(size_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O length vector array");
    if((off = H5FL_ARR_MALLOC(hsize_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O offset vector array");

    /* Initialize iterator */
    if (H5S_select_iter_init(&iter, space, fill_size)<0)
        HGOTO_ERROR (H5E_DATASPACE, H5E_CANTINIT, FAIL, "unable to initialize selection iterator");
    iter_init=1;	/* Selection iteration info has been initialized */

    /* Get the number of elements in selection */
    if((nelmts = (*space->select.get_npoints)(space))<0)
        HGOTO_ERROR (H5E_DATASPACE, H5E_CANTCOUNT, FAIL, "can't get number of elements selected");

    /* Compute the number of bytes to process */
    H5_CHECK_OVERFLOW(nelmts,hssize_t,size_t);
    max_bytes=(size_t)nelmts*fill_size;

    /* Loop, while elements left in selection */
    while(max_bytes>0) {
        /* Get the sequences of bytes */
        if((*space->select.get_seq_list)(space,0,&iter,fill_size,(size_t)vector_size,max_bytes,&nseq,&nbytes,off,len)<0)
            HGOTO_ERROR (H5E_INTERNAL, H5E_UNSUPPORTED, FAIL, "sequence length generation failed");

        /* Loop over sequences */
        for(curr_seq=0; curr_seq<nseq; curr_seq++) {
            /* Get offset in memory buffer */
            buf=(uint8_t *)_buf+off[curr_seq];

            /* Fill each sequence in memory with fill value */
            assert((len[curr_seq]%fill_size)==0);
            H5V_array_fill(buf, fill, fill_size, (len[curr_seq]/fill_size));
        } /* end for */

        /* Decrement number of bytes left to process */
        max_bytes-=nbytes;
    } /* end while */

done:
    /* Release selection iterator */
    if(iter_init) {
        if (H5S_select_iter_release(&iter)<0)
            HDONE_ERROR (H5E_DATASPACE, H5E_CANTRELEASE, FAIL, "unable to release selection iterator");
    } /* end if */

    /* Release length & offset vectors */
    if(len!=NULL)
        H5FL_ARR_FREE(size_t,len);
    if(off!=NULL)
        H5FL_ARR_FREE(hsize_t,off);

    /* Release fill value, if allocated */
    if(_fill==NULL && fill)
        H5FL_BLK_FREE(type_elem,fill);

    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_fill() */


/*-------------------------------------------------------------------------
 * Function:	H5S_select_fscat
 *
 * Purpose:	Scatters dataset elements from the type conversion buffer BUF
 *		to the file F where the data points are arranged according to
 *		the file data space FILE_SPACE and stored according to
 *		LAYOUT and EFL. Each element is ELMT_SIZE bytes.
 *		The caller is requesting that NELMTS elements are copied.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *              Thursday, June 20, 2002
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5S_select_fscat (H5F_t *f, struct H5O_layout_t *layout,
    H5P_genplist_t *dc_plist, const H5D_storage_t *store, 
    size_t elmt_size, const H5S_t *space, H5S_sel_iter_t *iter, 
    hsize_t nelmts, hid_t dxpl_id, const void *_buf)
{
    const uint8_t *buf=_buf;       /* Alias for pointer arithmetic */
    hsize_t *off=NULL;             /* Array to store sequence offsets */
    hsize_t mem_off;               /* Offset in memory */
    size_t mem_curr_seq;           /* "Current sequence" in memory */
    size_t dset_curr_seq;          /* "Current sequence" in dataset */
    size_t *len=NULL;              /* Array to store sequence lengths */
    size_t mem_len;                /* Length of sequence in memory */
    ssize_t vector_size;           /* Value for vector size */
    size_t maxbytes;               /* Number of bytes in the buffer */
    size_t  nseq;                  /* Number of sequences generated */
    size_t  nbytes;                /* Number of bytes used in sequences */
    herr_t  ret_value=SUCCEED;     /* Return value */

    FUNC_ENTER_NOAPI(H5S_select_fscat, FAIL);

    /* Check args */
    assert (f);
    assert (layout);
    assert (elmt_size>0);
    assert (store);
    assert (space);
    assert (iter);
    assert (nelmts>0);
    assert (_buf);
    assert(TRUE==H5P_isa_class(dxpl_id,H5P_DATASET_XFER));

    /* Get the hyperslab vector size */
    if((vector_size=H5S_get_vector_size(dxpl_id))<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "unable to get I/O vector size");

    /* Allocate the vector I/O arrays */
    if((len = H5FL_ARR_MALLOC(size_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O length vector array");
    if((off = H5FL_ARR_MALLOC(hsize_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O offset vector array");

    /* Compute the number of bytes available in buffer */
    H5_ASSIGN_OVERFLOW(maxbytes,nelmts*elmt_size,hsize_t,size_t);

    /* Loop until all elements are written */
    while(maxbytes>0) {
        /* Get list of sequences for selection to write */
        if((*space->select.get_seq_list)(space,H5S_GET_SEQ_LIST_SORTED,iter,elmt_size,(size_t)vector_size,maxbytes,&nseq,&nbytes,off,len)<0)
            HGOTO_ERROR (H5E_INTERNAL, H5E_UNSUPPORTED, FAIL, "sequence length generation failed");

        /* Reset the current sequence information */
        mem_curr_seq=dset_curr_seq=0;
        mem_len=nbytes;
        mem_off=0;

        /* Write sequence list out */
        if (H5F_seq_writevv(f, dxpl_id, layout, dc_plist, store, nseq, &dset_curr_seq, len, off, 1, &mem_curr_seq, &mem_len, &mem_off, buf)<0)
            HGOTO_ERROR(H5E_DATASPACE, H5E_WRITEERROR, FAIL, "write error");

        /* Update buffer */
        buf += nbytes;

        /* Decrement number of elements left to process */
        assert(nbytes%elmt_size==0);
        maxbytes -= nbytes;
    } /* end while */

done:
    if(len!=NULL)
        H5FL_ARR_FREE(size_t,len);
    if(off!=NULL)
        H5FL_ARR_FREE(hsize_t,off);
    FUNC_LEAVE_NOAPI(ret_value);
} /* H5S_select_fscat() */


/*-------------------------------------------------------------------------
 * Function:	H5S_select_fgath
 *
 * Purpose:	Gathers data points from file F and accumulates them in the
 *		type conversion buffer BUF.  The LAYOUT argument describes
 *		how the data is stored on disk and EFL describes how the data
 *		is organized in external files.  ELMT_SIZE is the size in
 *		bytes of a datum which this function treats as opaque.
 *		FILE_SPACE describes the data space of the dataset on disk
 *		and the elements that have been selected for reading (via
 *		hyperslab, etc).  This function will copy at most NELMTS
 *		elements.
 *
 * Return:	Success:	Number of elements copied.
 *		Failure:	0
 *
 * Programmer:	Quincey Koziol
 *              Monday, June 24, 2002
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
hsize_t
H5S_select_fgath (H5F_t *f, const struct H5O_layout_t *layout,
    H5P_genplist_t *dc_plist, const H5D_storage_t *store, 
    size_t elmt_size, const H5S_t *space, H5S_sel_iter_t *iter, 
    hsize_t nelmts, hid_t dxpl_id, void *_buf/*out*/)
{
    uint8_t *buf=_buf;          /* Alias for pointer arithmetic */
    hsize_t *off=NULL;          /* Array to store sequence offsets */
    hsize_t mem_off;            /* Offset in memory */
    size_t mem_curr_seq;        /* "Current sequence" in memory */
    size_t dset_curr_seq;       /* "Current sequence" in dataset */
    size_t *len=NULL;           /* Array to store sequence lengths */
    size_t mem_len;             /* Length of sequence in memory */
    ssize_t vector_size;        /* Value for vector size */
    size_t maxbytes;            /* Number of bytes in the buffer */
    size_t nseq;                /* Number of sequences generated */
    size_t nbytes;              /* Number of bytes used in sequences */
    hsize_t ret_value=nelmts;   /* Return value */

    FUNC_ENTER_NOAPI(H5S_select_fgath, 0);

    /* Check args */
    assert (f);
    assert (layout);
    assert (elmt_size>0);
    assert (store);
    assert (space);
    assert (iter);
    assert (nelmts>0);
    assert (_buf);

    /* Get the hyperslab vector size */
    if((vector_size=H5S_get_vector_size(dxpl_id))<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, 0, "unable to get I/O vector size");

    /* Allocate the vector I/O arrays */
    if((len = H5FL_ARR_MALLOC(size_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, 0, "can't allocate I/O length vector array");
    if((off = H5FL_ARR_MALLOC(hsize_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, 0, "can't allocate I/O offset vector array");

    /* Compute the number of bytes available in buffer */
    H5_ASSIGN_OVERFLOW(maxbytes,nelmts*elmt_size,hsize_t,size_t);

    /* Loop until all elements are written */
    while(maxbytes>0) {
        /* Get list of sequences for selection to write */
        if((*space->select.get_seq_list)(space,H5S_GET_SEQ_LIST_SORTED,iter,elmt_size,(size_t)vector_size,maxbytes,&nseq,&nbytes,off,len)<0)
            HGOTO_ERROR (H5E_INTERNAL, H5E_UNSUPPORTED, 0, "sequence length generation failed");

        /* Reset the current sequence information */
        mem_curr_seq=dset_curr_seq=0;
        mem_len=nbytes;
        mem_off=0;

        /* Read sequence list in */
        if (H5F_seq_readvv(f, dxpl_id, layout, dc_plist, store, nseq, &dset_curr_seq, len, off, 1, &mem_curr_seq, &mem_len, &mem_off, buf)<0)
            HGOTO_ERROR(H5E_DATASPACE, H5E_READERROR, 0, "read error");

        /* Update buffer */
        buf += nbytes;

        /* Decrement number of elements left to process */
        assert(nbytes%elmt_size==0);
        maxbytes -= nbytes;
    } /* end while */

done:
    if(len!=NULL)
        H5FL_ARR_FREE(size_t,len);
    if(off!=NULL)
        H5FL_ARR_FREE(hsize_t,off);
    FUNC_LEAVE_NOAPI(ret_value);
} /* H5S_select_fgath() */


/*-------------------------------------------------------------------------
 * Function:	H5S_select_mscat
 *
 * Purpose:	Scatters NELMTS data points from the scatter buffer
 *		TSCAT_BUF to the application buffer BUF.  Each element is
 *		ELMT_SIZE bytes and they are organized in application memory
 *		according to SPACE.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *              Monday, July 8, 2002
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5S_select_mscat (const void *_tscat_buf, size_t elmt_size, const H5S_t *space,
    H5S_sel_iter_t *iter, hsize_t nelmts, hid_t dxpl_id, void *_buf/*out*/)
{
    uint8_t *buf=(uint8_t *)_buf;   /* Get local copies for address arithmetic */
    const uint8_t *tscat_buf=(const uint8_t *)_tscat_buf;
    hsize_t *off=NULL;          /* Array to store sequence offsets */
    ssize_t vector_size;        /* Value for vector size */
    size_t *len=NULL;           /* Array to store sequence lengths */
    size_t curr_len;            /* Length of bytes left to process in sequence */
    size_t maxbytes;            /* Number of bytes in the buffer */
    size_t nseq;                /* Number of sequences generated */
    size_t curr_seq;            /* Current sequence being processed */
    size_t nbytes;              /* Number of bytes used in sequences */
    herr_t ret_value=SUCCEED;   /* Number of elements scattered */

    FUNC_ENTER_NOAPI(H5S_select_mscat, FAIL);

    /* Check args */
    assert (tscat_buf);
    assert (elmt_size>0);
    assert (space);
    assert (iter);
    assert (nelmts>0);
    assert (buf);

    /* Get the hyperslab vector size */
    if((vector_size=H5S_get_vector_size(dxpl_id))<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "unable to get I/O vector size");

    /* Allocate the vector I/O arrays */
    if((len = H5FL_ARR_MALLOC(size_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O length vector array");
    if((off = H5FL_ARR_MALLOC(hsize_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O offset vector array");

    /* Compute the number of bytes available in buffer */
    H5_ASSIGN_OVERFLOW(maxbytes,nelmts*elmt_size,hsize_t,size_t);

    /* Loop until all elements are written */
    while(maxbytes>0) {
        /* Get list of sequences for selection to write */
        if((*space->select.get_seq_list)(space,0,iter,elmt_size,(size_t)vector_size,maxbytes,&nseq,&nbytes,off,len)<0)
            HGOTO_ERROR (H5E_INTERNAL, H5E_UNSUPPORTED, 0, "sequence length generation failed");

        /* Loop, while sequences left to process */
        for(curr_seq=0; curr_seq<nseq; curr_seq++) {
            /* Get the number of bytes in sequence */
            curr_len=len[curr_seq];

            HDmemcpy(buf+off[curr_seq],tscat_buf,curr_len);

            /* Advance offset in destination buffer */
            tscat_buf+=curr_len;
        } /* end for */

        /* Decrement number of elements left to process */
        assert(nbytes%elmt_size==0);
        maxbytes -= nbytes;
    } /* end while */

done:
    if(len!=NULL)
        H5FL_ARR_FREE(size_t,len);
    if(off!=NULL)
        H5FL_ARR_FREE(hsize_t,off);
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_mscat() */


/*-------------------------------------------------------------------------
 * Function:	H5S_select_mgath
 *
 * Purpose:	Gathers dataset elements from application memory BUF and
 *		copies them into the gather buffer TGATH_BUF.
 *		Each element is ELMT_SIZE bytes and arranged in application
 *		memory according to SPACE.  
 *		The caller is requesting that at most NELMTS be gathered.
 *
 * Return:	Success:	Number of elements copied.
 *		Failure:	0
 *
 * Programmer:	Quincey Koziol
 *              Monday, June 24, 2002
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
hsize_t
H5S_select_mgath (const void *_buf, size_t elmt_size, const H5S_t *space,
    H5S_sel_iter_t *iter, hsize_t nelmts, hid_t dxpl_id, void *_tgath_buf/*out*/)
{
    const uint8_t *buf=(const uint8_t *)_buf;   /* Get local copies for address arithmetic */
    uint8_t *tgath_buf=(uint8_t *)_tgath_buf;
    hsize_t *off=NULL;          /* Array to store sequence offsets */
    ssize_t vector_size;        /* Value for vector size */
    size_t *len=NULL;           /* Array to store sequence lengths */
    size_t curr_len;            /* Length of bytes left to process in sequence */
    size_t maxbytes;            /* Number of bytes in the buffer */
    size_t nseq;                /* Number of sequences generated */
    size_t curr_seq;            /* Current sequence being processed */
    size_t nbytes;              /* Number of bytes used in sequences */
    hsize_t ret_value=nelmts;   /* Number of elements gathered */

    FUNC_ENTER_NOAPI(H5S_select_mgath, 0);

    /* Check args */
    assert (buf);
    assert (elmt_size>0);
    assert (space);
    assert (iter);
    assert (nelmts>0);
    assert (tgath_buf);

    /* Get the hyperslab vector size */
    if((vector_size=H5S_get_vector_size(dxpl_id))<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, 0, "unable to get I/O vector size");

    /* Allocate the vector I/O arrays */
    if((len = H5FL_ARR_MALLOC(size_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, 0, "can't allocate I/O length vector array");
    if((off = H5FL_ARR_MALLOC(hsize_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, 0, "can't allocate I/O offset vector array");

    /* Compute the number of bytes available in buffer */
    H5_ASSIGN_OVERFLOW(maxbytes,nelmts*elmt_size,hsize_t,size_t);

    /* Loop until all elements are written */
    while(maxbytes>0) {
        /* Get list of sequences for selection to write */
        if((*space->select.get_seq_list)(space,0,iter,elmt_size,(size_t)vector_size,maxbytes,&nseq,&nbytes,off,len)<0)
            HGOTO_ERROR (H5E_INTERNAL, H5E_UNSUPPORTED, 0, "sequence length generation failed");

        /* Loop, while sequences left to process */
        for(curr_seq=0; curr_seq<nseq; curr_seq++) {
            /* Get the number of bytes in sequence */
            curr_len=len[curr_seq];

            HDmemcpy(tgath_buf,buf+off[curr_seq],curr_len);

            /* Advance offset in gather buffer */
            tgath_buf+=curr_len;
        } /* end for */

        /* Decrement number of elements left to process */
        assert(nbytes%elmt_size==0);
        maxbytes -= nbytes;
    } /* end while */

done:
    if(len!=NULL)
        H5FL_ARR_FREE(size_t,len);
    if(off!=NULL)
        H5FL_ARR_FREE(hsize_t,off);
    FUNC_LEAVE_NOAPI(ret_value);
}   /* H5S_select_mgath() */


/*-------------------------------------------------------------------------
 * Function:	H5S_select_read
 *
 * Purpose:	Reads directly from file into application memory.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *              Tuesday, July 23, 2002
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5S_select_read(H5F_t *f, const H5O_layout_t *layout, H5P_genplist_t *dc_plist,
    const H5D_storage_t *store, size_t elmt_size, const H5S_t *file_space,
    const H5S_t *mem_space, hid_t dxpl_id, void *buf/*out*/)
{
    H5S_sel_iter_t mem_iter;    /* Memory selection iteration info */
    hbool_t mem_iter_init=0;    /* Memory selection iteration info has been initialized */
    H5S_sel_iter_t file_iter;   /* File selection iteration info */
    hbool_t file_iter_init=0;	/* File selection iteration info has been initialized */
    hsize_t *mem_off=NULL;      /* Array to store sequence offsets in memory */
    hsize_t *file_off=NULL;     /* Array to store sequence offsets in the file */
    ssize_t vector_size;        /* Value for vector size */
    size_t *mem_len=NULL;       /* Array to store sequence lengths in memory */
    size_t *file_len=NULL;      /* Array to store sequence lengths in the file */
    size_t maxbytes;            /* Number of bytes in selection */
    size_t mem_nseq;            /* Number of sequences generated in the file */
    size_t file_nseq;           /* Number of sequences generated in memory */
    size_t mem_nbytes;          /* Number of bytes used in memory sequences */
    size_t file_nbytes;         /* Number of bytes used in file sequences */
    size_t curr_mem_seq;        /* Current memory sequence to operate on */
    size_t curr_file_seq;       /* Current file sequence to operate on */
    ssize_t tmp_file_len;       /* Temporary number of bytes in file sequence */
    herr_t ret_value=SUCCEED;   /* Return value */

    FUNC_ENTER_NOAPI(H5S_select_read, FAIL);

    /* Check args */
    assert(f);
    assert(store);
    assert(buf);
    assert(TRUE==H5P_isa_class(dxpl_id,H5P_DATASET_XFER));
    
    /* Initialize file iterator */
    if (H5S_select_iter_init(&file_iter, file_space, elmt_size)<0)
        HGOTO_ERROR (H5E_DATASPACE, H5E_CANTINIT, FAIL, "unable to initialize selection iterator");
    file_iter_init=1;	/* File selection iteration info has been initialized */

    /* Initialize memory iterator */
    if (H5S_select_iter_init(&mem_iter, mem_space, elmt_size)<0)
        HGOTO_ERROR (H5E_DATASPACE, H5E_CANTINIT, FAIL, "unable to initialize selection iterator");
    mem_iter_init=1;	/* Memory selection iteration info has been initialized */

    /* Get the hyperslab vector size */
    if((vector_size=H5S_get_vector_size(dxpl_id))<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "unable to get I/O vector size");

    /* Allocate the vector I/O arrays */
    if((mem_len = H5FL_ARR_MALLOC(size_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O length vector array");
    if((mem_off = H5FL_ARR_MALLOC(hsize_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O offset vector array");
    if((file_len = H5FL_ARR_MALLOC(size_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O length vector array");
    if((file_off = H5FL_ARR_MALLOC(hsize_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O offset vector array");

    /* Get number of bytes in selection */
#ifndef NDEBUG
    {
        hsize_t tmp_maxbytes=(*file_space->select.get_npoints)(file_space)*elmt_size;
        H5_ASSIGN_OVERFLOW(maxbytes,tmp_maxbytes,hsize_t,size_t);
    }
#else /* NDEBUG */
    maxbytes=(size_t)((*file_space->select.get_npoints)(file_space)*elmt_size);
#endif /* NDEBUG */

    /* Initialize sequence counts */
    curr_mem_seq=curr_file_seq=0;
    mem_nseq=file_nseq=0;

    /* Loop, until all bytes are processed */
    while(maxbytes>0) {
        /* Check if more file sequences are needed */
        if(curr_file_seq>=file_nseq) {
            /* Get sequences for file selection */
            if((*file_space->select.get_seq_list)(file_space,H5S_GET_SEQ_LIST_SORTED,&file_iter,elmt_size,(size_t)vector_size,maxbytes,&file_nseq,&file_nbytes,file_off,file_len)<0)
                HGOTO_ERROR (H5E_INTERNAL, H5E_UNSUPPORTED, FAIL, "sequence length generation failed");

            /* Start at the beginning of the sequences again */
            curr_file_seq=0;
        } /* end if */

        /* Check if more memory sequences are needed */
        if(curr_mem_seq>=mem_nseq) {
            /* Get sequences for memory selection */
            if((*mem_space->select.get_seq_list)(mem_space,0,&mem_iter,elmt_size,(size_t)vector_size,maxbytes,&mem_nseq,&mem_nbytes,mem_off,mem_len)<0)
                HGOTO_ERROR (H5E_INTERNAL, H5E_UNSUPPORTED, FAIL, "sequence length generation failed");

            /* Start at the beginning of the sequences again */
            curr_mem_seq=0;
        } /* end if */

        /* Read file sequences into current memory sequence */
        if ((tmp_file_len=H5F_seq_readvv(f, dxpl_id, layout, dc_plist, store,
                file_nseq, &curr_file_seq, file_len, file_off,
                mem_nseq, &curr_mem_seq, mem_len, mem_off,
                buf))<0)
            HGOTO_ERROR(H5E_DATASPACE, H5E_READERROR, FAIL, "read error");

        /* Decrement number of bytes left to process */
        maxbytes-=tmp_file_len;
    } /* end while */

done:
    /* Release file selection iterator */
    if(file_iter_init) {
        if (H5S_select_iter_release(&file_iter)<0)
            HDONE_ERROR (H5E_DATASPACE, H5E_CANTRELEASE, FAIL, "unable to release selection iterator");
    } /* end if */

    /* Release memory selection iterator */
    if(mem_iter_init) {
        if (H5S_select_iter_release(&mem_iter)<0)
            HDONE_ERROR (H5E_DATASPACE, H5E_CANTRELEASE, FAIL, "unable to release selection iterator");
    } /* end if */

    /* Free vector arrays */
    if(file_len!=NULL)
        H5FL_ARR_FREE(size_t,file_len);
    if(file_off!=NULL)
        H5FL_ARR_FREE(hsize_t,file_off);
    if(mem_len!=NULL)
        H5FL_ARR_FREE(size_t,mem_len);
    if(mem_off!=NULL)
        H5FL_ARR_FREE(hsize_t,mem_off);
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5S_select_read() */


/*-------------------------------------------------------------------------
 * Function:	H5S_select_write
 *
 * Purpose:	Writes directly from application memory into a file
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *              Tuesday, July 23, 2002
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5S_select_write(H5F_t *f, H5O_layout_t *layout, H5P_genplist_t *dc_plist,
    const H5D_storage_t *store, size_t elmt_size, const H5S_t *file_space,
    const H5S_t *mem_space, hid_t dxpl_id, const void *buf/*out*/)
{
    H5S_sel_iter_t mem_iter;    /* Memory selection iteration info */
    hbool_t mem_iter_init=0;    /* Memory selection iteration info has been initialized */
    H5S_sel_iter_t file_iter;   /* File selection iteration info */
    hbool_t file_iter_init=0;	/* File selection iteration info has been initialized */
    hsize_t *mem_off=NULL;      /* Array to store sequence offsets in memory */
    hsize_t *file_off=NULL;     /* Array to store sequence offsets in the file */
    ssize_t vector_size;        /* Value for vector size */
    size_t *mem_len=NULL;       /* Array to store sequence lengths in memory */
    size_t *file_len=NULL;      /* Array to store sequence lengths in the file */
    size_t maxbytes;            /* Number of bytes in selection */
    size_t mem_nseq;            /* Number of sequences generated in the file */
    size_t file_nseq;           /* Number of sequences generated in memory */
    size_t mem_nbytes;          /* Number of bytes used in memory sequences */
    size_t file_nbytes;         /* Number of bytes used in file sequences */
    size_t curr_mem_seq;        /* Current memory sequence to operate on */
    size_t curr_file_seq;       /* Current file sequence to operate on */
    ssize_t tmp_file_len;       /* Temporary number of bytes in file sequence */
    herr_t ret_value=SUCCEED;   /* Return value */

    FUNC_ENTER_NOAPI(H5S_select_write, FAIL);

    /* Check args */
    assert(f);
    assert(store);
    assert(buf);
    assert(TRUE==H5P_isa_class(dxpl_id,H5P_DATASET_XFER));
    
    /* Get the hyperslab vector size */
    if((vector_size=H5S_get_vector_size(dxpl_id))<0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "unable to get I/O vector size");

    /* Allocate the vector I/O arrays */
    if((mem_len = H5FL_ARR_MALLOC(size_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O length vector array");
    if((mem_off = H5FL_ARR_MALLOC(hsize_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O offset vector array");
    if((file_len = H5FL_ARR_MALLOC(size_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O length vector array");
    if((file_off = H5FL_ARR_MALLOC(hsize_t,(size_t)vector_size))==NULL)
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "can't allocate I/O offset vector array");

    /* Initialize file iterator */
    if (H5S_select_iter_init(&file_iter, file_space, elmt_size)<0)
        HGOTO_ERROR (H5E_DATASPACE, H5E_CANTINIT, FAIL, "unable to initialize selection iterator");
    file_iter_init=1;	/* File selection iteration info has been initialized */

    /* Initialize memory iterator */
    if (H5S_select_iter_init(&mem_iter, mem_space, elmt_size)<0)
        HGOTO_ERROR (H5E_DATASPACE, H5E_CANTINIT, FAIL, "unable to initialize selection iterator");
    mem_iter_init=1;	/* Memory selection iteration info has been initialized */
    
    /* Get number of bytes in selection */
#ifndef NDEBUG
    {
        hsize_t tmp_maxbytes=(*file_space->select.get_npoints)(file_space)*elmt_size;
        H5_ASSIGN_OVERFLOW(maxbytes,tmp_maxbytes,hsize_t,size_t);
    }
#else /* NDEBUG */
    maxbytes=(size_t)((*file_space->select.get_npoints)(file_space)*elmt_size);
#endif /* NDEBUG */

    /* Initialize sequence counts */
    curr_mem_seq=curr_file_seq=0;
    mem_nseq=file_nseq=0;

    /* Loop, until all bytes are processed */
    while(maxbytes>0) {
        /* Check if more file sequences are needed */
        if(curr_file_seq>=file_nseq) {
            /* Get sequences for file selection */
            if((*file_space->select.get_seq_list)(file_space,H5S_GET_SEQ_LIST_SORTED,&file_iter,elmt_size,(size_t)vector_size,maxbytes,&file_nseq,&file_nbytes,file_off,file_len)<0)
                HGOTO_ERROR (H5E_INTERNAL, H5E_UNSUPPORTED, FAIL, "sequence length generation failed");

            /* Start at the beginning of the sequences again */
            curr_file_seq=0;
        } /* end if */

        /* Check if more memory sequences are needed */
        if(curr_mem_seq>=mem_nseq) {
            /* Get sequences for memory selection */
            if((*mem_space->select.get_seq_list)(mem_space,0,&mem_iter,elmt_size,(size_t)vector_size,maxbytes,&mem_nseq,&mem_nbytes,mem_off,mem_len)<0)
                HGOTO_ERROR (H5E_INTERNAL, H5E_UNSUPPORTED, FAIL, "sequence length generation failed");

            /* Start at the beginning of the sequences again */
            curr_mem_seq=0;
        } /* end if */

        /* Write memory sequences into file sequences */
        if ((tmp_file_len=H5F_seq_writevv(f, dxpl_id, layout, dc_plist, store,
                file_nseq, &curr_file_seq, file_len, file_off,
                mem_nseq, &curr_mem_seq, mem_len, mem_off,
                buf))<0)
            HGOTO_ERROR(H5E_DATASPACE, H5E_WRITEERROR, FAIL, "write error");

        /* Decrement number of bytes left to process */
        maxbytes-=tmp_file_len;
    } /* end while */

done:
    /* Release file selection iterator */
    if(file_iter_init) {
        if (H5S_select_iter_release(&file_iter)<0)
            HDONE_ERROR (H5E_DATASPACE, H5E_CANTRELEASE, FAIL, "unable to release selection iterator");
    } /* end if */

    /* Release memory selection iterator */
    if(mem_iter_init) {
        if (H5S_select_iter_release(&mem_iter)<0)
            HDONE_ERROR (H5E_DATASPACE, H5E_CANTRELEASE, FAIL, "unable to release selection iterator");
    } /* end if */

    /* Free vector arrays */
    if(file_len!=NULL)
        H5FL_ARR_FREE(size_t,file_len);
    if(file_off!=NULL)
        H5FL_ARR_FREE(hsize_t,file_off);
    if(mem_len!=NULL)
        H5FL_ARR_FREE(size_t,mem_len);
    if(mem_off!=NULL)
        H5FL_ARR_FREE(hsize_t,mem_off);
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5S_select_write() */

