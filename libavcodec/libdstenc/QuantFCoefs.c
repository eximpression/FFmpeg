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

Source file: QuantFCoefs.c (Quantization of prediction filter coefficients)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/
#include "QuantFCoefs.h"
#include "types.h"
#include <math.h>
#include <float.h>

/*============================================================================*/
/*       GLOBAL FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : OEM_FirQuantFCoefs
 * Description            : Quantization of floating point Prediction Filter coefficients
 * Input                  :  
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               : DST_NOERROR (OK)              
 * Global parameter usage :
 * 
 *****************************************************************************/
ENCODING_STATUS  MANGLE(OEM_FirQuantFCoefs)(/* in */ 
                                    float   FCoef[MAXCH][MAXPREDORDER],
                                    int     NrOfFilters,
                                    int     OptPredOrder[MAXCH],
                                    /* out */
                                    int     ICoef[MAXCH][MAXPREDORDER] )
{  
  int    i;
  int    j;
  float  MaxCoef;

  for (i=0; i<NrOfFilters; i++)
  {
    /* Search for the maximum coefficient */
    MaxCoef = 0.0F;
    
    for(j = 0; j < OptPredOrder[i]; j++)
    {
      if (fabs(FCoef[i][j]) > MaxCoef)
      {
        MaxCoef = (float)fabs(FCoef[i][j]);
      }
    }

    if (MaxCoef < FLT_MIN)
    {
      for(j=0; j < OptPredOrder[i]; j++)
      {
        ICoef[i][j] = 0;
      }
    }
    else
    {
      /* Scale all float coefficients to the found maximum coefficient to use the
          precision of the integer coefficients optimally */
      for(j = 0; j < OptPredOrder[i]; j++)
      {
        ICoef[i][j] = (int) floor( FCoef[i][j] / MaxCoef * (float)PFCOEFSCALER + 0.5F);
      }
    }
  }
  
  ICoef[0][0] = ICoef[0][0] | 0x1;

  return (DST_NOERROR);
}

/* end of file */
