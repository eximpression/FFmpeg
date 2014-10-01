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

Source file: FrameToStream.c (Stream formatter)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include "FrameToStream.h"
#include "conststr.h"
#include <string.h>
#include <stdio.h>

/*============================================================================*/
/*       CONSTANT DEFINITIONS                                                 */
/*============================================================================*/

#define NROFPLAINDSDSTUFFBITS    6

#define BITMASK(data,bitnr) ( (unsigned char)(((data)>>(bitnr))&1) )


static int BitTable[MAXCH];
static int BitTableInit = 0;

/*============================================================================*/
/*       PROTOTYPES STATIC FUNCTIONS                                          */
/*============================================================================*/

static void AddToStream(unsigned char *EncodedFrame,
                        int           DataBit,
                        int           BitNo);

/*============================================================================*/
/*       GLOBAL FUNCTIONS                                                     */
/*============================================================================*/


/*
 * NAME            : DST_StrfFrameToStream
 * 
 * PURPOSE         : Generates the complete stream
 *                  
 * INPUT           : see definition
 *
 * OUTPUT          : see definition
 *
 * INPUT/OUTPUT    : see definition
 *
 * DATA REFERENCES : -
 *
 * RETURN          : DST_NOERROR                 (OK)
 *                   
 */

ENCODING_STATUS  DST_StrfFrameToStream(/* in */
                                       unsigned int   AData[MAXCH*(MAXCHBITS/32)],
                                       int            AritEncoded,
                                       unsigned char* MuxedChannelData,
                                       int            ChannelFilter[MAXCH],            
                                       int            ChannelPTable[MAXCH],
                                       int            NrOfChannelBits,
                                       int            NrOfChannels,
                                       int            ICoef[MAXCH][MAXPREDORDER],
                                       int            NrOfFilters,
                                       int            OptPredOrder[MAXCH],
                                       int            POne[MAXCH][MAXPTABLELEN],
                                       int            NrOfPtables,
                                       int            PtableLen[MAXCH],
                                       /* in/out */                                      
                                       int*           ADataLen,
                                       /* out */
                                       unsigned char* EncodedFrame,
                                       int*           EncodedFrameLen,
                                       int*           LosslessCoded)

{
  int           CountTables;
  int           NrOfBits;
  int           BitToAdd;
  int           ChNr;
  int           BitNr;
  int           i;
  int           j;
  int           k;
  int           ByteAlignBits; 
  int           SameFilterForAllChannels;
  int           SamePTableForAllChannels;
  int           SameMapping;
  int           FDataLen;
  int           PDataLen;
  int           CalcEncFrameLen;  
  int           LLBytes;


  if (BitTableInit == 0)
  {
    int cur_len, next_len;
    BitTable[0] = 0;
    NrOfBits = 1;
    cur_len = 1;
    next_len = 2;
    for(ChNr = 1; ChNr < NrOfChannels; ChNr++)
    {
       BitTable[ChNr] = NrOfBits;
       cur_len--;
       if (cur_len == 0)
       {
         NrOfBits++;
         cur_len = next_len;
         next_len *= 2;
       }
    }
    BitTableInit = 1;
  }

  /* length ...+1 : lossless coded flag + stuffing consumes one byte */
  /* Clear all bits in encoded frame                                 */
  memset(EncodedFrame, 0, NrOfChannels*NrOfChannelBits/(int)ENCWORDLEN + 1 );
  *EncodedFrameLen = 0;
  
  /* 
   * start composing the frame until Segmentation() and Mapping().
   * Then calculate the number of bits needed for a lossless encoded frame.
   */

  BitNr = 0; 

  SameFilterForAllChannels = 0;
  SamePTableForAllChannels = 0;
  SameMapping              = 1;

  AddToStream(EncodedFrame, 1, BitNr); /* Decide later whether LossLessCoded = 1 or 0 */
  BitNr++;

  /**** Add segmentation ****/

  AddToStream(EncodedFrame, 1, BitNr); /* SameSegmentation   */
  BitNr++;
  AddToStream(EncodedFrame, 1, BitNr); /* SameForAllChannels */
  BitNr++;
  AddToStream(EncodedFrame, 1, BitNr); /* EndOfChannel       */
  BitNr++;

  /**** Add mapping ****/

  AddToStream(EncodedFrame, SameMapping, BitNr);
  BitNr++;

  /**** ReadMapping(Filter) ****/
  
  AddToStream(EncodedFrame, SameFilterForAllChannels, BitNr);
  BitNr++;

  CountTables = 1;

  if (SameFilterForAllChannels == 0)
  {
    for(ChNr = 1; ChNr < NrOfChannels; ChNr++)
    {
      NrOfBits = BitTable[CountTables];

      while (NrOfBits > 0)
      {
        NrOfBits--;
        BitToAdd = (ChannelFilter[ChNr] >> NrOfBits) & 1;
        AddToStream(EncodedFrame, BitToAdd, BitNr);
        BitNr++;
      }

      if (ChannelFilter[ChNr] >= CountTables)
      {
        CountTables++;
      }
    }
  }
  else /* SameFilterForAllChannels = 1 */
  {
    /* No bits to add */
  }

  if (SameMapping == 0)

  /* ReadMapping(PTable) */

  {
    AddToStream(EncodedFrame, SamePTableForAllChannels, BitNr);
    BitNr++;

    CountTables = 1;
    if (SamePTableForAllChannels == 0)
    {
      for(ChNr = 1; ChNr < NrOfChannels; ChNr++)
      {
        NrOfBits = BitTable[CountTables];

        while (NrOfBits > 0)
        {
          NrOfBits--;
          BitToAdd = (ChannelPTable[ChNr] >> NrOfBits) & 1;
          AddToStream(EncodedFrame, BitToAdd, BitNr);
          BitNr++;
        }

        if (ChannelPTable[ChNr] >= CountTables)
        {
          CountTables++;
        }
      }
    }
    else /* SamePTableForAllChannels = 1 */
    {
      /* no bits to add to the straem */
    }
  }
  else
  {
    /* CopyMapping(Filter,PTable), no bits to add to the straem */
  }

  /* HalfProb                                                             */
  /* Add for each channel how the first PredOrder bits must be coded      */

    for (i = 0; i < NrOfChannels; i++)
    {
      AddToStream(EncodedFrame, 1, BitNr);
      BitNr++;
    }

  /*** End of Mapping ***/

  /* Count number of bits for Filter_Coef_Sets */
  FDataLen = 0;

  for (i=0; i<NrOfFilters; i++)
  {
    FDataLen += SIZECODEDPREDORDER + 1 + SIZEPREDCOEF*OptPredOrder[i];
  }

  /* Count number of bits for Probability_Tables */
  PDataLen = 0;

  for (i=0; i<NrOfPtables; i++)
  {
    PDataLen += SIZECODEDPTABLELEN;
    
    if (PtableLen[i] != 1)
    {
     PDataLen += 1 + (SIZEPROBS-1)*PtableLen[i];
    }
  }

  /* Calculate the length of the bitstream in bits */
  CalcEncFrameLen  = BitNr + FDataLen + PDataLen + (*ADataLen);

  ByteAlignBits    = (8 - (CalcEncFrameLen % 8)) % 8;

  CalcEncFrameLen  = CalcEncFrameLen + ByteAlignBits;


  if (AritEncoded==0)
  {
    *LosslessCoded=0;
  }
  else /* AritEncoded = 1 */
  {
    if ( CalcEncFrameLen > (NrOfChannels * NrOfChannelBits + 8))
    {
      *LosslessCoded=0;
    }
    else
    {
      *LosslessCoded=1;    
    }
  }

  if (*LosslessCoded == 0)   
  { /* write plain DSD data to the stream */
    memset(EncodedFrame, 0, NrOfChannels*NrOfChannelBits/(int)ENCWORDLEN+1);
    BitNr = 0;

    /* LossLessCoded = 0 */
    AddToStream(EncodedFrame, 0, BitNr); 
    BitNr++;

    /* DST_X_Bit */
    AddToStream(EncodedFrame, 0, BitNr);
    BitNr++;

    /* Stuffing */
    for (i = 0; i < NROFPLAINDSDSTUFFBITS; i++)
    {
      AddToStream(EncodedFrame,0,BitNr);
      BitNr++;
    }

    /* Write plain DSD */
    memcpy(EncodedFrame+1, MuxedChannelData, NrOfChannels*(NrOfChannelBits/8));
    BitNr+=NrOfChannels*NrOfChannelBits;

    /* Set length of encoded stream */
    *EncodedFrameLen = BitNr;

  }  
  else
  { 
    
    /* continue writing lossless coded data to the stream */

    /*** Add Filter coefficients to encoded stream ***/
    for(i=0; i<NrOfFilters; i++)
    {
      /* Coded_Pred_Order */
      for (j=SIZECODEDPREDORDER-1; j>=0; j--)
      {
        AddToStream(EncodedFrame,BITMASK(OptPredOrder[i]-1,j),BitNr);
        BitNr++;
      }
    
      /* Coded_Filter_Coef_Set */
      AddToStream(EncodedFrame,0,BitNr);
      BitNr++;
    
      /* Coefficients */
      for(j=0; j<OptPredOrder[i]; j++)
      {
        for (k = SIZEPREDCOEF - 1; k >=0; k--)
        {
          AddToStream(EncodedFrame,BITMASK(ICoef[i][j],k),BitNr);
          BitNr++;
        }
      }
    }

    /*** Add Ptables ***/
    for(i=0; i<NrOfPtables; i++)
    {
      /* Coded_Ptable_Len */
      for (j=SIZECODEDPTABLELEN-1; j>=0; j--)
      {
        AddToStream(EncodedFrame,BITMASK(PtableLen[i]-1,j),BitNr);
        BitNr++;
      }
    
      if (PtableLen[i] != 1)
      {
        /* Coded_Ptable */
        AddToStream(EncodedFrame,0,BitNr);
        BitNr++;
    
        /* Probability table values */
        for(j=0; j<PtableLen[i]; j++)
        {
          for (k = SIZEPROBS-2; k >=0; k--)
          {
            AddToStream(EncodedFrame,BITMASK(POne[i][j]-1,k),BitNr);
            BitNr++;
          }
        }
      }
    }

    /*** Add arithmetic encoded data ***/
    for (i = 0; i < *ADataLen; i++) 
    {
      AddToStream(EncodedFrame,(AData[i/32] >> (i%32)) & 1,BitNr);
      BitNr++; 
    }

    /* Add zeroes in order to align the bitstream to a byte boundary and */
    /* adjust AdataLen                                                   */

    for (i = 0; i < ByteAlignBits; i++)
    {
      AddToStream(EncodedFrame,0,BitNr);
      BitNr++;
      (*ADataLen)++;
    }

    *EncodedFrameLen = BitNr;

  }

  LLBytes = (BitNr/8) ;
  
  return (DST_NOERROR);
}

/*============================================================================*/
/*       STATIC FUNCTIONS                                                     */
/*============================================================================*/
static void AddToStream(unsigned char *EncodedFrame,
                        int           DataBit,
                        int           BitNo)
{
  EncodedFrame[BitNo/ENCWORDLEN] |= DataBit << ( ENCWORDLEN - 1 - (BitNo%ENCWORDLEN) );
}

/* end of file */

