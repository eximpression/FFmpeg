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

Source file: GeneratePTables.c (Probability Table Merge)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


#include "GeneratePTables.h"
#include "conststr.h"
#include <math.h>
#include <stdio.h>

/*============================================================================*/
/*       GLOBAL FUNCTIONS                                                     */
/*============================================================================*/

/*
 * NAME            : DST_EACGeneratePtables
 * 
 * PURPOSE         : Fill probability tables; p-table values calculated from count values
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
 */

ENCODING_STATUS  MANGLE(DST_EACGeneratePtables)(/* in */
                                        int           ChannelPtable[MAXCH],
                                        int           Count[MAXCH][2][MAXPTABLELEN],
                                        int           NrOfChannels,
                                        int           NrOfPtables,
                                        int           PtableLen[MAXCH],
                                        /* out */
                                        unsigned int  *TotalCountWrong,
                                        unsigned int  *TotalCountRight,
                                        int           POne[MAXCH][MAXPTABLELEN] )


{
  int PtableNr;
  int PtableIndex;
  int CntRightPt;
  int CntWrongPt;
  int CntAllPt;
  int ChNr;

  *TotalCountWrong = 0;
  *TotalCountRight = 0;

  /* Merging of counter tables and calculation of P_one table */
  for(PtableNr=0; PtableNr<NrOfPtables; PtableNr++)
  {

    for (PtableIndex=0; PtableIndex<PtableLen[PtableNr]; PtableIndex++)
    {
      CntRightPt=0;
      CntWrongPt=0;

      for (ChNr=0; ChNr<NrOfChannels; ChNr++)
      {
        if (ChannelPtable[ChNr]==PtableNr)
        {
          CntRightPt+=Count[ChNr][0][PtableIndex]; /* CountRight[ChNr][PtableIndex] */
          CntWrongPt+=Count[ChNr][1][PtableIndex]; /* CountWrong[ChNr][PtableIndex] */
        }
      }
      CntAllPt=CntRightPt+CntWrongPt;
      *TotalCountWrong += CntWrongPt;
      *TotalCountRight += CntRightPt;

      if (CntAllPt>0)
      {
        POne[PtableNr][PtableIndex]= MIN( (CntAllPt + (PROBLEVELS<<1)*CntWrongPt)/(CntAllPt<<1), 
                                          PROBLEVELS/2 );
        POne[PtableNr][PtableIndex]= MAX( POne[PtableNr][PtableIndex], 1 );
      }
      else
      {
        POne[PtableNr][PtableIndex]=1;
      }
    }
  }

  return (DST_NOERROR);
}

/* end of file */
