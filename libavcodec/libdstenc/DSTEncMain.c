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

Source file: DSTEncMain.c (Reference DST Encoder - Main Entrypoint)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/

/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include "conststr.h"
#include "types.h"
#include "dst_init.h"
#include "fio_dsd.h"
#include "dst_fram.h"
#include "DSTEncoder.h"
#include <stdio.h>
#include <string.h>

/*============================================================================*/
/*       MAIN                                                                 */
/*============================================================================*/
int main(int argc, char *argv[])
{
  BOOL              Continue = TRUE;
  BOOL              InitEncOk = TRUE;
  RDFRAME_STATUS    InDataStatus;
  ENCODING_STATUS   EncStatus = DST_NOERROR;
  int               frameSize = 0;
  unsigned char     DSDframeBuf[DSDFRAMESIZE_ALLCH];
  unsigned char     DSTframeBuf[DSDFRAMESIZE_ALLCH+1];
  ULONG             FrameNr = 0;
  CoderOptions      CodOpt;

  CodOpt.MaxPredOrder         = 128;
  CodOpt.NrOfFramesInFile     = 0;

  MANGLE(ReadEncCmdLineParams)(argc, argv, &CodOpt);
  
  /* open inputfile and outputfile */
  Continue = FIO_Open(CodOpt.InFileName,
                      CodOpt.OutFileName,
                      &CodOpt.NrOfChannels,
                      &CodOpt.Fsample44,
                      &CodOpt.NrOfFramesInFile);
  
  if (Continue == TRUE)
  {
    FIO_WriteDSTHeader();
  }

  
  /***************************************************
   * Init Encoder
   ***************************************************/
  if (Continue == TRUE)
  {
    InitEncOk = MANGLE(Init)(CodOpt.NrOfChannels, CodOpt.Fsample44);
    Continue = InitEncOk;
  }

  /***************************************************
   * Encoder main loop                       
   ***************************************************/
  while (Continue == TRUE)
  {
    /***************************************************
     * Read DSD Frame from file
      ***************************************************/
    InDataStatus=FIO_DSDReadFrame(DSDframeBuf);
    if ((NOMOREFRAMES==InDataStatus) || (FRAMEINCOMPLETE==InDataStatus) )
    {
      Continue = FALSE;
    }

    /***************************************************
     * Encode DSD Frame to DST frame
      ***************************************************/
    if (Continue == TRUE)
    {
      EncStatus = MANGLE(Encode)(DSDframeBuf, DSTframeBuf, &frameSize);
      if (EncStatus != DST_NOERROR)
      {
        Continue = FALSE; 
      }
    }

    /***************************************************
     * Write DST frame to file
      ***************************************************/
    if (Continue == TRUE)
    {
      FIO_DSTWriteFrame(DSTframeBuf, frameSize/8); /* framesize in bits */
    }

    /***************************************************
     * Update Frame counter
      ***************************************************/
    FrameNr++;
    if (FrameNr>=CodOpt.NrOfFramesInFile)
    {
      Continue = FALSE;
    }
  }
  /***************************************************
   * End of Encoder main loop                       
   ***************************************************/

  /***************************************************
   * Close Encoder
   ***************************************************/
  if (InitEncOk == TRUE)
  {
    MANGLE(Close)();
  }

  /***************************************************
   * Close output file
   ***************************************************/
  FIO_WriteDSTChunkSizes();
  FIO_Close();

  return(EncStatus);

}

/* end of file */
