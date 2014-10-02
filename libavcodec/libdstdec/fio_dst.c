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

Source file: FIO_DST.c (File I/O DST stream - read)

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
#include <stdlib.h>
#include <string.h>
#include "fio_dst.h"
#include "dst_ac.h"
#include "types.h"

/*============================================================================*/
/*       STATIC VARIABLES                                                     */
/*============================================================================*/

int isIDValid(char *ckID);
int isIDEqual (char *ckID1, char *ckID2,int size);
int setfpBefore(char *ckIDIn, FILE *fp, int IDSize,int *numRead);
int sizeOfDataPortion_32bit (BYTE* headerInfo);
int GetNumberFP(int *map, int chcnt);

static FILE*  m_DSTFp = NULL;

static int    m_NrOfChannels; 
static int    m_Fsample44;
static ULONG  m_NrOfFramesInFile;
static USHORT m_frameRate;


/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_retrieveNumberOfChannels
 * Description            : .
 * Input                  :  
 * Output                 :
 * Pre-condition          : DSDFp is not NULL (FIO_Open is called)
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
int FIO_retrieveNumberOfChannels (void)
{
  BYTE ckId_ckDataSize[12];
  BYTE numChannels[2];
  int dummy;

  m_NrOfChannels = 0;

  /* Set file pointer just before channel data */
  if (setfpBefore("CHNL",m_DSTFp,4,&dummy)) 
  {
    fread(ckId_ckDataSize,sizeof(ckId_ckDataSize),1,m_DSTFp);
    fread(numChannels,sizeof(numChannels),1,m_DSTFp);
    m_NrOfChannels = (int)numChannels[1];
  } 
  else 
  {
    printf("Input file %p not DSDIFF format\n",m_DSTFp);
    exit(1);
  }
  return(m_NrOfChannels);
}


/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_retrieveFsample44
 * Description            : .
 * Input                  :  
 * Output                 :
 * Pre-condition          : DSDFp is not NULL (FIO_Open is called)
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
int FIO_retrieveFsample44(void)
{
  BYTE ckId_ckDataSize[12];
  BYTE fsample[4];
  int dummy;

  m_Fsample44 = 0;

  /* Set file pointer just before channel data */
  if (setfpBefore("FS  ",m_DSTFp,4,&dummy)) 
  {
    dummy = fread(ckId_ckDataSize,sizeof(ckId_ckDataSize),1,m_DSTFp);
    dummy = fread(fsample,sizeof(fsample),1,m_DSTFp);
    m_Fsample44 = ( ((int)fsample[0] << 24) + 
                    ((int)fsample[1] << 16) +
                    ((int)fsample[2] <<  8) + 
                    ((int)fsample[3] <<  0) ) / 44100;
  } 
  else 
  {
    printf("Input file %p not DSDIFF format\n",m_DSTFp);
    exit(1);
  }
  return(m_Fsample44);
}


/***************************************************************************/
/*                                                                         */
/* name     : FIO_DSTOpenRead                                              */
/*                                                                         */
/***************************************************************************/
int FIO_DSTOpenRead(char* InFileName, int* nrOfChannels, int* Fsample44, unsigned int* nrFramesInFile)
{
  int   retval = 0;
  BYTE  ckDataSize[8];
  BYTE  ckNrFr[4];
  char  ckID[4];
  int   dummy;

  /* open input file for reading  */
  /* ---------------------------- */
  if ((m_DSTFp = fopen(InFileName, "rb")) == NULL)
  {
    fprintf(stdout, "\nError opening inputfile: %s\n", InFileName);
    retval = 1;
    exit(1);
  }
  else
  {
    fprintf(stdout, "Input filename     : %s\n", InFileName);
  }

  *Fsample44    = FIO_retrieveFsample44();
  fprintf(stdout, "Samplefrequency    : %d (%dxFS44)\n", *Fsample44 * 44100, *Fsample44);
  *nrOfChannels = FIO_retrieveNumberOfChannels();
  fprintf(stdout, "Number of channels : %d\n", *nrOfChannels);

  /* Set file pointer just before 'FRTE' (DST Frame Information Chunk) */
  if (setfpBefore("FRTE",m_DSTFp,4,&dummy)) 
  {
    fread(ckID,sizeof(ckID),1,m_DSTFp);
    fread(ckDataSize,sizeof(ckDataSize),1,m_DSTFp);
    fread(&ckNrFr,sizeof(ckNrFr),1,m_DSTFp);
    m_NrOfFramesInFile = ((ULONG)ckNrFr[3]      ) +
                         ((ULONG)ckNrFr[2] << 8 ) +
                         ((ULONG)ckNrFr[1] << 16) +
                         ((ULONG)ckNrFr[0] << 24);
    fread(&m_frameRate ,sizeof(m_frameRate ),1,m_DSTFp);
    m_frameRate = (USHORT)(((m_frameRate & 0xFF00) >> 8) + ((m_frameRate & 0x00FF) << 8));
    
    *nrFramesInFile = m_NrOfFramesInFile; /* return number of frames in file */
  } 
  else 
  {
    fprintf(stderr, "ERROR:Input file not DSDIFF format\n");
    exit(1);
  }

  return retval;
}


/***************************************************************************/
/*                                                                         */
/* name        : FIO_DSTReadFrame                                          */
/* description : Reads DST Frame from DSDIFF File                          */
/*                                                                         */
/***************************************************************************/
int FIO_DSTReadFrame(BYTE*  DSTFrameData,
                     ULONG* DSTframeSize)
{
  int     retval = 0;
  char    ckID[4];
  BYTE    ckDataSize[8];
  BYTE    padbyte;
  fpos_t  oldPos;

  ULONG   CRCsize;
  BYTE    CRCdata[4];
  
  /* Check if there if 'DSTF' chunk  */
  /* ------------------------------- */
  fread(ckID,sizeof(ckID),1,m_DSTFp); /* 'DSTF' [4] */
  if (isIDEqual ("DSTF",ckID,4))
  {
    /* read DST frame size */
    fread(ckDataSize,sizeof(ckDataSize),1,m_DSTFp);
    *DSTframeSize = ((ULONG)ckDataSize[7]      ) +
                    ((ULONG)ckDataSize[6] <<  8) +
                    ((ULONG)ckDataSize[5] << 16) +
                    ((ULONG)ckDataSize[4] << 24);
    /* read DST data frame */
    fread(DSTFrameData,*DSTframeSize,1,m_DSTFp); /* ckDataSize [8] */

    /* read pad-byte if needed */
    if (((*DSTframeSize) % 2) != 0)
    {
      fread(&padbyte, 1, 1, m_DSTFp);
    }
  }
  else
  {
    /* no DST frame found */
    retval = 1;
  }

  /* Check if there if 'DSTC' chunk */
  /* ------------------------------ */
  fgetpos(m_DSTFp,&oldPos);           /* get current file pointer */
  fread(ckID,sizeof(ckID),1,m_DSTFp); /* 'DSTC' [4] */
  if (isIDEqual ("DSTC",ckID,4))
  {
    /* CRC data found */
    /* read CRC size  */
    fread(ckDataSize,sizeof(ckDataSize),1,m_DSTFp);
    CRCsize = ((ULONG)ckDataSize[7]      ) +
              ((ULONG)ckDataSize[6] <<  8) +
              ((ULONG)ckDataSize[5] << 16) +
              ((ULONG)ckDataSize[4] << 24);
    
    /* clear CRCdata */
    memset(CRCdata, CRCsize, 1);

    /* read CRC data */
    fread(CRCdata, CRCsize,1,m_DSTFp);

    /* read pad-byte if needed */
    if (((CRCsize) % 2) != 0)
    {
      fread(&padbyte, 1, 1, m_DSTFp);
    }
  }
  else
  {
    /* rewind file pointer 4 positions */
    fsetpos(m_DSTFp,&oldPos);
  }

  return retval;
}


/***************************************************************************/
/*                                                                         */
/* name        : FIO_DSTCloseRead                                          */
/* description : Close DSDIFF File                                         */
/*                                                                         */
/***************************************************************************/
int FIO_DSTCloseRead()
{
  int retval = 0;
  /* close file */
  if (m_DSTFp != NULL)
  {
    if (fclose(m_DSTFp) != 0)
    {
      fprintf(stderr,"Error closing DSDIFF(DST) input file!\n");
      retval = 1;
    }
  }

  return retval;
}



/***************************************************************************/
/*                                                                         */
/* name        : isIDValid                                                 */
/* description : Is this an existing ckID                                  */
/*                                                                         */
/***************************************************************************/
int isIDValid(char *ckID) 
{
  return (
    isIDEqual ("FRM8",ckID,4) ||
    isIDEqual ("FVER",ckID,4) ||
    isIDEqual ("PROP",ckID,4) ||
    isIDEqual ("COMT",ckID,4) ||
    isIDEqual ("DSD ",ckID,4) ||
    isIDEqual ("FS  ",ckID,4) ||
    isIDEqual ("CHNL",ckID,4) ||
    isIDEqual ("CMPR",ckID,4) ||
    isIDEqual ("ABSS",ckID,4) ||
    isIDEqual ("FRTE",ckID,4) ||
    isIDEqual ("DSTF",ckID,4) ||
    isIDEqual ("DSTC",ckID,4) ||
    isIDEqual ("DST ",ckID,4) ||
    isIDEqual ("LSCO",ckID,4)
  );
}

/* pre: current file pointer right before chunkID. */
int setfpBefore(char *ckIDIn, FILE *fp, int IDSize, int *numRead)
{
  char ckID[4];
	BYTE ckDataSize[8],dummyByte[322];
  int skipBytes;
  fpos_t oldPos;

  rewind(fp);
    
  *numRead = 0;

  while (!feof(fp)) 
  {
    fgetpos(fp,&oldPos);
    *numRead += 4*(fread(ckID,sizeof(ckID),1,fp));
    if (isIDValid(ckID)) 
    {
      if (isIDEqual(ckIDIn,ckID,IDSize)) 
      {
        /* We located the requested chunk.               */
        /* Set file pointer right before requested chunk */
        fsetpos(fp,&oldPos);
        return 1;
      } 
      else 
      {
        if ( !(isIDEqual("FRM8",ckID,4) ||  isIDEqual("PROP",ckID,4) || isIDEqual("DST ",ckID,4)) ) 
        {
          /* Set file pointer right before NEXT chunk */
          *numRead += 8*(fread(ckDataSize,sizeof(ckDataSize),1,fp));
          skipBytes = 256 *((int) ckDataSize[6]) + (int) ckDataSize[7];
          *numRead += fread(&dummyByte,sizeof(char),skipBytes,fp);
        } 
        else 
        {
          /* Container chunk */
          if (isIDEqual("DST ",ckID,4))
          {
            *numRead += fread(&dummyByte,sizeof(char),8,fp);
          } 
          else
          {
            *numRead += fread(&dummyByte,sizeof(char),12,fp);
          }
        }
      }
    } 
    else 
    {
      return 0;
    }
  }
  return 0;
}

/* Are ckID1 and ckID2 equal? Returns true if they are. */
int isIDEqual (char *ckID1, char *ckID2,int size) 
{
  switch (size) 
  {
    case 4:
      return (
        ckID1[0] == ckID2[0] &&
        ckID1[1] == ckID2[1] &&
        ckID1[2] == ckID2[2] &&
        ckID1[3] == ckID2[3] 
      );
      break;
    default:
      return 0;
  }
}

