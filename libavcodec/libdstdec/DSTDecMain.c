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

Source file: DSTDecMain.c (Reference DST Decoder - Main Entrypoint)
                          (for decoding Fs=64FS44, 128FS44, 256FS44)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include <stdio.h>

#include "fio_dst.h"  /* Reading DST */
#include "fio_dsd.h"  /* Writing DSD */

#include "dst_init.h"
#include "DSTDecoder.h"

static BYTE       pDSTdataBuf[64*MAX_DSDBYTES_INFRAME+1];
static BYTE       pDSDdataBuf[64*MAX_DSDBYTES_INFRAME+4];

/*============================================================================*/
/*       MAIN FUNCTION IMPLEMENTATION                                         */
/*============================================================================*/

int main(int argc, char **argv) 
{
  BOOL            Continue  = TRUE;  

  ULONG           frameSize = 0;
  ULONG           FrameNr = 0;
  
  CoderOptions    CodOpt;

  MANGLE(SetDefaults)(&CodOpt);                       /* Set default decoder options   */
  MANGLE(ReadDecCmdLineParams)(argc, argv, &CodOpt);  /* read command line parameters  */
  MANGLE(CheckDecParams)(&CodOpt);                    /* check command line parameters */

  /*==================================================
   * Open files
   *==================================================*/
  if (Continue == TRUE)
  {
    /* Open input file for reading */
    /* ----------------------------*/
    FIO_DSTOpenRead(CodOpt.InFileName, &CodOpt.NrOfChannels, &CodOpt.Fsample44, &CodOpt.NrOfFramesInFile);
  }

  if (Continue == TRUE)
  {
    /* Open output file for writing */
    /* -----------------------------*/
    FIO_DSDOpenWrite(CodOpt.OutFileName, CodOpt.NrOfChannels, CodOpt.Fsample44);
  }  

  /*==================================================
   * Init Decoder
   *==================================================*/
  if (Continue==TRUE) 
  {
    Continue = MANGLE(Init)(CodOpt.NrOfChannels, CodOpt.Fsample44);
  }

  /*==================================================
   * Decoder main loop
   *==================================================*/

  /* Read the DST input file on a frame by frame basis. 
   * FIO_DSTReadFrame returns the framelength of the frame read.
   * In case that is 0, there are no more frames to come. 
   */
  while (Continue == TRUE) 
  {
    /*==================================================
     * Read DST Frame from file
     *==================================================*/
    if (FIO_DSTReadFrame( pDSTdataBuf,         /* DST data frame     */
                          &frameSize) == EOF)  /* FrameSize in bytes */
    {
      
    }

    /*==================================================
     * Decode DST Frame to DSD frame
     *==================================================*/
    if (MANGLE(Decode)( pDSTdataBuf, 
                pDSDdataBuf, 
                FrameNr,
                &frameSize ) == TRUE)
    {
    }

    /*==================================================
     * Write decoded DSD frame to file
     *==================================================*/
    FIO_DSDWriteFrame(pDSDdataBuf);
    
    /*==================================================
     * Update Frame Counter
     *==================================================*/
    FrameNr++;
    if ( (FrameNr>=(ULONG)CodOpt.NrOfFramesInFile) && (CodOpt.NrOfFramesInFile != 0) )
    {
      Continue = FALSE;
    }

  }
  /*==================================================
   * End of Decoder main loop
   *==================================================*/

  /*==================================================
   * Close Decoder
   *==================================================*/
  MANGLE(Close)();

  /*==================================================
   * Close Files
   *==================================================*/
  FIO_DSTCloseRead();   /* close input file  */
  FIO_DSDCloseWrite();  /* close output file */

  return 0;
}

