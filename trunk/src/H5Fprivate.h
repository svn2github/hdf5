/****************************************************************************
 * NCSA HDF                                                                 *
 * Software Development Group                                               *
 * National Center for Supercomputing Applications                          *
 * University of Illinois at Urbana-Champaign                               *
 * 605 E. Springfield, Champaign IL 61820                                   *
 *                                                                          *
 * For conditions of distribution and use, see the accompanying             *
 * hdf/COPYING file.                                                        *
 *                                                                          *
 ****************************************************************************/

/* $Id$ */

/*
 * This file contains macros & information for file access
 */

#ifndef _H5Fprivate_H
#define _H5Fprivate_H

#include <H5Fpublic.h>

/* This is a near top-level header! Try not to include much! */
#include <H5FDpublic.h>		/*file drivers				     */

typedef struct H5F_t H5F_t;

/*
 * Encode and decode macros for file meta-data.
 * Currently, all file meta-data is little-endian.
 */

/* For non-little-endian platforms, encode each byte by itself */
#ifdef WORDS_BIGENDIAN
#  define INT16ENCODE(p, i) {						      \
   *(p) = (uint8_t)( (uintn)(i)	    & 0xff); (p)++;			      \
   *(p) = (uint8_t)(((uintn)(i) >> 8) & 0xff); (p)++;			      \
}

#  define UINT16ENCODE(p, i) {						      \
   *(p) = (uint8_t)(	    (i)	    & 0xff); (p)++;			      \
   *(p) = (uint8_t)(((uintn)(i) >> 8) & 0xff); (p)++;			      \
}

#  define INT32ENCODE(p, i) {						      \
   *(p) = (uint8_t)( (uint32_t)(i)	  & 0xff); (p)++;		      \
   *(p) = (uint8_t)(((uint32_t)(i) >>  8) & 0xff); (p)++;		      \
   *(p) = (uint8_t)(((uint32_t)(i) >> 16) & 0xff); (p)++;		      \
   *(p) = (uint8_t)(((uint32_t)(i) >> 24) & 0xff); (p)++;		      \
}

#  define UINT32ENCODE(p, i) {						      \
   *(p) = (uint8_t)( (i)        & 0xff); (p)++;				      \
   *(p) = (uint8_t)(((i) >>  8) & 0xff); (p)++;				      \
   *(p) = (uint8_t)(((i) >> 16) & 0xff); (p)++;				      \
   *(p) = (uint8_t)(((i) >> 24) & 0xff); (p)++;				      \
}

#  define INT64ENCODE(p, n) {						      \
   int64_t _n = (n);							      \
   size_t _i;								      \
   uint8_t *_p = (uint8_t*)(p);						      \
   for (_i=0; _i<sizeof(int64_t); _i++, _n>>=8) {			      \
      *_p++ = (uint8_t)(_n & 0xff);					      \
   }									      \
   for (/*void*/; _i<8; _i++) {						      \
      *_p++ = (n)<0 ? 0xff : 0;						      \
   }									      \
   (p) = (uint8_t*)(p)+8;						      \
}

#  define UINT64ENCODE(p, n) {						      \
   uint64_t _n = (n);							      \
   size_t _i;								      \
   uint8_t *_p = (uint8_t*)(p);						      \
   for (_i=0; _i<sizeof(uint64_t); _i++, _n>>=8) {			      \
      *_p++ = (uint8_t)(_n & 0xff);					      \
   }									      \
   for (/*void*/; _i<8; _i++) {						      \
      *_p++ = 0;							      \
   }									      \
   (p) = (uint8_t*)(p)+8;						      \
}

/* DECODE converts little endian bytes pointed by p to integer values and store
 * it in i.  For signed values, need to do sign-extension when converting
 * the last byte which carries the sign bit.
 * The macros does not require i be of a certain byte sizes.  It just requires
 * i be big enough to hold the intended value range.  E.g. INT16DECODE works
 * correctly even if i is actually a 64bit int like in a Cray.
 */

#  define INT16DECODE(p, i) {						      \
   (i)	= (int16_t)((*(p) & 0xff));      (p)++;				      \
   (i) |= (int16_t)(((*(p) & 0xff) << 8) |                                    \
                   ((*(p) & 0x80) ? ~0xffff : 0x0)); (p)++;		      \
}

#  define UINT16DECODE(p, i) {						      \
   (i)	= (uint16_t) (*(p) & 0xff);	  (p)++;			      \
   (i) |= (uint16_t)((*(p) & 0xff) << 8); (p)++;			      \
}

#  define INT32DECODE(p, i) {						      \
   (i)	= (	     *(p) & 0xff);	  (p)++;			      \
   (i) |= ((int32_t)(*(p) & 0xff) <<  8); (p)++;			      \
   (i) |= ((int32_t)(*(p) & 0xff) << 16); (p)++;			      \
   (i) |= ((int32_t)(((*(p) & 0xff) << 24) |                                  \
                   ((*(p) & 0x80) ? ~0xffffffff : 0x0))); (p)++;	      \
}

#  define UINT32DECODE(p, i) {						      \
   (i)	=  (uint32_t)(*(p) & 0xff);	   (p)++;			      \
   (i) |= ((uint32_t)(*(p) & 0xff) <<  8); (p)++;			      \
   (i) |= ((uint32_t)(*(p) & 0xff) << 16); (p)++;			      \
   (i) |= ((uint32_t)(*(p) & 0xff) << 24); (p)++;			      \
}

#  define INT64DECODE(p, n) {						      \
   /* WE DON'T CHECK FOR OVERFLOW! */					      \
   size_t _i;								      \
   n = 0;								      \
   (p) += 8;								      \
   for (_i=0; _i<sizeof(int64_t); _i++) {				      \
      n = (n<<8) | *(--p);						      \
   }									      \
   (p) += 8;								      \
}

#  define UINT64DECODE(p, n) {						      \
   /* WE DON'T CHECK FOR OVERFLOW! */					      \
   size_t _i;								      \
   n = 0;								      \
   (p) += 8;								      \
   for (_i=0; _i<sizeof(uint64_t); _i++) {				      \
      n = (n<<8) | *(--p);						      \
   }									      \
   (p) += 8;								      \
}

#else
   /* For little-endian platforms, make the compiler do the work */
#  define INT16ENCODE(p, i) {*((int16_t*)(p))=(int16_t)(i);(p)+=2;}
#  define UINT16ENCODE(p, i) {*((uint16_t*)(p))=(uint16_t)(i);(p)+=2;}
#  define INT32ENCODE(p, i)  {*((int32_t*)(p))=(int32_t)(i);(p)+=4;}
#  define UINT32ENCODE(p, i) {*((uint32_t*)(p))=(uint32_t)(i);(p)+=4;}

#  define INT64ENCODE(p, i)  {						      \
   *((int64_t *)(p)) = (int64_t)(i);					      \
   (p) += sizeof(int64_t);						      \
   if (4==sizeof(int64_t)) {						      \
      *(p)++ = (i)<0?0xff:0x00;						      \
      *(p)++ = (i)<0?0xff:0x00;						      \
      *(p)++ = (i)<0?0xff:0x00;						      \
      *(p)++ = (i)<0?0xff:0x00;						      \
   }									      \
}

#  define UINT64ENCODE(p, i) {						      \
   *((uint64_t *)(p)) = (uint64_t)(i);					      \
   (p) += sizeof(uint64_t);						      \
   if (4==sizeof(uint64_t)) {						      \
      *(p)++ = 0x00;							      \
      *(p)++ = 0x00;							      \
      *(p)++ = 0x00;							      \
      *(p)++ = 0x00;							      \
   }									      \
}

#  define INT16DECODE(p, i)  {(i)=(int16_t)(*(const int16_t*)(p));(p)+=2;}
#  define UINT16DECODE(p, i) {(i)=(uint16_t)(*(const uint16_t*)(p));(p)+=2;}
#  define INT32DECODE(p, i)  {(i)=(int32_t)(*(const int32_t*)(p));(p)+=4;}
#  define UINT32DECODE(p, i) {(i)=(uint32_t)(*(const uint32_t*)(p));(p)+=4;}
#  define INT64DECODE(p, i)  {(i)=(int64_t)(*(const int64_t*)(p));(p)+=8;}
#  define UINT64DECODE(p, i) {(i)=(uint64_t)(*(const uint64_t*)(p));(p)+=8;}

#endif

#define NBYTEENCODE(d, s, n) {	 HDmemcpy(d,s,n); p+=n }

/*
 * Note:  the NBYTEDECODE macro is backwards from the memcpy() routine, in
 *	  the spirit of the other DECODE macros.
 */
#define NBYTEDECODE(s, d, n) {	 HDmemcpy(d,s,n); p+=n }

/* Address-related macros */
#define H5F_addr_overflow(X,Z)	(HADDR_UNDEF==(X) ||			      \
				 HADDR_UNDEF==(X)+(haddr_t)(Z) ||	      \
				 (X)+(haddr_t)(Z)<(X))
#define H5F_addr_hash(X,M)	((unsigned)((X)%(M)))
#define H5F_addr_defined(X)	(X!=HADDR_UNDEF)
#define H5F_addr_eq(X,Y)	((X)!=HADDR_UNDEF &&			      \
				 (Y)!=HADDR_UNDEF &&			      \
				 (X)==(Y))
#define H5F_addr_ne(X,Y)	(!H5F_addr_eq((X),(Y)))
#define H5F_addr_lt(X,Y) 	((X)!=HADDR_UNDEF &&			      \
				 (Y)!=HADDR_UNDEF &&			      \
				 (X)<(Y))
#define H5F_addr_le(X,Y)	((X)!=HADDR_UNDEF &&			      \
				 (Y)!=HADDR_UNDEF &&			      \
				 (X)<=(Y))
#define H5F_addr_gt(X,Y)	((X)!=HADDR_UNDEF &&			      \
				 (Y)!=HADDR_UNDEF &&			      \
				 (X)>(Y))
#define H5F_addr_ge(X,Y)	((X)!=HADDR_UNDEF &&			      \
				 (Y)!=HADDR_UNDEF &&			      \
				 (X)>=(Y))
#define H5F_addr_cmp(X,Y)	(H5F_addr_eq(X,Y)?0:			      \
				 (H5F_addr_lt(X, Y)?-1:1))
#define H5F_addr_pow2(N)	((haddr_t)1<<(N))

/* size of size_t and off_t as they exist on disk */
#ifdef H5F_PACKAGE
#define H5F_SIZEOF_ADDR(F)	((F)->shared->fcpl->sizeof_addr)
#define H5F_SIZEOF_SIZE(F)	((F)->shared->fcpl->sizeof_size)
#else /* H5F_PACKAGE */
#define H5F_SIZEOF_ADDR(F)	(H5F_sizeof_addr(F))
#define H5F_SIZEOF_SIZE(F)	(H5F_sizeof_size(F))
#endif /* H5F_PACKAGE */
__DLL__ size_t H5F_sizeof_addr(H5F_t *f);
__DLL__ size_t H5F_sizeof_size(H5F_t *f);

/* Macros to encode/decode offset/length's for storing in the file */
#ifdef NOT_YET
#define H5F_ENCODE_OFFSET(f,p,o) (H5F_SIZEOF_ADDR(f)==4 ? UINT32ENCODE(p,o) \
    : H5F_SIZEOF_ADDR(f)==8 ? UINT64ENCODE(p,o) \
    : H5F_SIZEOF_ADDR(f)==2 ? UINT16ENCODE(p,o) \
    : H5FPencode_unusual_offset(f,&(p),(uint8_t*)&(o)))
#else /* NOT_YET */
#define H5F_ENCODE_OFFSET(f,p,o) switch(H5F_SIZEOF_ADDR(f)) {		      \
    case 4: UINT32ENCODE(p,o); break;					      \
    case 8: UINT64ENCODE(p,o); break;					      \
    case 2: UINT16ENCODE(p,o); break;					      \
}
#endif /* NOT_YET */

#define H5F_DECODE_OFFSET(f,p,o) switch (H5F_SIZEOF_ADDR (f)) {	\
   case 4: UINT32DECODE (p, o);	break;							\
   case 8: UINT64DECODE (p, o);	break;							\
   case 2: UINT16DECODE (p, o);	break;							\
}

#define H5F_ENCODE_LENGTH(f,p,l) switch(H5F_SIZEOF_SIZE(f)) {   \
   case 4: UINT32ENCODE(p,l); break;					      \
   case 8: UINT64ENCODE(p,l); break;					      \
   case 2: UINT16ENCODE(p,l); break;					      \
}

#define H5F_DECODE_LENGTH(f,p,l) switch(H5F_SIZEOF_SIZE(f)) {   \
   case 4: UINT32DECODE(p,l); break;					      \
   case 8: UINT64DECODE(p,l); break;					      \
   case 2: UINT16DECODE(p,l); break;					      \
}

/*
 * File-creation property list.
 */
typedef struct H5F_create_t {
    hsize_t	userblock_size;	/* Size of the file user block in bytes */
    intn	sym_leaf_k;	/* 1/2 rank for symbol table leaf nodes */
    intn	btree_k[8];	/* 1/2 rank for btree internal nodes	*/
    size_t	sizeof_addr;	/* Number of bytes in an address	*/
    size_t	sizeof_size;	/* Number of bytes for obj sizes	*/
    intn	bootblock_ver;	/* Version # of the bootblock		*/
    intn	freespace_ver;	/* Version # of the free-space information*/
    intn	objectdir_ver;	/* Version # of the object directory format*/
    intn	sharedheader_ver;/* Version # of the shared header format */
} H5F_create_t;

/*
 * File-access property list.
 */
typedef struct H5F_access_t {
    intn	mdc_nelmts;	/* Size of meta data cache (elements)	*/
    intn	rdcc_nelmts;	/* Size of raw data chunk cache (elmts)	*/
    size_t	rdcc_nbytes;	/* Size of raw data chunk cache	(bytes)	*/
    double	rdcc_w0;	/* Preempt read chunks first? [0.0..1.0]*/
    hsize_t	threshold;	/* Threshold for alignment		*/
    hsize_t	alignment;	/* Alignment				*/
    size_t	meta_block_size;    /* Minimum metadata allocation block size (when aggregating metadata allocations) */
    hsize_t	sieve_buf_size;     /* Maximum sieve buffer size (when data sieving is allowed by file driver) */
    uintn	gc_ref;		/* Garbage-collect references?		*/
    hid_t	driver_id;	/* File driver ID			*/
    void	*driver_info;	/* File driver specific information	*/
} H5F_access_t;

/* Mount property list */
typedef struct H5F_mprop_t {
    hbool_t		local;	/* Are absolute symlinks local to file?	*/
} H5F_mprop_t;

/* library variables */
__DLLVAR__ const H5F_create_t H5F_create_dflt;
__DLLVAR__ H5F_access_t H5F_access_dflt;
__DLLVAR__ const H5F_mprop_t H5F_mount_dflt;

/* Forward declarations for prototypes arguments */
struct H5O_layout_t;
struct H5O_efl_t;
struct H5O_pline_t;
struct H5O_fill_t;
struct H5G_entry_t;
struct H5S_t;

/* Private functions, not part of the publicly documented API */
__DLL__ herr_t H5F_init(void);
__DLL__ uintn H5F_get_intent(H5F_t *f);
__DLL__ hid_t H5F_get_driver_id(H5F_t *f);

/* Functions that operate on array storage */
__DLL__ herr_t H5F_arr_create(H5F_t *f,
			      struct H5O_layout_t *layout /*in,out*/);
__DLL__ herr_t H5F_arr_read (H5F_t *f, hid_t dxpl_id,
			     const struct H5O_layout_t *layout,
			     const struct H5O_pline_t *pline,
			     const struct H5O_fill_t *fill,
			     const struct H5O_efl_t *efl,
			     const hsize_t _hslab_size[],
			     const hsize_t mem_size[],
			     const hssize_t mem_offset[],
			     const hssize_t file_offset[], void *_buf/*out*/);
__DLL__ herr_t H5F_arr_write (H5F_t *f, hid_t dxpl_id,
			      const struct H5O_layout_t *layout,
			      const struct H5O_pline_t *pline,
			      const struct H5O_fill_t *fill,
			      const struct H5O_efl_t *efl,
			      const hsize_t _hslab_size[],
			      const hsize_t mem_size[],
			      const hssize_t mem_offset[],
			      const hssize_t file_offset[], const void *_buf);

/* Functions that operate on blocks of bytes wrt boot block */
__DLL__ herr_t H5F_block_read(H5F_t *f, H5FD_mem_t type, haddr_t addr, hsize_t size,
			      hid_t dxpl_id, void *buf/*out*/);
__DLL__ herr_t H5F_block_write(H5F_t *f, H5FD_mem_t type, haddr_t addr,
                  hsize_t size, hid_t dxpl_id, const void *buf);

/* Functions that operate on byte sequences */
__DLL__ herr_t H5F_seq_read(H5F_t *f, hid_t dxpl_id,
        const struct H5O_layout_t *layout, const struct H5O_pline_t *pline,
        const struct H5O_fill_t *fill, const struct H5O_efl_t *efl,
        const struct H5S_t *file_space, size_t elmt_size, hsize_t seq_len,
        hssize_t mem_offset, hssize_t file_offset, void *_buf/*out*/);
__DLL__ herr_t H5F_seq_write (H5F_t *f, hid_t dxpl_id,
        const struct H5O_layout_t *layout, const struct H5O_pline_t *pline,
        const struct H5O_fill_t *fill, const struct H5O_efl_t *efl,
        const struct H5S_t *file_space, size_t elmt_size, hsize_t seq_len,
        hssize_t mem_offset, hssize_t file_offset, const void *_buf);


/* Functions that operate on indexed storage */
__DLL__ hsize_t H5F_istore_allocated(H5F_t *f, int ndims, haddr_t addr);
__DLL__ herr_t H5F_istore_dump_btree(H5F_t *f, FILE *stream, int ndims,
				     haddr_t addr);

/* Functions for allocation/releasing chunks */
__DLL__ void * H5F_istore_chunk_free(void *chunk);

/* Address-related functions */
__DLL__ void H5F_addr_encode(H5F_t *, uint8_t**/*in,out*/, haddr_t);
__DLL__ void H5F_addr_decode(H5F_t *, const uint8_t**/*in,out*/,
			     haddr_t*/*out*/);
__DLL__ herr_t H5F_addr_pack(H5F_t UNUSED *f, haddr_t *addr_p/*out*/,
			     const unsigned long objno[2]);

#endif
