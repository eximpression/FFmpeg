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

Source file: FIO_DSD.h (File I/O DSD stream)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


#ifndef __FIO_DSD_H_INCLUDED
#define __FIO_DSD_H_INCLUDED

/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include "types.h"
#include "conststr.h"
#include <stdio.h>

/*============================================================================*/
/*       FUNCTION PROTOTYPES                                                  */
/*============================================================================*/

int FIO_retrieveNumberOfChannels (void);
int FIO_DSDCheckFileAndSkip(ULONG* TotFr);

RDFRAME_STATUS FIO_DSDReadFrame(unsigned char *DF);
void           FIO_DSTWriteFrame (unsigned char *EncodedFrame,int EncodedFrameLen);

void FIO_WriteDSTHeader (void);
void FIO_WriteDSTChunkSizes (void);

BOOL FIO_Open (char*  FileIn,
               char*  FileOut,
               ULONG* nrOfChannels,
               ULONG* Fsample44,
               ULONG* nrOfFramesInFile);
void FIO_Close (void);

#endif

/* end of file */
