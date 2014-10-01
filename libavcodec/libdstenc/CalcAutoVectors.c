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

Copyright © 2004.

Source file: CalcAutoVectors.c (Calculation of AutoVectors from AutoSum)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include "CalcAutoVectors.h"
#include "types.h"

/*============================================================================*/
/*       GLOBAL FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/

/*
 * NAME            : CalcAutoVectors
 * 
 * PURPOSE         : Calculation of autocorrelation vectors from bitstream
 *
 * RETURN          : DST_NOERROR (OK)
 * 
 */

ENCODING_STATUS  CalcAutoVectors(/* in */
                                 unsigned char BitStream[MAXCH][MAXCHBITS],
                                 int           ChannelFilter[MAXCH],
                                 int           NrOfChannelBits,
                                 int           NrOfChannels,
                                 int           PredOrder[MAXCH],
                                 /* out */
                                 float         v[MAXCH][MAXAUTOLEN])
{
  static int      Signal[MAXCHBITS];

  int      i;
  int      j;
  int      shift;

  for (i = 0; i < MAXCH; i++) 
  {
    for (shift = 0; shift < MAXAUTOLEN; shift++) 
    {
      v[i][shift]  = 0.0F;
    }
  }
  
  /**********************************************
   *  Decode BitStream values  (1) -> (+1)
   *                           (0) -> (-1)
   **********************************************/

  for (i = 0; i < NrOfChannels; i++) 
  {
    for (j = 0; j < NrOfChannelBits; j++) 
    {
      if (BitStream[i][j] == 0) 
      {
        Signal[j] = -1;
      } 
      else 
      {
        Signal[j] = +1;
      }
    }

    for (shift = 0; shift <= PredOrder[ChannelFilter[i]]; shift++) 
    {
      for (j = shift; j < NrOfChannelBits; j++)
      {
        v[ChannelFilter[i]][shift] += (float)(Signal[j]*Signal[j-shift]);
      }
    }
  }

  return (DST_NOERROR);
}

/* end of file */
