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

Source file: FIO_DSD.c (File I/O DSD stream - write)

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
#include "fio_dsd.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================*/
/*       PROTOTYPES STATIC FUNCTIONS                                          */
/*============================================================================*/

static int    m_framesWritten = 0;

static FILE*  m_DSDFp         = NULL;

static int    m_headerSize    = 0;
static int    m_NrOfChannels  = 1; 
static int    m_Fsample44     = 64;

/*============================================================================*/
/*       GLOBAL FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/


/***************************************************************************/
/*                                                                         */
/* name     : FIO_DSDOpenWrite                                             */
/*                                                                         */
/***************************************************************************/
int FIO_DSDOpenWrite(char* OutFileName, int NrOfChannels, int Fsample44)
{
  int     retval = 0;
  BYTE*   DSTHeader;

  /* open output file for writing */
  /* ---------------------------- */
  if ((m_DSDFp = fopen(OutFileName, "wb")) == NULL)
  {
    fprintf(stdout, "\nError opening outputfile: %s\n", OutFileName);
    retval = 1;
    exit(1);
  }
  else
  {
    fprintf(stdout, "Output filename    : %s\n", OutFileName);
  }

  m_NrOfChannels  = NrOfChannels;
  m_Fsample44     = Fsample44;

  /*
   *  write DSDIFF(DSD) header
   *  ----------------------------
   *  Create DST Header depending on number of channels
   *  'FRM8'    4 + 8 + 4              = 16
   *  'FVER'    4 + 8 + 4              = 16
   *  'PROP'    4 + 8 + 4              = 16
   *    'FS  '  4 + 8 + 4              = 16
   *    'CHNL'  4 + 8 + 2 + 4*#ch      = 14 + 4*#ch
   *    'CMPR'  4 + 8 + 4 + 1 + 14 + 1 = 32  ('not compressed'=14)
   *  'DSD '    4 + 8                  = 12
   */
  m_headerSize = 122 + 4*NrOfChannels;

  DSTHeader = (BYTE*)malloc(m_headerSize);
  memset(DSTHeader,0x00,m_headerSize);
  /* 'FRM8' = 12 + 4 + size */
  memcpy(DSTHeader +  0,"FRM8", 4);
  memcpy(DSTHeader + 12,"DSD ", 4);
  /* 'FVER' = 12 + 4 = 16 */
  memcpy(DSTHeader + 16,"FVER", 4);
  memset(DSTHeader + 27, 0x04 , 1); /* size of 'FVER' */
  memset(DSTHeader + 28, 0x01 , 1); /* version number */
  memset(DSTHeader + 29, 0x04 , 1);
  memset(DSTHeader + 30, 0x00 , 1);
  memset(DSTHeader + 31, 0x00 , 1);
  /* 'PROP' = 12 + (16 + 14 + 4*#ch + 36) = 42 + 4*#ch */
  memcpy(DSTHeader + 32,"PROP", 4);
{
  int prop_size = 66 + 4*m_NrOfChannels;
  memset(DSTHeader + 42, (prop_size >> 8), 1);
  memset(DSTHeader + 43, (prop_size & 0xff), 1);
}
  memcpy(DSTHeader + 44,"SND ", 4);
  /* 'FS  ' = 12 + 4 = 16 */
  memcpy(DSTHeader + 48,"FS  ", 4);
  memset(DSTHeader + 59, 0x04 , 1); /* size of 'FS  ' = 4 */
  /* m_Fsample44
   * sampleRate =  64FS44 =  2822400 = 0x002B1100
   * sampleRate = 128FS44 =  5644800 = 0x00562200
   * sampleRate = 256FS44 = 11289600 = 0x00AC4400
   */
  memset(DSTHeader + 60, 0x00 , 1); 
  memset(DSTHeader + 63, 0x00 , 1);
  switch (m_Fsample44)
  {
    case 64:  memset(DSTHeader + 61, 0x2B , 1);
              memset(DSTHeader + 62, 0x11 , 1); break;
    case 128: memset(DSTHeader + 61, 0x56 , 1);
              memset(DSTHeader + 62, 0x22 , 1); break;
    case 256: memset(DSTHeader + 61, 0xAC , 1);
              memset(DSTHeader + 62, 0x44 , 1); break;
    default: retval = 1;break;
  }
  /* 'CHNL' = 12 + 2 + 4*#ch = 14 + 4*#ch */
  memcpy(DSTHeader + 64,"CHNL", 4);
{
  int chnl_size = 2 + 4*m_NrOfChannels;
  memset(DSTHeader + 74, (chnl_size >> 8), 1);
  memset(DSTHeader + 75, (chnl_size & 0xff), 1);
}
  memset(DSTHeader + 77, m_NrOfChannels , 1);
  switch(NrOfChannels)
  {
    case 2: memcpy(DSTHeader + 78,"SLFTSRGT", 4*2);
            break;
    case 5: memcpy(DSTHeader + 78,"MLFTMRGTC   LS  RS  ", 4*5);
            break;
    case 6: memcpy(DSTHeader + 78,"MLFTMRGTC   LFE LS  RS  ", 4*6);
            break;
    default:
            {
                int ch;
                BYTE ChannelIdentifier[5];

                for (ch = 0; ch < m_NrOfChannels; ch++)
                {
                    sprintf (ChannelIdentifier, "C%03d", ch+1);
                    memcpy(DSTHeader + 78 + 4*ch, ChannelIdentifier, 4);
                }
            }
            break;
  }
  /* 'CMPR' = 12 + 4 + 4 + 1 + 14 + 1 = 36 */
  memcpy(DSTHeader +  78 + 4*NrOfChannels,"CMPR", 4);
  memset(DSTHeader +  89 + 4*NrOfChannels, 20   , 1); /* size of 'CMPR' = 4 + 1 + 14 + 1 = 20 */
  memcpy(DSTHeader +  90 + 4*NrOfChannels,"DSD ", 4);
  memset(DSTHeader +  94 + 4*NrOfChannels, 14  , 1);
  memcpy(DSTHeader +  95 + 4*NrOfChannels,"not compressed", 14); /* + 1 pad-byte */
  /* 'DSD ' = 12 + size */
  memcpy(DSTHeader + 110 + 4*NrOfChannels,"DSD ", 4);

  fwrite(DSTHeader,m_headerSize,1,m_DSDFp);
  free(DSTHeader);

  return (retval);
}


/**************************************************************************
 *
 * name        : FIO_DSDWriteFrame
 * description : Writes DSD Frame to DSDIFF File
 *
 **************************************************************************/
void FIO_DSDWriteFrame(unsigned char *DF)
{
  (void)fwrite(DF, 1, (588 * m_Fsample44 / 8)*m_NrOfChannels, m_DSDFp);
  m_framesWritten++;
}


/**************************************************************************
 *
 * name        : FIO_DSDCloseWrite
 * description : Close DSDIFF File
 *
 **************************************************************************/
int FIO_DSDCloseWrite()
{
  int      retval = 0;
  BYTE    i;
  BYTE    ckDataSize[4];
  ULONG   mask;
  ULONG   DSDSoundDataChunk = m_framesWritten * m_NrOfChannels * (588 * m_Fsample44 / 8);
  ULONG   formsize = (m_headerSize-12) + DSDSoundDataChunk;

  /* write size of Form DSD Chunk 'FRM8' into DSDIFF(DSD) File */
  memset(ckDataSize,0x00,4);
  rewind(m_DSDFp);
  if (!feof(m_DSDFp)) 
  {
    /* Write 'FRM8'-chunk size */
    /* ----------------------- */
    mask = 0xFF;
    for (i=0;i<4;i++) 
    {
      ckDataSize[3-i] = (BYTE)((formsize & mask) >> (i*8));
      mask <<= 8;
    }
    fseek(m_DSDFp,8,SEEK_SET);
    fwrite(ckDataSize,sizeof(BYTE),4,m_DSDFp);
    fflush(m_DSDFp);

    /* Write 'DSD '-chunk size */
    /* ----------------------- */
    mask = 0xFF;
    for (i=0;i<4;i++) 
    {
      ckDataSize[3-i] = (BYTE)((DSDSoundDataChunk & mask) >> (i*8));
      mask <<= 8;
    }
    fseek(m_DSDFp,m_headerSize-4,SEEK_SET);
    fwrite(ckDataSize,sizeof(BYTE),4,m_DSDFp);
    fflush(m_DSDFp);

    /* File must contain even number of bytes. */
    if (DSDSoundDataChunk%2) 
    {
      /* write pad byte */
      fflush(m_DSDFp);
      fseek(m_DSDFp,0,SEEK_END);
      fputc(0,m_DSDFp);
    }
  }
  /* close file */
  if (m_DSDFp != NULL)
  {
    if (fclose(m_DSDFp) != 0)
    {
      fprintf(stderr,"Error closing DSDIFF(DSD) output file!\n");
      retval = 1;
    }
  }

  return (retval);
}
