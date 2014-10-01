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

Source file: CalcFCoefs.c (Calculation of prediction filter coefficients)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/

/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include "CalcFCoefs.h"
#include "types.h"
#include <math.h>

/*============================================================================*/
/*       STATIC FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/

/***************************************************************************/
/*                                                                         */
/* name     : chol                                                         */
/*                                                                         */
/* function : Robust method using the Cholesky decomposition.              */
/*            Routine chol() is based on choldet1() from the book          */
/*           "Linear Algebra", Wilkinson & Reinsch, Springer, 1971.        */
/*            The computation of the determinant has been removed.         */
/*                                                                         */
/* pre      : n = E->PredOrder[FilterNr], a[][], p[]                       */
/*                                                                         */
/* post     : a, p, return value is order where decomposition just went OK */
/*            or maximum given prediction order (n)                        */
/*                                                                         */
/* uses     :                                                              */
/*                                                                         */
/***************************************************************************/

int calc_predorder( int n, 
                    float a[MAXPREDORDER][MAXPREDORDER], 
                    float p[MAXPREDORDER])
{
  int     i=0;
  int     j=0;
  int     k;
  int     rv; /* Return Value */
  float   x;
  float   threshold; /* Chol threshold */
  
  threshold = (float)(a[0][0] * TRESHOLD);
  i  = 0;
  rv = n;
  while ((i < n) && (rv == n))
  {
    j = i;
    while ((j < n) && (rv == n))
    {
      x = a[i][j];
      for (k = i - 1; k >= 0; k--)
      {
        x -= a[j][k] * a[i][k]; 
      }
      if (j == i) 
      {
        if (x > threshold)
        {
          p[i] = (float)(1.0 / sqrt(x));
        }
        else
        {
          rv = i;
        }
      }
      else
      {
        a[j][i] = x * p[i];
      }
      j++;
    }
    i++;
  }
  return rv;
} 

/***************************************************************************/
/*                                                                         */
/* name     : cholsol                                                      */
/*                                                                         */
/* function : Routine cholsol() is based on cholsol1() from the book       */
/*           "Linear Algebra", Wilkinson & Reinsch, Springer, 1971.        */
/*                                                                         */
/* pre      : n = E->PredOrder[FilterNr], a, p, b = v+1,                   */
/*            x = &(E->FCoef[])                                           */
/*                                                                         */
/* post     : E->FCoefA[]                                                  */
/*                                                                         */
/* uses     : ?                                                            */
/*                                                                         */
/***************************************************************************/

void calc_order(  int   n, 
                  float *b, 
                  float *x, 
                  float a[MAXPREDORDER][MAXPREDORDER], 
                  float p[MAXPREDORDER]     )
{
  int     i;
  int     k;
  float   z;

  for (i = 0; i < n; i++)
  {
    z = b[i];
    for (k = i - 1; k >= 0; k--)
    {
      z -= a[i][k] * x[k];
    }
    x[i] = z * p[i];
  }
  for (i = n - 1; i >= 0; i--)
  {
    z = x[i];
    for (k = i + 1; k < n; k++)
    {
      z -= a[k][i] * x[k];
    }
    x[i] = z * p[i];
  }
}


/*============================================================================*/
/*       GLOBAL FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/

/*
 * NAME            : OEM_FirCalcFCoefs                                           
 * 
 * PURPOSE         : Calculation of floating point Prediction Filter coefficients
 *
 * RETURN          : DST_NOERROR (OK)
 * 
 */

ENCODING_STATUS OEM_FirCalcFCoefs(/* in */
                                  int     NrOfFilters,
                                  int*    PredOrder,
                                  float  v[MAXCH][MAXAUTOLEN],
                                  /* out */
                                  float   FCoef[MAXCH][MAXPREDORDER],
                                  int*    OptPredOrder )

{
  static float  a[MAXPREDORDER][MAXPREDORDER];
  static float  p[MAXPREDORDER];
  int           FilterNr;
  int           ir;       /* row index pointer */
  int           ic;       /* column index pointer */
  
  /* Filter-based processing */
  for (FilterNr=0; FilterNr<NrOfFilters; FilterNr++)
  {
    /***** Construct autocorrelation matrix *****/
    for (ir=0; ir<PredOrder[FilterNr]; ir++)
    {
      for (ic=ir; ic<PredOrder[FilterNr]; ic++)
      {
        a[ir][ic] = v[FilterNr][ic - ir];
      }
    }

    /***** Decompose and solve *****/
    OptPredOrder[FilterNr] = calc_predorder(PredOrder[FilterNr],a,p);

    if (OptPredOrder[FilterNr] <= 0)
    {
      OptPredOrder[FilterNr] = 1;
      if (v[FilterNr][1] < 0)
      {
        FCoef[FilterNr][0]     = -1.0F;
      }
      else
      {
        FCoef[FilterNr][0]     = 1.0F;
      }
    }
    else
    {
      calc_order(OptPredOrder[FilterNr], &v[FilterNr][1], FCoef[FilterNr], a, p);
    }
  }
  
  return (DST_NOERROR);
}

/* end of file */
