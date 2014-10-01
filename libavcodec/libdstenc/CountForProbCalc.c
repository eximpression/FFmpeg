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

Source file: CountForProbCalc.c (Counting of right and wrong predictions)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include "CountForProbCalc.h"
#include "conststr.h"
#include <math.h>
#include <string.h>

/*============================================================================*/
/*       GLOBAL FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/

/*
 * NAME            : DST_EACCountForProbCalc
 * 
 * PURPOSE         : Counting wrong and right predictions (for p-table generation)
 *                  
 * INPUT           : see definition
 *
 * OUTPUT          : see definition
 *
 * INPUT/OUTPUT    : see definition
 *
 * DATA REFERENCES : -
 *
 * RETURN          : DST_NOERROR (OK)
 * 
 * PRECONDITIONS   :
 *
 * POSTCONDITIONS  :
 *
 */

ENCODING_STATUS  DST_EACCountForProbCalc(/* in */
                                         unsigned char  BitResidual[MAXCH][MAXCHBITS],
                                         int            ChannelFilter[MAXCH],
                                         int            ChannelPtable[MAXCH],
                                         int            NrOfChannelBits,
                                         int            NrOfChannels,
                                         int            OptPredOrder[MAXCH],
                                         int            PtableLen[MAXCH],
                                         unsigned char  Z[MAXCH][MAXCHBITS],
                                         /* out */
                                         int            Count[MAXCH][2][MAXPTABLELEN] )

{
  int ChNr;
  int PtableIndex;
  int BitNr;

  for(ChNr=0; ChNr<NrOfChannels; ChNr++)
  {
    /* Clear counter tables */
    for (PtableIndex=0; PtableIndex<MAXPTABLELEN; PtableIndex++)
    {
      Count[ChNr][0][PtableIndex]=0; /* Right Prediction */
      Count[ChNr][1][PtableIndex]=0; /* Wrong Prediction */
    }

    for (BitNr=OptPredOrder[ChannelFilter[ChNr]]; BitNr<NrOfChannelBits; BitNr++)
    {
      PtableIndex = MIN( Z[ChNr][BitNr], PtableLen[ChannelPtable[ChNr]]-1 );

      if (BitResidual[ChNr][BitNr]==1)
      {
        /* Wrong Prediction */
        Count[ChNr][1][PtableIndex]++;
      }
      else /* BitResidual[ChNr][BitNr]==0 */
      {
        /* Right Prediction */
        Count[ChNr][0][PtableIndex]++;
      }
    }
  }
  return (DST_NOERROR);
}

/* end of file */
