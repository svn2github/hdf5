/*
 * This file is part of the Pablo Performance Analysis Environment
 *
 *          (R)
 * The Pablo    Performance Analysis Environment software is NOT in
 * the public domain.  However, it is freely available without fee for
 * education, research, and non-profit purposes.  By obtaining copies
 * of this and other files that comprise the Pablo Performance Analysis
 * Environment, you, the Licensee, agree to abide by the following
 * conditions and understandings with respect to the copyrighted software:
 * 
 * 1.  The software is copyrighted in the name of the Board of Trustees
 *     of the University of Illinois (UI), and ownership of the software
 *     remains with the UI. 
 *
 * 2.  Permission to use, copy, and modify this software and its documentation
 *     for education, research, and non-profit purposes is hereby granted
 *     to Licensee, provided that the copyright notice, the original author's
 *     names and unit identification, and this permission notice appear on
 *     all such copies, and that no charge be made for such copies.  Any
 *     entity desiring permission to incorporate this software into commercial
 *     products should contact:
 *
 *          Professor Daniel A. Reed                 reed@cs.uiuc.edu
 *          University of Illinois
 *          Department of Computer Science
 *          2413 Digital Computer Laboratory
 *          1304 West Springfield Avenue
 *          Urbana, Illinois  61801
 *          USA
 *
 * 3.  Licensee may not use the name, logo, or any other symbol of the UI
 *     nor the names of any of its employees nor any adaptation thereof in
 *     advertizing or publicity pertaining to the software without specific
 *     prior written approval of the UI.
 *
 * 4.  THE UI MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE
 *     SOFTWARE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS
 *     OR IMPLIED WARRANTY.
 *
 * 5.  The UI shall not be liable for any damages suffered by Licensee from
 *     the use of this software.
 *
 * 6.  The software was developed under agreements between the UI and the
 *     Federal Government which entitle the Government to certain rights.
 *
 **************************************************************************
 *
 * Developed by: The Pablo Research Group
 *               University of Illinois at Urbana-Champaign
 *               Department of Computer Science
 *               1304 W. Springfield Avenue
 *               Urbana, IL     61801
 *
 *               http://www-pablo.cs.uiuc.edu
 *
 * Send comments to: pablo-feedback@guitar.cs.uiuc.edu
 *
 * Copyright (c) 1987-1996
 * The University of Illinois Board of Trustees.
 *      All Rights Reserved.
 *
 * PABLO is a registered trademark of
 * The Board of Trustees of the University of Illinois
 * registered in the U.S. Patent and Trademark Office.
 *
 * Author: Dan Wells (dwells@cs.uiuc.edu)
 *
 * Project Manager and Principal Investigator:
 *      Daniel A. Reed (reed@cs.uiuc.edu)
 *
 * Funded in part by National Science Foundation grants NSF CCR87-06653
 * and NSF CDA87-22836 (Tapestry), DARPA contracts DABT63-91-K-0004,
 * DABT63-93-C-0040, DABT63-94-C-0049 (SIO), and F30602-96-C-0161, NASA
 * contracts NAG-1-613 (ICLASS), USRA 5555-22, and NGT-51023, and a
 * collaborative research agreement with the Intel Supercomputer
 * Systems Division
 */
/*
 * HDFrecord.h: Class to represent HDF records.
 *
 *	$Header$
 */

#ifndef HDF5RECORD_RT_H
#define HDF5RECORD_RT_H
#ifndef NULL
#define NULL 0
#endif 

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "ProcIDs.h"
 
#ifndef min
#define min( x , y ) ( x <= y ? x : y )
#endif
#ifndef max
#define max( x , y ) ( x >= y ? x : y )
#endif
/*======================================================================*
// Enumeration of time fields in an HDFrec_t below			*
//======================================================================*/
enum TimeFields { HDF_, MPI, Malloc, AllIO, Open, Close, Read, Write, ARead,
                  AWrite, Seek, Wait, Misc, nTallyFields };
/*======================================================================*
// Enumeration of byte fields in an HDFrec_t below			*
//======================================================================*/
enum ByteFields{ MallocBytes, ReadBytes, WriteBytes, AReadBytes, AWriteBytes,
                 nByteFields };
/*======================================================================*
// Definition of first and last IO event.         	 		*
//======================================================================*/
#define FirstIO Open
#define LastIO Misc
#define nBkts 4
#define ONEK 1024
int BktLim[] = { 1, 4*ONEK, 64*ONEK, 256*ONEK, INT_MAX } ;
/*======================================================================*
// Definition of structure used to account activity in an HDF call	*
//======================================================================*/
typedef struct {
  	int     nCalls;   		/* number of proc calls		*/
  	double  lastCall;		/* time of last call		*/
  	double  incDur;			/* inclusive duration		*/
  	double  excDur;			/* exclusive duration		*/
	double  times[nTallyFields];	/* Tally op/calls times		*/
	int     counts[nTallyFields];	/* Tally op/calls counts	*/
	int     bytes[nByteFields];	/* Tally bytes transferred	*/
	int     Hists[nByteFields][nBkts];  /* Historgrams		*/
	long	hdfID;			/* data set ID			*/
	long	xRef;			/* data set cross reference	*/
} HDFrec_t;
/*======================================================================*
// Node used to maintain linked lists of HDF procedure activity.       	*
//======================================================================*/
typedef struct HDFnode {
  	double  lastIOtime;		/* last IO time stamp		*/
	HDFrec_t record;	        /* data				*/
	struct HDFnode *ptr;		/* link pointer			*/
	int	eventID;		/* event ID			*/
} HDFnode_t;
/*======================================================================*
// Structure used to produce SDDF packets for Named identifiers.       	*
//======================================================================*/
typedef struct {
        int packetLength;       /* bytes in packet			*/
        int packetType;         /* == PKT_DATA				*/
        int packetTag;          /* == FAMILY_<name>                	*/
        int fileType;           /* Type of data set          		*/
	int fileID;     	/* File ID             			*/
        int nameLen;		/* length of file			*/
} HDFNamePacket_t;
/*======================================================================*
// Node used to form linked lists to track named identifiers.          	*
//======================================================================*/
typedef struct fileRec {
	struct fileRec *ptr;
	int    hdfID;
	int    PabloID;
	char   *fileName;
} fileRec_t;
/*=======================================================================
//	Utility programs to determine field index for a given eventID	* 
//=====================================================================*/
int getHDFFieldIndex( int eventID );
int getHDFByteFieldIndex( int eventID );

#define IOerrorID	      700000

#define openBeginID           700001
#define openEndID             700002

#define fopenBeginID          700003
#define fopenEndID            700004

#define closeBeginID          700005
#define closeEndID            700006

#define fcloseBeginID         700007
#define fcloseEndID           700008

#define readBeginID           700009
#define readEndID             700010

#define freadBeginID          700011
#define freadEndID            700012

#define lseekBeginID          700013
#define lseekEndID            700014

#define fseekBeginID          700015
#define fseekEndID            700016

#define writeBeginID          700017
#define writeEndID            700018

#define fwriteBeginID         700019
#define fwriteEndID           700020

#define fflushBeginID	      700021
#define fflushEndID	      700022

#define flushBeginID	      700023
#define flushEndID	      700024

#define rewindBeginID	      700025
#define rewindEndID	      700026

#define fsetposBeginID	      700027
#define fsetposEndID	      700028

#define lifetimeID	      700040
#define timeSummaryID         700041
#define regionSummaryID       700042

#define IOinitTraceID  	      700100
#define IOendTraceID	      700101
#define IOenableTraceID       700102
#define IOdisableTraceID      700103
#define IOenableDetailID      700104
#define IOdisableDetailID     700105
#define IOenableLifeSummID    700106
#define IOdisableLifeSummID   700107
#define IOenableTimeSummID    700108
#define IOdisableTimeSummID   700109
#define IOchangeTimeWindowID  700110
#define IOenableRegionSummID  700111
#define IOdisableRegionSummID 700112
#define IOchangeFileRegionID  700113

/* 
 * Define the masks for the I/O event families,	    
 */
#define FAMILY_OPEN             0500  
#define FAMILY_FLUSH            0510
#define FAMILY_CLOSE            0520 
#define FAMILY_READ	        0530		
#define FAMILY_SEEK             0540   
#define FAMILY_WRITE	        0550	
#define FAMILY_LIFETIME         0560
#define FAMILY_TIME_SUMMARY     0570
#define FAMILY_REGION_SUMMARY   0600
#define FAMILY_IOTRACE_STATE    0610
#define FAMILY_IO_MISC          0620

/*
 * Define flags to distinguish misc i/o begin from misc i/o end
 */
#define MISC_BEGIN		0
#define MISC_END		1

#endif /* HDF5RECORD_RT_H */
