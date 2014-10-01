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

Source file: DST_Fram.c (Framework encoder algorithm)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include "dst_fram.h"
#include "types.h"
#include "conststr.h"
#include "CalcAutoVectors.h"
#include "CalcFCoefs.h"
#include "QuantFCoefs.h"
#include "FIR.h"
#include "CountForProbCalc.h"
#include "GeneratePTables.h"
#include "BitPLookUp.h"
#include "ACEncodeFrame.h"
#include "FrameToStream.h"
#include <string.h>

/*============================================================================*/
/*       GLOBAL FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : DemuxDSDstreamTo01
 * Description            : Demultiplex Multiplexed DSD Buffer
 * Input                  :  
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/

void DemuxDSDstreamTo01(unsigned char*  DsdMuxedFrameBuffer,
                        unsigned char   DsdDemuxedFrameBuffer[MAXCH][MAXCHBITS],
                        int             numberOfChannels,
                        int             framesize)
{
  int   ChNr;
  ULONG BitNr;
  int   WordNr;
  int   Pos;
  ULONG DsdIndex;

  /* input stream expected like DSDIFF stream: channels are byte interleaved! */
  for (ChNr=0; ChNr < numberOfChannels; ChNr++)
  {
    BitNr=0;
    for (WordNr = 0; WordNr < framesize; WordNr++)
    {
      DsdIndex = (WordNr*numberOfChannels + ChNr);
      for (Pos=7; Pos>=0; Pos--)
      {
        DsdDemuxedFrameBuffer[ChNr][BitNr++] = (unsigned char) ((DsdMuxedFrameBuffer[DsdIndex]>>Pos)&0x01);
      }
    }
  }

}


/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : ConvertCharToPacked32Bit
 * Description            :
 * Input                  :  
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
void ConvertCharToPacked32Bit(unsigned char   CharBits[MAXCH][MAXCHBITS],
                              int             NrOfChannels,
                              int             NrOfChannelBits,
                              unsigned int    Packed32Bits[MAXCH][MAXCHBITS/32])
{
  int ChNr;
  int BitNr;

  memset(Packed32Bits, 0x00, MAXCH * MAXCHBITS / 8);

  for ( ChNr=0; ChNr < NrOfChannels; ChNr++)
  {
    for ( BitNr=0; BitNr < NrOfChannelBits; BitNr++)
    { 
      Packed32Bits[ChNr][BitNr/(32)] |= ( ((unsigned int)CharBits[ChNr][BitNr]) << (BitNr%(32)) );
    }
  }
}


/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : DST_FramLosslessEncode
 * Description            : Encode a complete frame (all channels)
 *                          The optimum encoding strategy is assumed to be known
 * Input                  :  
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
ENCODING_STATUS DST_FramLosslessEncode(unsigned char* MuxedChannelData,
                                       unsigned char* DSTFrame,
                                       ebunch*        E )
{
  /* start with proceesing of new frame -> reset ErrorStatus */
  ENCODING_STATUS ErrorStatus = DST_NOERROR; 

  /* set default value; */
  E->EncodedFrameLen = 0;
  
  DemuxDSDstreamTo01(MuxedChannelData,E->ChBitStream01,E->NrOfChannels, E->DSDFrameSize);

/* ---------------------------------------------------------- */
/* Calculation of autocorrelation vectors                     */
/* ---------------------------------------------------------- */
  
  if (DST_NOERROR == ErrorStatus)
  {
    ErrorStatus = CalcAutoVectors(/* input */
                                  E->ChBitStream01,
                                  E->ChannelFilter,
                                  E->DSDFrameSize*BYTESIZE,
                                  E->NrOfChannels,
                                  E->PredOrder,
                                  /* output */
                                  E->FloatV);
  }


/* ---------------------------------------------------------- */
/* Calculation of filter coefficients                         */
/* ---------------------------------------------------------- */
  
  if (DST_NOERROR == ErrorStatus)
  {
    ErrorStatus = OEM_FirCalcFCoefs(/* input */
                                    E->NrOfFilters,
                                    E->PredOrder,
                                    E->FloatV,
                                    /* output */
                                    E->FCoefM,
                                    E->OptPredOrder);
  }


/* ---------------------------------------------------------- */
/* Quantization of filter coefficients                        */
/* ---------------------------------------------------------- */

  if (DST_NOERROR == ErrorStatus)
  {
    ErrorStatus=OEM_FirQuantFCoefs(/* input */
                                   E->FCoefM,
                                   E->NrOfFilters,
                                   E->OptPredOrder,
                                   /* output */
                                   E->ICoefM);
  }

/* ---------------------------------------------------------- */
/* Bit prediction                                             */
/* ---------------------------------------------------------- */

  if (DST_NOERROR == ErrorStatus)
  {
    ErrorStatus=DST_EFirBitPredFilter(E->ChBitStream01,
                                      E->ChannelFilter,
                                      E->ICoefM,
                                      E->DSDFrameSize*BYTESIZE,
                                      E->NrOfChannels,
                                      E->OptPredOrder,
                                      E->ChBitResidual,
                                      E->Z    );

    ConvertCharToPacked32Bit(/* input */
                             E->ChBitResidual, 
                             E->NrOfChannels, 
                             E->DSDFrameSize*BYTESIZE, 
                             /* output */
                             E->BitResidual);

  }

/* ---------------------------------------------------------- */
/* Counting wrong and right predictions                       */
/* ---------------------------------------------------------- */

  if (DST_NOERROR == ErrorStatus)
  {
    ErrorStatus=DST_EACCountForProbCalc(E->ChBitResidual,
                                        E->ChannelFilter,
                                        E->ChannelPtable,
                                        E->DSDFrameSize*BYTESIZE,
                                        E->NrOfChannels,
                                        E->OptPredOrder,
                                        E->PtableLen,
                                        E->Z,
                                        E->Count);
  }


/* ---------------------------------------------------------- */
/* Generation of probability tables                           */
/* ---------------------------------------------------------- */

  if (DST_NOERROR == ErrorStatus)
  {
    ErrorStatus=DST_EACGeneratePtables(E->ChannelPtable,
                                       E->Count,
                                       E->NrOfChannels,
                                       E->NrOfPtables,
                                       E->PtableLen,
                                       &E->TotalCountWrong,
                                       &E->TotalCountRight,
                                       E->P_oneM);
  }

/* ---------------------------------------------------------- */
/* Determination of bit probabilities                         */
/* ---------------------------------------------------------- */

  if (DST_NOERROR == ErrorStatus)
  {
   
    ErrorStatus=DST_EACBitPLookUp(E->ChannelFilter,
                                  E->ChannelPtable,
                                  E->DSDFrameSize*BYTESIZE,
                                  E->NrOfChannels,
                                  E->OptPredOrder,
                                  E->PtableLen,
                                  E->P_oneM,
                                  E->Z,
                                  E->BitP );
    
  }

/* ---------------------------------------------------------- */
/* Arithmetic encoding                                        */
/* ---------------------------------------------------------- */

  if (DST_NOERROR == ErrorStatus)
  {
    if ( ((ULONG)E->TotalCountWrong > (((E->DSDFrameSize*BYTESIZE)/2) * E->NrOfChannels)) )
    {
        /* Too many wrong predicted samples --> do not encode, write plain DSD */
        E->AritEncoded = 0;
        E->ADataLen    = 0;
    }
    else
    {
        ErrorStatus=DST_EACEncodeFrame(/* input */
                                       E->BitP,
                                       E->BitResidual,
                                       E->DSDFrameSize*BYTESIZE,
                                       E->NrOfChannels,
                                       E->ICoefM[0][0],
                                       /* output */
                                       E->AData,
                                       &E->ADataLen,
                                       &E->AritEncoded);
    }
  }

/* ---------------------------------------------------------- */
/* Stream formatting                                          */
/* ---------------------------------------------------------- */

  if (DST_NOERROR == ErrorStatus)
  {
    ErrorStatus=DST_StrfFrameToStream(E->AData,
                                      E->AritEncoded,
                                      MuxedChannelData,
                                      E->ChannelFilter,
                                      E->ChannelPtable,
                                      E->DSDFrameSize*BYTESIZE,
                                      E->NrOfChannels,
                                      E->ICoefM,
                                      E->NrOfFilters,
                                      E->OptPredOrder,
                                      E->P_oneM,
                                      E->NrOfPtables,
                                      E->PtableLen,
                                      &E->ADataLen,
                                      DSTFrame,
                                      &E->EncodedFrameLen,
                                      &E->LosslessCoded);
  }

  return (ErrorStatus);
}

/* end of file */

