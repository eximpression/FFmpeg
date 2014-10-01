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

Source file: DSTEncoder.c (DST Encoder Module)

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
#include "dst_init.h"
#include "DSTEncoder.h"
#include <malloc.h>

/*============================================================================*/
/*       STATIC VARIABLES                                                     */
/*============================================================================*/

static ebunch*          E;

/*============================================================================*/
/*       GLOBAL FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : Init
 * Description            : Initialises the encoder component.
 * Input                  : NrOfChannels: 1 .. MAXCH
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               : True if Init ok, else false
 * Global parameter usage :
 * 
 *****************************************************************************/
BOOL MANGLE(Init)(int NrOfChannels, int Fsample44)
{
    BOOL result = FALSE;

    if ( (NrOfChannels <= MAXCH) &&
         (Fsample44 == 64 || Fsample44 == 128 || Fsample44 == 256)        )
    {
      E = malloc(sizeof(ebunch));

      result            = TRUE;
      E->NrOfChannels   = NrOfChannels;
      E->Fsample44       = Fsample44; /* 64, 128, 256 */
      E->DSDFrameSize    = 588 * E->Fsample44 / 8;

      MANGLE(DST_InitEncInitialisation)(E);
    }
    return result;
}

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : Encode
 * Description            : Encodes the DSD data into a DST frame
 * Input                  : MuxedChannelData: Multiplexed DSD input data
 *                          FrameCnt        : Frame number
 *                          DSTFrame        : DST encoded data
 *                          FrameSize       : actual frame size in bits after
 *                                            encoding
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
ENCODING_STATUS MANGLE(Encode)( unsigned char* MuxedChannelData,
                        unsigned char* DSTFrame, int* FrameSize)
{
    ENCODING_STATUS result;

    result = MANGLE(DST_FramLosslessEncode)(MuxedChannelData,DSTFrame,E);

    *FrameSize = E->EncodedFrameLen;

    return result;
}

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : Close
 * Description            : Shutdown the encoder component.
 * Input                  :  
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
BOOL MANGLE(Close)(void)
{
    BOOL result = TRUE;

    free(E);

    return result;
}

/* end of file */
