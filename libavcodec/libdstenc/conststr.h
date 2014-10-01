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

Source file: ConstStr.h (Constants)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


#ifndef __CONSTSTR_H_INCLUDED
#define __CONSTSTR_H_INCLUDED

/*============================================================================*/
/*       CONSTANTS                                                            */
/*============================================================================*/

/***** GENERAL *****/

#define BYTESIZE            8  /* Number of bits per byte */

#define MAXCH              64  /* Maximum number of channels                   */

/* maximum no of bits per channel within a frame */
/* (#bits/channel)/frame at 64 x fs              */
/* for  64FS DSD   37632                         */
/* for 128FS DSD   75264                         */
/* for 256FS DSD  150528                         */

#define MAXCHBITS           (150528)
#define DSDFRAMESIZE        (MAXCHBITS/8)

#define DSDFRAMESIZE_ALLCH  (DSDFRAMESIZE*MAXCH)

/***** CHOLESKY ALGORITHM *****/
#define TRESHOLD            0.334 /* treshold in function Chol() */


/***** PREDICTION FILTER *****/

#define SIZECODEDPREDORDER  7   /* Number of bits for representing the prediction */
                                /* order (for each channel) in each frame         */
                                /* SIZECODEDPREDORDER = 7 (128)                   */

#define MAXPREDORDER        (1<<(SIZECODEDPREDORDER)) 
                               /* Maximum prediction filter order */

#define SIZEPREDCOEF        9  /* Number of bits for representing each filter 
                                  coefficient in each frame */

#define PFCOEFSCALER        ((1 << ((SIZEPREDCOEF) - 1)) - 1)
                               /* Scaler for coefficient normalization */


/***** P-TABLES *****/

#define SIZECODEDPTABLELEN  6  /* Number bits for p-table length */

#define MAXPTABLELEN        (1<<(SIZECODEDPTABLELEN))
                               /* Maximum length of p-tables */ 

/* P-table indices */

#define MAXPTIND            ((MAXPTABLELEN)-1)
                               /* Maximum level for p-table indices */
                              
#define ACQSTEP             ((SIZEPREDCOEF)-(SIZECODEDPTABLELEN)) 
                               /* Quantization step for histogram */

                              
/* P-table values */

#define SIZEPROBS           8  /* Number of bits for coding probabilities */   

#define PROBLEVELS          (1<<(SIZEPROBS)) /* Number of probability levels 
                                                Maximum level: ((PROBLEVELS)-1) */


/***** ARITHMETIC CODE *****/

#define SIZEADATALENGTH     18 /* Number of bits for representing the length of
                                  the arithmetic coded sequence */

#define MAXADATALEN         (1<<(SIZEADATALENGTH))
                               /* Maximum length of Arithmetic encoded data  */

/***** DIMENSIONS *****/

#define MAXAUTOLEN          ((MAXPREDORDER)+1)
                             /* Length of autocorrelation vectors */

#define FRAMEHEADERLEN      8     /* Maximum length of the encoded frame header in bits */

#define ENCWORDLEN          8     /* Length of output words in encoded stream */

#define MAXENCFRAMELEN      ( (MAXCH)*(MAXCHBITS)+(FRAMEHEADERLEN))
                             /* Maximum length of encoded frame in bits
                                number of output words in encoded stream =
                                MAXENCFRAMELEN / ENCWORDLEN */

/*============================================================================*/
/*       STATUS MESSAGES                                                      */
/*============================================================================*/

typedef enum
{
  DST_NOERROR                 =    0,
  DST_ERROR                   =   -1
} ENCODING_STATUS;

typedef enum
{
  NOFRAMEERROR,
  NOMOREFRAMES,
  FRAMEINCOMPLETE
} RDFRAME_STATUS;


/*============================================================================*/
/*       MACRO'S                                                              */
/*============================================================================*/

#define MAX(x,y)    (((x) > (y)) ? (x) : (y))

#define MIN(x,y)    (((x) < (y)) ? (x) : (y))

#endif

/* end of file */
