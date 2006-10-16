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
 * Created:		H5Ocache.c
 *			Sep 28 2005
 *			Quincey Koziol <koziol@ncsa.uiuc.edu>
 *
 * Purpose:		Object header metadata cache virtual functions.
 *
 *-------------------------------------------------------------------------
 */

/****************/
/* Module Setup */
/****************/

#define H5O_PACKAGE		/*suppress error about including H5Opkg	  */

/***********/
/* Headers */
/***********/
#include "H5private.h"		/* Generic Functions			*/
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5FLprivate.h"	/* Free lists                           */
#include "H5Opkg.h"             /* Object headers			*/

/****************/
/* Local Macros */
/****************/

/* Set the object header size to speculatively read in */
/* (needs to be more than the default dataset header size) */
#define H5O_SPEC_READ_SIZE 512


/******************/
/* Local Typedefs */
/******************/


/********************/
/* Package Typedefs */
/********************/


/********************/
/* Local Prototypes */
/********************/

/* Metadata cache callbacks */
static H5O_t *H5O_load(H5F_t *f, hid_t dxpl_id, haddr_t addr, const void *_udata1,
		       void *_udata2);
static herr_t H5O_flush(H5F_t *f, hid_t dxpl_id, hbool_t destroy, haddr_t addr, H5O_t *oh);
static herr_t H5O_clear(H5F_t *f, H5O_t *oh, hbool_t destroy);
static herr_t H5O_size(const H5F_t *f, const H5O_t *oh, size_t *size_ptr);


/*********************/
/* Package Variables */
/*********************/


/*****************************/
/* Library Private Variables */
/*****************************/


/*******************/
/* Local Variables */
/*******************/

/* H5O inherits cache-like properties from H5AC */
const H5AC_class_t H5AC_OHDR[1] = {{
    H5AC_OHDR_ID,
    (H5AC_load_func_t)H5O_load,
    (H5AC_flush_func_t)H5O_flush,
    (H5AC_dest_func_t)H5O_dest,
    (H5AC_clear_func_t)H5O_clear,
    (H5AC_size_func_t)H5O_size,
}};


/*-------------------------------------------------------------------------
 * Function:	H5O_flush_msgs
 *
 * Purpose:	Flushes messages for object header.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@ncsa.uiuc.edu
 *		Nov 21 2005
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5O_flush_msgs(H5F_t *f, H5O_t *oh)
{
    H5O_mesg_t *curr_msg;       /* Pointer to current message being operated on */
    unsigned	u;              /* Local index variable */
    herr_t      ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_NOAPI(H5O_flush_msgs, FAIL)

    /* check args */
    HDassert(f);
    HDassert(oh);

    /* Encode any dirty messages */
    for(u = 0, curr_msg = &oh->mesg[0]; u < oh->nmesgs; u++, curr_msg++) {
        if(curr_msg->dirty) {
            uint8_t	*p;             /* Temporary pointer to encode with */

            /* Point into message's chunk's image */
            p = curr_msg->raw - H5O_SIZEOF_MSGHDR_OH(oh);

            /* Encode the message prefix */
            UINT16ENCODE(p, curr_msg->type->id);
            HDassert(curr_msg->raw_size < H5O_MESG_MAX_SIZE);
            UINT16ENCODE(p, curr_msg->raw_size);
            *p++ = curr_msg->flags;

            /* Only encode reserved bytes for version 1 of format */
            if(oh->version == H5O_VERSION_1) {
                *p++ = 0; /*reserved*/
                *p++ = 0; /*reserved*/
                *p++ = 0; /*reserved*/
            } /* end for */

            /* Encode the message itself */
            if(curr_msg->native) {
                herr_t	(*encode)(H5F_t*, uint8_t*, const void*);

                HDassert(curr_msg->type->encode);

                /*
                 * Encode the message.  If the message is shared then we
                 * encode a Shared Object message instead of the object
                 * which is being shared.
                 */
                HDassert(curr_msg->raw >= oh->chunk[curr_msg->chunkno].image);
                HDassert(curr_msg->raw_size == H5O_ALIGN_OH(oh, curr_msg->raw_size));
                HDassert(curr_msg->raw + curr_msg->raw_size <=
                       oh->chunk[curr_msg->chunkno].image + oh->chunk[curr_msg->chunkno].size);
                if(curr_msg->flags & H5O_FLAG_SHARED)
                    encode = H5O_MSG_SHARED->encode;
                else
                    encode = curr_msg->type->encode;
                if((encode)(f, curr_msg->raw, curr_msg->native) < 0)
                    HGOTO_ERROR(H5E_OHDR, H5E_CANTENCODE, FAIL, "unable to encode object header message")
            } /* end if */
            curr_msg->dirty = FALSE;
            oh->chunk[curr_msg->chunkno].dirty = TRUE;
        } /* end if */
    } /* end for */

    /* Sanity check for the correct # of messages in object header */
    if(oh->nmesgs != u)
        HGOTO_ERROR(H5E_OHDR, H5E_CANTFLUSH, FAIL, "corrupt object header - too few messages")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5O_flush_msgs() */


/*-------------------------------------------------------------------------
 * Function:	H5O_load
 *
 * Purpose:	Loads an object header from disk.
 *
 * Return:	Success:	Pointer to the new object header.
 *
 *		Failure:	NULL
 *
 * Programmer:	Robb Matzke
 *		matzke@llnl.gov
 *		Aug  5 1997
 *
 *-------------------------------------------------------------------------
 */
static H5O_t *
H5O_load(H5F_t *f, hid_t dxpl_id, haddr_t addr, const void UNUSED * _udata1,
	 void UNUSED * _udata2)
{
    H5O_t	*oh = NULL;     /* Object header read in */
    uint8_t     read_buf[H5O_SPEC_READ_SIZE];       /* Buffer for speculative read */
    uint8_t	*p;             /* Pointer into buffer to decode */
    size_t	spec_read_size; /* Size of buffer to speculatively read in */
    size_t	prefix_size;    /* Size of object header prefix */
    unsigned	nmesgs;         /* Total # of messages in this object header */
    unsigned	curmesg = 0;    /* Current message being decoded in object header */
    unsigned    skipped_msgs = 0;       /* Number of unknown messages skipped */
    unsigned    merged_null_msgs = 0;   /* Number of null messages merged together */
    haddr_t	chunk_addr;     /* Address of first chunk */
    size_t	chunk_size;     /* Size of first chunk */
    haddr_t     abs_eoa;	/* Absolute end of file address	*/
    haddr_t     rel_eoa;	/* Relative end of file address	*/
    uint32_t    prefix_chksum = 0;     /* Checksum of object header prefix */
    H5O_t	*ret_value;     /* Return value */

    FUNC_ENTER_NOAPI(H5O_load, NULL)

    /* check args */
    HDassert(f);
    HDassert(H5F_addr_defined(addr));
    HDassert(!_udata1);
    HDassert(!_udata2);

    /* Make certain we don't speculatively read off the end of the file */
    if(HADDR_UNDEF == (abs_eoa = H5F_get_eoa(f)))
        HGOTO_ERROR(H5E_OHDR, H5E_CANTGET, NULL, "unable to determine file size")

    /* Adjust absolute EOA address to relative EOA address */
    rel_eoa = abs_eoa - H5F_get_base_addr(f);

    /* Compute the size of the speculative object header buffer */
    H5_ASSIGN_OVERFLOW(spec_read_size, MIN(rel_eoa - addr, H5O_SPEC_READ_SIZE), /* From: */ hsize_t, /* To: */ size_t);

    /* Attempt to speculatively read both object header prefix and first chunk */
    if(H5F_block_read(f, H5FD_MEM_OHDR, addr, spec_read_size, dxpl_id, read_buf) < 0)
	HGOTO_ERROR(H5E_OHDR, H5E_READERROR, NULL, "unable to read object header")
    p = read_buf;

    /* allocate ohdr and init chunk list */
    if(NULL == (oh = H5FL_CALLOC(H5O_t)))
	HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed")

    /* Check for magic number */
    /* (indicates version 2 or later) */
    if(!HDmemcmp(p, H5O_HDR_MAGIC, (size_t)H5O_SIZEOF_MAGIC)) {
        /* Magic number */
        p += H5O_SIZEOF_MAGIC;

        /* Version */
        oh->version = *p++;
        if(H5O_VERSION_2 != oh->version)
            HGOTO_ERROR(H5E_OHDR, H5E_VERSION, NULL, "bad object header version number")
    } /* end if */
    else {
        /* Version */
        oh->version = *p++;
        if(H5O_VERSION_1 != oh->version)
            HGOTO_ERROR(H5E_OHDR, H5E_VERSION, NULL, "bad object header version number")

        /* Reserved */
        p++;
    } /* end else */

    /* Number of messages */
    UINT16DECODE(p, nmesgs);

    /* Link count */
    UINT32DECODE(p, oh->nlink);

    /* First chunk size */
    UINT32DECODE(p, chunk_size);

    /* Reserved, in version 1 */
    if(H5O_VERSION_1 == oh->version)
        p += 4;

    /* Determine object header prefix length */
    prefix_size = (size_t)(p - read_buf);

    /* Compute partial checksum, for later versions of the format */
    if(oh->version > H5O_VERSION_1)
        prefix_chksum = H5_checksum_lookup3(read_buf, prefix_size, 0);

    /* Compute first chunk address */
    chunk_addr = addr + (hsize_t)prefix_size;

    /* Allocate the message array */
    oh->alloc_nmesgs = nmesgs;
    if(NULL == (oh->mesg = H5FL_SEQ_MALLOC(H5O_mesg_t, oh->alloc_nmesgs)))
	HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed")

    /* Read each chunk from disk */
    while(H5F_addr_defined(chunk_addr)) {
        unsigned chunkno;       /* Current chunk's index */

	/* Increase chunk array size, if necessary */
	if(oh->nchunks >= oh->alloc_nchunks) {
	    unsigned na = MAX(H5O_NCHUNKS, oh->alloc_nchunks * 2);        /* Double # of chunks allocated */
	    H5O_chunk_t *x = H5FL_SEQ_REALLOC(H5O_chunk_t, oh->chunk, (size_t)na);

	    if(!x)
                HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed")
	    oh->alloc_nchunks = na;
	    oh->chunk = x;
	} /* end if */

	/* Init the chunk data info */
	chunkno = oh->nchunks++;
	oh->chunk[chunkno].dirty = FALSE;
        if(chunkno == 0) {
            /* First chunk's 'image' includes room for the object header prefix */
            oh->chunk[0].addr = addr;
            oh->chunk[0].size = chunk_size + H5O_SIZEOF_HDR_OH(oh);
        } /* end if */
        else {
            oh->chunk[chunkno].addr = chunk_addr;
            oh->chunk[chunkno].size = chunk_size;
        } /* end else */
	if(NULL == (oh->chunk[chunkno].image = H5FL_BLK_MALLOC(chunk_image, oh->chunk[chunkno].size)))
	    HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed")

        /* Handle chunk 0 as special case */
        if(chunkno == 0) {
            /* Check for speculative read of first chunk containing all the data needed */
            if(spec_read_size >= oh->chunk[0].size)
                HDmemcpy(oh->chunk[0].image, read_buf, oh->chunk[0].size);
            else {
                /* Copy the object header prefix into chunk 0's image */
                HDmemcpy(oh->chunk[0].image, read_buf, prefix_size);

                /* Read the chunk raw data */
                if(H5F_block_read(f, H5FD_MEM_OHDR, chunk_addr, (oh->chunk[0].size - prefix_size),
                        dxpl_id, (oh->chunk[0].image + prefix_size)) < 0)
                    HGOTO_ERROR(H5E_OHDR, H5E_READERROR, NULL, "unable to read object header data")
            } /* end else */

            /* Point into chunk image to decode */
            p = oh->chunk[0].image + prefix_size;
        } /* end if */
        else {
            /* Read the chunk raw data */
            if(H5F_block_read(f, H5FD_MEM_OHDR, chunk_addr, chunk_size,
                    dxpl_id, oh->chunk[chunkno].image) < 0)
                HGOTO_ERROR(H5E_OHDR, H5E_READERROR, NULL, "unable to read object header data")

            /* Point into chunk image to decode */
            p = oh->chunk[chunkno].image;
        } /* end else */

        /* Check for magic # on chunks > 0 in later versions of the format */
        if(chunkno > 0 && oh->version > H5O_VERSION_1) {
            /* Magic number */
            if(!HDmemcmp(p, H5O_CHK_MAGIC, (size_t)H5O_SIZEOF_MAGIC))
                HGOTO_ERROR(H5E_OHDR, H5E_CANTLOAD, NULL, "wrong object header chunk signature")
            p += H5O_SIZEOF_MAGIC;
        } /* end if */

	/* Decode messages from this chunk */
	while(p < (oh->chunk[chunkno].image + (oh->chunk[chunkno].size - H5O_SIZEOF_CHKSUM_OH(oh)))) {
            unsigned    mesgno;         /* Current message to operate on */
            size_t	mesg_size;      /* Size of message read in */
            unsigned	id;             /* ID (type) of current message */
            uint8_t	flags;          /* Flags for current message */

            /* Decode message prefix info */
	    UINT16DECODE(p, id);
	    UINT16DECODE(p, mesg_size);
	    HDassert(mesg_size == H5O_ALIGN_OH(oh, mesg_size));
	    flags = *p++;
            if(oh->version == H5O_VERSION_1)
                p += 3; /*reserved*/

            /* Try to detect invalidly formatted object header messages */
	    if(p + mesg_size > oh->chunk[chunkno].image + (oh->chunk[chunkno].size - H5O_SIZEOF_CHKSUM_OH(oh)))
		HGOTO_ERROR(H5E_OHDR, H5E_CANTINIT, NULL, "corrupt object header")

            /* Skip header messages we don't know about */
            /* (Usually from future versions of the library) */
	    if(id >= NELMTS(H5O_msg_class_g) || NULL == H5O_msg_class_g[id]) {
                /* Increment skipped messages counter */
                skipped_msgs++;

                /* Advance decode pointer past message */
                p += mesg_size;

                /* Go get next message */
                continue;
            } /* end if */

            /* Check for combining two adjacent 'null' messages */
            if((H5F_get_intent(f) & H5F_ACC_RDWR) &&
	            H5O_NULL_ID == id && oh->nmesgs > 0 &&
                    H5O_NULL_ID == oh->mesg[oh->nmesgs - 1].type->id &&
                    oh->mesg[oh->nmesgs - 1].chunkno == chunkno) {

		/* Combine adjacent null messages */
		mesgno = oh->nmesgs - 1;
		oh->mesg[mesgno].raw_size += H5O_SIZEOF_MSGHDR_OH(oh) + mesg_size;
		oh->mesg[mesgno].dirty = TRUE;
                merged_null_msgs++;
	    } /* end if */
            else {
		/* New message */
		if(oh->nmesgs >= nmesgs)
		    HGOTO_ERROR(H5E_OHDR, H5E_CANTLOAD, NULL, "corrupt object header - too many messages")
		mesgno = oh->nmesgs++;
		oh->mesg[mesgno].type = H5O_msg_class_g[id];
		oh->mesg[mesgno].dirty = FALSE;
		oh->mesg[mesgno].flags = flags;
		oh->mesg[mesgno].native = NULL;
		oh->mesg[mesgno].raw = p;
		oh->mesg[mesgno].raw_size = mesg_size;
		oh->mesg[mesgno].chunkno = chunkno;
	    } /* end else */

            /* Advance decode pointer past message */
            p += mesg_size;
	} /* end while */

        /* Check for correct checksum on chunks, in later versions of the format */
        if(oh->version > H5O_VERSION_1) {
            uint32_t stored_chksum;     /* Checksum from file */
            uint32_t computed_chksum;   /* Checksum computed in memory */

            /* Metadata checksum */
            UINT32DECODE(p, stored_chksum);

            /* Compute checksum on entire header */
            if(chunkno == 0)
                computed_chksum = H5_checksum_metadata(oh->chunk[chunkno].image + prefix_size, chunk_size, prefix_chksum);
            else
                computed_chksum = H5_checksum_metadata(oh->chunk[chunkno].image, (chunk_size - H5O_SIZEOF_CHKSUM), 0);

            /* Verify checksum */
            if(stored_chksum != computed_chksum)
                HGOTO_ERROR(H5E_OHDR, H5E_BADVALUE, NULL, "incorrect metadata checksum for object header chunk")
        } /* end if */

        /* Sanity check */
        HDassert(p == oh->chunk[chunkno].image + oh->chunk[chunkno].size);

        /* Check for another chunk to read in & parse */
        for(chunk_addr = HADDR_UNDEF; !H5F_addr_defined(chunk_addr) && curmesg < oh->nmesgs; ++curmesg) {
            /* Check if next message to examine is a continuation message */
            if(H5O_CONT_ID == oh->mesg[curmesg].type->id) {
                H5O_cont_t *cont;

                /* Decode continuation message */
                cont = (H5O_MSG_CONT->decode)(f, dxpl_id, oh->mesg[curmesg].raw);
                cont->chunkno = oh->nchunks;	/*the next chunk to allocate */

                /* Save 'native' form of continuation message */
                oh->mesg[curmesg].native = cont;

                /* Set up to read in next chunk */
                chunk_addr = cont->addr;
                chunk_size = cont->size;
            } /* end if */
        } /* end for */
    } /* end while */

    /* Mark the object header dirty if we've merged a message */
    if(merged_null_msgs)
	oh->cache_info.is_dirty = TRUE;

    /* Sanity check for the correct # of messages in object header */
    if((oh->nmesgs + skipped_msgs + merged_null_msgs) != nmesgs)
        HGOTO_ERROR(H5E_OHDR, H5E_CANTLOAD, NULL, "corrupt object header - too few messages")

    /* Set return value */
    ret_value = oh;

done:
    /* Release the [possibly partially initialized] object header on errors */
    if(!ret_value && oh) {
        if(H5O_dest(f,oh) < 0)
	    HDONE_ERROR(H5E_OHDR, H5E_CANTFREE, NULL, "unable to destroy object header data")
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5O_load() */


/*-------------------------------------------------------------------------
 * Function:	H5O_flush
 *
 * Purpose:	Flushes (and destroys) an object header.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Robb Matzke
 *		matzke@llnl.gov
 *		Aug  5 1997
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5O_flush(H5F_t *f, hid_t dxpl_id, hbool_t destroy, haddr_t UNUSED addr, H5O_t *oh)
{
    herr_t      ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_NOAPI(H5O_flush, FAIL)

    /* check args */
    HDassert(f);
    HDassert(H5F_addr_defined(addr));
    HDassert(oh);

    /* flush */
    if(oh->cache_info.is_dirty) {
        uint8_t	*p;             /* Pointer to object header prefix buffer */
        unsigned u;             /* Local index variable */

	/* Encode any dirty messages */
        if(H5O_flush_msgs(f, oh) < 0)
            HGOTO_ERROR(H5E_OHDR, H5E_CANTFLUSH, FAIL, "unable to flush object header messages")

        /* Point to raw data 'image' for first chunk, which has room for the prefix */
        p = oh->chunk[0].image;

        /* Later versions of object header prefix have different format and
         * also require that chunk 0 always be updated, since the checksum
         * on the entire block of memory needs to be updated if anything is
         * modified */
        if(oh->version > H5O_VERSION_1) {
            size_t prefix_size;         /* Length of object header prefix */
            uint32_t prefix_chksum;     /* Prefix checksum value */
            uint32_t full_chksum;       /* Full checksum value */
            size_t raw_size;            /* Size of raw data in first chunk */

            /* Verify magic number */
            HDassert(!HDmemcmp(oh->chunk[u].image, H5O_HDR_MAGIC, H5O_SIZEOF_MAGIC));
            p += H5O_SIZEOF_MAGIC;

            /* Version */
            *p++ = oh->version;

            /* Number of messages */
            UINT16ENCODE(p, oh->nmesgs);

            /* Link count */
            UINT32ENCODE(p, oh->nlink);

            /* Chunk size */
            UINT32ENCODE(p, (oh->chunk[0].size - H5O_SIZEOF_HDR_OH(oh)));

            /* Determine object header prefix length */
            prefix_size = (size_t)(p - oh->chunk[0].image);

            /* Compute partial checksum for later */
            /* (checksum performed in this odd way in order to accomodate
             *  reading in the header & first chunk in reasonable way)
             */
            prefix_chksum = H5_checksum_lookup3(oh->chunk[0].image, prefix_size, 0);

            /* Finish full checksum, over chunk data */
            raw_size = oh->chunk[0].size - H5O_SIZEOF_HDR_OH(oh);
            full_chksum = H5_checksum_metadata(p, raw_size, prefix_chksum);
            p += raw_size;

            /* Metadata checksum */
            UINT32ENCODE(p, full_chksum);
            HDassert((size_t)(p - oh->chunk[0].image) == oh->chunk[0].size);
        } /* end if */
        else {
            /* Version */
            *p++ = oh->version;

            /* Reserved */
            *p++ = 0;

            /* Number of messages */
            UINT16ENCODE(p, oh->nmesgs);

            /* Link count */
            UINT32ENCODE(p, oh->nlink);

            /* First chunk size */
            UINT32ENCODE(p, (oh->chunk[0].size - H5O_SIZEOF_HDR_OH(oh)));

            /* Zero to alignment */
            HDmemset(p, 0, (size_t)(H5O_SIZEOF_HDR_OH(oh) - 12));
        } /* end else */

        /* Mark chunk 0 as dirty, since the object header prefix has been updated */
        /* (this could be more sophisticated and track whether any prefix fields
         *      have been changed, which could save I/O accesses - QAK)
         */
        HDassert(H5F_addr_eq(addr, oh->chunk[0].addr));
        oh->chunk[0].dirty = TRUE;

	/* Write each chunk to disk, if it's dirty */
	for(u = 0; u < oh->nchunks; u++) {
            /* Sanity check - make certain the magic # is present */
            if(oh->version > H5O_VERSION_1)
                HDassert(!HDmemcmp(oh->chunk[u].image, (u == 0 ? H5O_HDR_MAGIC : H5O_CHK_MAGIC), H5O_SIZEOF_MAGIC));

            /* Write out chunk, if it's dirty */
	    if(oh->chunk[u].dirty) {
                /* Compute checksum, for chunks > 0 & later versions of format */
                if(u > 0 && oh->version > H5O_VERSION_1) {
                    uint32_t metadata_chksum; /* Computed metadata checksum value */

                    /* Compute metadata checksum */
                    metadata_chksum = H5_checksum_metadata(oh->chunk[u].image, (oh->chunk[u].size - H5O_SIZEOF_CHKSUM), 0);

                    /* Metadata checksum */
                    p = oh->chunk[u].image + (oh->chunk[u].size - H5O_SIZEOF_CHKSUM);
                    UINT32ENCODE(p, metadata_chksum);
                } /* end if */

                /* Write the chunk out */
                HDassert(H5F_addr_defined(oh->chunk[u].addr));
                if(H5F_block_write(f, H5FD_MEM_OHDR, oh->chunk[u].addr,
                            oh->chunk[u].size, dxpl_id, oh->chunk[u].image) < 0)
                    HGOTO_ERROR(H5E_OHDR, H5E_WRITEERROR, FAIL, "unable to write object header chunk to disk")

                /* Mark chunk as clean now */
                oh->chunk[u].dirty = FALSE;
	    } /* end if */
	} /* end for */

        /* Mark object header as clean now */
	oh->cache_info.is_dirty = FALSE;
    } /* end if */

    /* Destroy the object header, if requested */
    if(destroy)
        if(H5O_dest(f,oh) < 0)
	    HGOTO_ERROR(H5E_OHDR, H5E_CANTFREE, FAIL, "unable to destroy object header data")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5O_flush() */


/*-------------------------------------------------------------------------
 * Function:	H5O_dest
 *
 * Purpose:	Destroys an object header.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@ncsa.uiuc.edu
 *		Jan 15 2003
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5O_dest(H5F_t UNUSED *f, H5O_t *oh)
{
    unsigned	u;                      /* Local index variable */

    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5O_dest)

    /* check args */
    HDassert(oh);

    /* Verify that node is clean */
    HDassert(oh->cache_info.is_dirty == FALSE);

    /* destroy chunks */
    for(u = 0; u < oh->nchunks; u++) {
        /* Verify that chunk is clean */
        HDassert(oh->chunk[u].dirty == 0);

        oh->chunk[u].image = H5FL_BLK_FREE(chunk_image, oh->chunk[u].image);
    } /* end for */
    if(oh->chunk)
        oh->chunk = H5FL_SEQ_FREE(H5O_chunk_t, oh->chunk);

    /* destroy messages */
    for(u = 0; u < oh->nmesgs; u++) {
        /* Verify that message is clean */
        HDassert(oh->mesg[u].dirty == 0);

        H5O_free_mesg(&oh->mesg[u]);
    } /* end for */
    if(oh->mesg)
        oh->mesg = H5FL_SEQ_FREE(H5O_mesg_t, oh->mesg);

    /* destroy object header */
    H5FL_FREE(H5O_t,oh);

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* end H5O_dest() */


/*-------------------------------------------------------------------------
 * Function:	H5O_clear
 *
 * Purpose:	Mark a object header in memory as non-dirty.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@ncsa.uiuc.edu
 *		Mar 20 2003
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5O_clear(H5F_t *f, H5O_t *oh, hbool_t destroy)
{
    unsigned	u;      /* Local index variable */
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_NOAPI_NOINIT(H5O_clear);

    /* check args */
    assert(oh);

    /* Mark chunks as clean */
    for (u = 0; u < oh->nchunks; u++)
        oh->chunk[u].dirty=FALSE;

    /* Mark messages as clean */
    for (u = 0; u < oh->nmesgs; u++)
        oh->mesg[u].dirty=FALSE;

    /* Mark whole header as clean */
    oh->cache_info.is_dirty=FALSE;

    if (destroy)
        if (H5O_dest(f, oh) < 0)
            HGOTO_ERROR(H5E_OHDR, H5E_CANTFREE, FAIL, "unable to destroy object header data")

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5O_clear() */


/*-------------------------------------------------------------------------
 * Function:	H5O_size
 *
 * Purpose:	Compute the size in bytes of the specified instance of
 *              H5O_t on disk, and return it in *len_ptr.  On failure,
 *              the value of *len_ptr is undefined.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	John Mainzer
 *		5/13/04
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5O_size(const H5F_t UNUSED *f, const H5O_t *oh, size_t *size_ptr)
{
    size_t	size;           /* Running sum of the object header's size */
    unsigned	u;              /* Local index variable */

    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5O_size)

    /* check args */
    HDassert(oh);
    HDassert(size_ptr);

    /* Size of object header "prefix" */
    size = H5O_SIZEOF_HDR_OH(oh);

    /* Add sizes of all the chunks */
    for(u = 0; u < oh->nchunks; u++)
        size += oh->chunk[u].size;

    *size_ptr = size;

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* H5O_size() */

