/***********************************************************************
MPEG-4 Audio RM Module
Lossless coding of 1-bit oversampled audio - DST (Direct Stream Transfer)

This software was originally developed by:

* Aad Rijnberg 
  Philips Digital Systems Laboratories Eindhoven 
  <aad.rijnberg@philips.com>

* Fons Bruekers
  Philips Research Laboratories Eindhoven
  <fons.bruekers@philips.com>
   
* Eric Knapen
  Philips Digital Systems Laboratories Eindhoven
  <h.w.m.knapen@philips.com> 

And edited by:

* Richard Theelen
  Philips Digital Systems Laboratories Eindhoven
  <r.h.m.theelen@philips.com>

in the course of development of the MPEG-4 Audio standard ISO-14496-1, 2 and 3.
This software module is an implementation of a part of one or more MPEG-4 Audio
tools as specified by the MPEG-4 Audio standard. ISO/IEC gives users of the
MPEG-4 Audio standards free licence to this software module or modifications
thereof for use in hardware or software products claiming conformance to the
MPEG-4 Audio standards. Those intending to use this software module in hardware
or software products are advised that this use may infringe existing patents.
The original developers of this software of this module and their company,
the subsequent editors and their companies, and ISO/EIC have no liability for
use of this software module or modifications thereof in an implementation.
Copyright is not released for non MPEG-4 Audio conforming products. The
original developer retains full right to use this code for his/her own purpose,
assign or donate the code to a third party and to inhibit third party from
using the code for non MPEG-4 Audio conforming products. This copyright notice
must be included in all copies of derivative works.

Copyright  2004.

Source file: Types.h (Type definitions)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


#ifndef __TYPES_H_INCLUDED
#define __TYPES_H_INCLUDED

/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include "conststr.h"
#include <stdio.h>

/*============================================================================*/
/*       CONSTANTS                                                            */
/*============================================================================*/

#define TRUE        (1==1)
#define FALSE       (!TRUE)

/*============================================================================*/
/*       TYPE DEFINITIONS                                                     */
/*============================================================================*/

typedef int           LONG;
typedef unsigned int  ULONG;
typedef int           BOOL;
typedef unsigned char BYTE;


/*============================================================================*/
/*       STRUCTURES                                                           */
/*============================================================================*/

typedef struct CoderOptionsInfo
{
  ULONG         NrOfChannels;         /* */
  ULONG         Fsample44;            /* */
  long          ByteStreamLen;        /* MaxFrameLen * NrOfChannels              */
  long          BitStreamLen;         /* ByteStreamLen * BYTESIZE                */
  ULONG         NrOfFramesInFile;     /* Number of frames in the recording       */
  int           MaxPredOrder;         /* Maximum allowed prediction order        */
  char          InFileName[512];      /* Filename for encoder DSD input file     */
  char          OutFileName[512];     /* Filename for encoder outstream          */
} CoderOptions;


typedef struct ebunchdata
{
  /* Declaration of autocorrelation vectors */
  float           FloatV[MAXCH][MAXAUTOLEN];

  /* Declaration of floating point filter sets */
  float           FCoefM[MAXCH][MAXPREDORDER];

  unsigned int    BitResidual[MAXCH][MAXCHBITS/32];
  unsigned int    AData      [MAXCH*(MAXCHBITS/32)];
  int             ADataLen;       /* Number of code bits contained in AData[] */
  
  unsigned char   ChBitStream01[MAXCH][MAXCHBITS];
  unsigned char   ChBitResidual[MAXCH][MAXCHBITS];
  unsigned char   Z[MAXCH][MAXCHBITS];
  unsigned char   BitP[MAXCH][MAXCHBITS];

  int             P_oneM[MAXCH][MAXPTABLELEN];
 
  int             Count[MAXCH][2][MAXPTABLELEN]; /* Has replaced CountWrong & CountRight */

  unsigned int    TotalCountWrong;
  unsigned int    TotalCountRight;

  int             EncodedFrameLen;
  ULONG           Fsample44;
  ULONG           DSDFrameSize;
  unsigned int    NrOfChannels;

  int             ChannelFilter[MAXCH]; /* Which channel uses which filter         */
  int             PredOrder[MAXCH];     /* Prediction order used for this frame    */
  int             ChannelPtable[MAXCH]; /* Which channel uses which filter         */
  int             PtableLen[MAXCH];     /* Nr of Ptable entries used for this frame*/

  int             NrOfFilters;          /* Number of filters used for this frame   */
  int             NrOfPtables;          /* Number of Ptables used for this frame   */
  int             OptPredOrder[MAXCH];
  int             ICoefM[MAXCH][MAXPREDORDER];
  long            FrameNr;              /* Nr of frame that is currently processed */
  int             AritEncoded;
  int             LosslessCoded;        /* 1=lossless coded is put in DST stream, */

} ebunch;


#endif

/* end of file */
