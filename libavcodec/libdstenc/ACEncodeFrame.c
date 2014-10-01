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

Source file: ACEncodeFrame.c (Arithmetic encoder algorithm)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include "ACEncodeFrame.h"
#include "conststr.h"
#include <math.h>
#include <stdio.h>


/*============================================================================*/
/*       CONSTANT DEFINITIONS                                                 */
/*============================================================================*/

#define PBITS   SIZEPROBS
#define ABITS   (PBITS + 4)
#define ONE     (1 << ABITS)
#define HALF    (1 << (ABITS-1))

/*============================================================================*/
/*       STATIC VARIABLES                                                     */
/*============================================================================*/

static int rev7LSBtable[128] =
{
   1, 65, 33,  97, 17, 81, 49, 113,
   9, 73, 41, 105, 25, 89, 57, 121,
   5, 69, 37, 101, 21, 85, 53, 117,
  13, 77, 45, 109, 29, 93, 61, 125,
   3, 67, 35,  99, 19, 83, 51, 115,
  11, 75, 43, 107, 27, 91, 59, 123,
   7, 71, 39, 103, 23, 87, 55, 119,
  15, 79, 47, 111, 31, 95, 63, 127,
   2, 66, 34,  98, 18, 82, 50, 114,
  10, 74, 42, 106, 26, 90, 58, 122,
   6, 70, 38, 102, 22, 86, 54, 118,
  14, 78, 46, 110, 30, 94, 62, 126,
   4, 68, 36, 100, 20, 84, 52, 116,
  12, 76, 44, 108, 28, 92, 60, 124,
   8, 72, 40, 104, 24, 88, 56, 120,
  16, 80, 48, 112, 32, 96, 64, 128
};

/*============================================================================*/
/*       PROTOTYPES STATIC FUNCTIONS                                          */
/*============================================================================*/

static int Reverse7LSBs(int c);

/*============================================================================*/
/*       GLOBAL FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : DST_EACEncodeFrame
 * Description            : 
 * Input                  :  
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               : DST_NOERROR (OK)
 * Global parameter usage :
 * 
 *****************************************************************************/
ENCODING_STATUS MANGLE(DST_EACEncodeFrame)( /* in */
                                    unsigned char   BitP[MAXCH][MAXCHBITS],
                                    unsigned int    BitRes[MAXCH][MAXCHBITS/32],
                                    int             NrOfChannelBits,
                                    int             NrOfChannels,
                                    int             Current_PC_dstXbit,
                                    /* out */
                                    unsigned int    AData[MAXCH*(MAXCHBITS/32)], 
                                    int             *ADataLen,
                                    int             *AritEncoded )
{
  int               BitNr;
  int               MaxADataLen;
  unsigned int      C;
  unsigned int      A;
  unsigned int      AndBit;
  int               BitsToFollow;
  int               i, j, DWordNr,n;
  int               Temp;
  int               P_dstXbit;
  int               DSDArea[11], Area;
  int               Half_DSDframeSize = NrOfChannelBits / (8*2);

  /**********************/
  /***** Initialize *****/
  /**********************/
  C            = 0;
  A            = ONE-1;
  BitsToFollow = 0;
  MaxADataLen  = NrOfChannels * NrOfChannelBits;
  i            = 0;

  P_dstXbit = Reverse7LSBs(Current_PC_dstXbit);

  A = A - ((A >> PBITS) | ((A >> (PBITS - 1)) & 1)) * (P_dstXbit);

  /* Renormalize */
  while (A < HALF)
  {
    if ((C & (HALF | ONE)) == HALF)
    {
      /* extend current sequences */
      BitsToFollow++;
      C ^= HALF;
    }
    else
    {
      Temp = ((C & ONE) << (31 - ABITS));
      AData[i/32] = (AData[i/32] >> 1) | Temp;
      i++;
      while (BitsToFollow > 0)
      {
        AData[i/32] = (AData[i/32] >> 1) | (Temp^0x80000000);
        i++;
        BitsToFollow--;
      }
      C = C & (0xFFFFFFFF ^ ONE);
    }
    C <<= 1;
    A <<= 1;
  }                                                             \

  /* Limited bit expansion areas */
  for (j=0; j<5; j++) 
  {
    DSDArea[j]    = (j+1)*Half_DSDframeSize;
    DSDArea[10-j] = NrOfChannelBits-(j*Half_DSDframeSize);
  }
  DSDArea[5] = NrOfChannelBits-5*Half_DSDframeSize;

  /****************************/
  /***** Process all bits *****/
  /****************************/

  BitNr=0;
  
  /* Loop added for check on limited expansion */
  for (j=0; j<11; j++)
  {
    Area=DSDArea[j]; /* Process DSD data until this bit */
      
    while ((i < MaxADataLen) && (BitNr < Area))
    {
      AndBit = (1 <<  (BitNr % 32));
      DWordNr = BitNr / 32;
      for (n=0; n<NrOfChannels; n++)
      {
        /* Update C and A */
        if ((BitRes[n][DWordNr] & AndBit) != 0)
        {
          C += A;
          A  = ((A >> PBITS) | ((A >> (PBITS - 1)) & 1)) * BitP[n][BitNr];
          C -= A;
        }
        else
        {
          A = A - ((A >> PBITS) | ((A >> (PBITS - 1)) & 1)) * (BitP[n][BitNr]);
        }

        /* Renormalize */
        while (A < HALF)
        {
          if ((C & (HALF | ONE)) == HALF)
          {
            /* extend current sequences */
            BitsToFollow++;
            C ^= HALF;
          }
          else
          {
            Temp = ((C & ONE) << (31 - ABITS));
            AData[i/32] = (AData[i/32] >> 1) | Temp;
            i++;
            while (BitsToFollow > 0)
            {
              AData[i/32] = (AData[i/32] >> 1) | (Temp^0x80000000);
              i++;
              BitsToFollow--;
            }
            C = C & (0xFFFFFFFF ^ ONE);
          }
          C <<= 1;
          A <<= 1;
        }                                                             \
      }
      BitNr++;
    }
  }


  /*****************/
  /***** Flush *****/
  /*****************/

  /* Use new flushing method that transmits only 1 bit of C after */
  /* transmitting the BitsToFollow bits.                          */

  if ((C <= ONE) && (C + A > ONE))
  {
    C = ONE;
  }
  else
  {
    C += HALF - 1;
  }
  
  if ((i+BitsToFollow+2) < MaxADataLen)
  {

    /* write out MSB of C and bits in queue */
    if ((C & ONE) != 0)
    {
      AData[i/32] = (AData[i/32] >> 1) | 0x80000000;
      i++;
      while (BitsToFollow > 0)
      {
        AData[i/32] = (AData[i/32] >> 1) & 0x7FFFFFFF;
        i++;
        BitsToFollow--;
      }
    }
    else
    {
      AData[i/32] = (AData[i/32] >> 1) & 0x7FFFFFFF;
      i++;
      while (BitsToFollow > 0)
      {
        AData[i/32] = (AData[i/32] >> 1) | 0x80000000;
        i++;
        BitsToFollow--;
      }
    }
    
    C = (C & (ONE - 1)) << 1;
    if ((C & ONE) != 0)
    {
      AData[i/32] = (AData[i/32] >> 1) | 0x80000000;
    }
    else
    {
      AData[i/32] = (AData[i/32] >> 1) & 0x7FFFFFFF;
    }
    i++;

    AData[0] = AData[0] & 0xFFFFFFFE;

    j = i;
    while (j % 32 !=0)
    {  
        AData[j/32] = (AData[j/32] >> 1) & 0x7FFFFFFF;
        j++;
    }
  }
  else
  {
      i = MaxADataLen;
  }

  if (i < MaxADataLen)
  {
       
    /* Remove trailing zeroes from the arithmetic encoded stream */

    while ( (i > 0) && ( ( (AData[(i-1)/32] >> ((i-1)%32)) & 1) == 0 ) )
    {
      i--;
    }

    *AritEncoded = 1;
    *ADataLen    = i;
    
  }
  else
  {
    *AritEncoded = 0;
    *ADataLen    = 0;
  }
 
  return (DST_NOERROR);
}

/*============================================================================*/
/*       STATIC FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/

/* Take the 7 LSBs of a number consisting of SIZE_PREDCOEF bits  */
/* (2's complement), reverse the bit order and add 1 to it.      */
static int Reverse7LSBs(int c)
{
  int LSBs;

  if (c >= 0)
  {
    LSBs = c & 127;
  }
  else
  {
    LSBs = ((1 << SIZEPREDCOEF) + c) & 127;
  }
  return(rev7LSBtable[LSBs]); 
}

#undef PBITS
#undef ABITS
#undef HALF
#undef ONE

/* end of file */

