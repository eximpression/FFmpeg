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

Source file: FIO_DSD.c (File I/O DSD stream)

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
static int isIDValid(char *ckID);
static int isIDEqual (char *ckID1, char *ckID2,int size);
static int setfpBefore(char *ckIDIn, FILE *fp, int IDSize,int *numRead);
static int sizeOfDataPortion_32bit (BYTE* headerInfo);

static BYTE*  m_DSTHeader;
static int    m_headerSize        = 0;
static int    m_DSTsoundDataBytes = 0;
static int    m_framesWritten     = 0;

static FILE*  m_DSTFp       = NULL;
static FILE*  m_DSDFp       = NULL;
static FILE*  m_StrategyFp  = NULL;

static int    m_MaxFrameLen   = 0;
static int    m_NrOfChannels  = 1; 
static int    m_NrOfFrames    = 0;
static int    m_Fsample44     = 64;

/*============================================================================*/
/*       GLOBAL FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/
static void writeDataBeforeDSTchunk(FILE *fp);
static void writeDataSizeInChunk   (char *ckID,int ckIDsize,int numBytes,FILE *fp);
static void writeCompressionType   (FILE *fp);


/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_retrieveFsample44
 * Description            : .
 * Input                  :  
 * Output                 :
 * Pre-condition          : m_DSDFp is not NULL (FIO_Open is called)
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
  if (setfpBefore("FS  ",m_DSDFp,4,&dummy)) 
  {
    fread(ckId_ckDataSize,sizeof(ckId_ckDataSize),1,m_DSDFp);
    fread(fsample,sizeof(fsample),1,m_DSDFp);

    /* sample frequency of input signal */
    m_Fsample44 = ( ((int)fsample[0] << 24) + 
                    ((int)fsample[1] << 16) +
                    ((int)fsample[2] <<  8) + 
                    ((int)fsample[3] <<  0) ) / 44100;

    /* max frame len depending on input sample frequency */
    m_MaxFrameLen = 588 * m_Fsample44 / 8;
  } 
  else 
  {
    printf("Input file %p not DSDIFF format\n",m_DSDFp);
    exit(1);
  }
  return(m_Fsample44);
}


/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_retrieveNumberOfChannels
 * Description            : .
 * Input                  :  
 * Output                 :
 * Pre-condition          : m_DSDFp is not NULL (FIO_Open is called)
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
  if (setfpBefore("CHNL",m_DSDFp,4,&dummy)) 
  {
    fread(ckId_ckDataSize,sizeof(ckId_ckDataSize),1,m_DSDFp);
    fread(numChannels,sizeof(numChannels),1,m_DSDFp);
    m_NrOfChannels = (int)numChannels[1];
  } 
  else 
  {
    printf("Input file %p not DSDIFF format\n",m_DSDFp);
    exit(1);
  }
  return(m_NrOfChannels);
}

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_DSDCheckFileAndSkip
 * Description            : Check file format.
 * Input                  :  
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
int FIO_DSDCheckFileAndSkip(ULONG* TotFr)
{
  int size, bytesPerFrame;
  BYTE ckId_ckDataSize[12];
  int numRead, i, mask;
  int addHeaderBytes = 8 + 18;

  /* Create Copy header for re-use when writing the DST file */
  if (setfpBefore("DSD ",m_DSDFp,4,&numRead)) 
  {
    
    /* header:
     * Form DSD chunk ... DST sound data chunk with FRTE]
     * [0 ...                               m_headerSize] */
    m_headerSize = numRead + addHeaderBytes;
    /* create re-use header: */
    m_DSTHeader = (BYTE*)malloc(m_headerSize);
    memset(m_DSTHeader,0x00,m_headerSize);
    rewind(m_DSDFp);
    fread(m_DSTHeader,m_headerSize,1,m_DSDFp);

    m_DSTHeader[m_headerSize-(addHeaderBytes)-2]   = 'T'; /* DST instead of DSD */

    /* DST Frame Information Chunk */
    m_DSTHeader[m_headerSize-(addHeaderBytes)+8]   = 'F';
    m_DSTHeader[m_headerSize-(addHeaderBytes)+9]   = 'R';
    m_DSTHeader[m_headerSize-(addHeaderBytes)+10]  = 'T';
    m_DSTHeader[m_headerSize-(addHeaderBytes)+11]  = 'E';

    /* frameRate = 75 */
    for (i=0, mask=255;i<2;i++) 
    {
      m_DSTHeader[m_headerSize-(addHeaderBytes)+25-i] = (BYTE)((75 & mask) >> i*8);
      mask <<= 8;
    }

  } 
  else 
  {
    fprintf(stderr, "ERROR:Input file not DSDIFF format\n");
    exit(1);
  }
  /* Set file pointer just before DSD data. */
  if (setfpBefore("DSD ",m_DSDFp,4,&numRead)) 
  {
    fread(ckId_ckDataSize,sizeof(ckId_ckDataSize),1,m_DSDFp);
    /* DSD Sound Data Chunk content:
     *   ID             ckID            4 bytes
     *   double ulong   ckDataSize      8 bytes
     *   uchar          DSDsoundData[]  ckDataSize bytes

     * This version only supports files with size within 32 bit range!
     * Data above 32 bit size is not used.
     */
    size = sizeOfDataPortion_32bit(ckId_ckDataSize);
    bytesPerFrame = m_MaxFrameLen * m_NrOfChannels;

    /* return parameters */
    *TotFr = (size / bytesPerFrame);

    /* numFrames */
    m_NrOfFrames = *TotFr;
    for (i=0, mask=255;i<4;i++) 
    {
      m_DSTHeader[m_headerSize-(addHeaderBytes)+23-i] = (BYTE)((m_NrOfFrames & mask) >> i*8);
      mask <<= 8;
    }
  } 
  else 
  {
    fprintf(stderr, "ERROR:Input file not DSDIFF format\n");
    exit(1);
  }

  return (0);
}

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_DSDReadFrame
 * Description            : Read a frame from the DSD input file
 * Input                  :  
 * Output                 :
 * Pre-condition          : m_DSDFp is not NULL (FIO_Open is called)
 * Post-condition         :
 * Returns:               : FRAMEINCOMPLETE if not enough bytes available
 *              NOMOREFRAMES    if no more bytes are available
 *              NOFRAMEERROR    if OK
 *
 * Global parameter usage :
 * 
 *****************************************************************************/
RDFRAME_STATUS FIO_DSDReadFrame (  unsigned char *DF) 
{
  int BytesRead, BR1, BR2;
  int i;
  int StatusValue=NOFRAMEERROR;
  int IdleBytes;
  int rsize = (m_MaxFrameLen/2)*m_NrOfChannels;
  BR1 = (int)fread(DF       , 1, rsize, m_DSDFp);
  BR2 = (int)fread(DF+rsize , 1, rsize, m_DSDFp);
  BytesRead = BR1+BR2;
  if (BytesRead != 0) 
  {
    if (BytesRead != m_MaxFrameLen * m_NrOfChannels) 
    {
      IdleBytes = m_MaxFrameLen * m_NrOfChannels - BytesRead;
      for (i = 0; i < IdleBytes; i++) 
      {
        DF[BytesRead + i] = 0x55;
      }
      StatusValue=FRAMEINCOMPLETE;
    } 
  } 
  else 
  {
    StatusValue=NOMOREFRAMES;
  }
  return (StatusValue); 
}


/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_DSTWriteFrame
 * Description            : Write one DST frame Data Chunk to file.
 * Input                  : EncodedFrameLen in bytes!
 * Output                 :
 * Pre-condition          : DSTFp is not NULL (FIO_Open is called)
 * Post-condition         :
 * Returns:               :
 *
 * Note                   : No errorchecking on write operations
 *                          ex: free disc space availability.
 * Global parameter usage :
 * 
 *****************************************************************************/
void FIO_DSTWriteFrame (unsigned char *EncodedFrame,
                                  int EncodedFrameLen) 
{
  BYTE dummy[12];
  int frameSize = EncodedFrameLen, mask, i;

  dummy[0]='D';
  dummy[1]='S';
  dummy[2]='T';
  dummy[3]='F';
  for (i=0, mask=255;i<4;i++) 
  {
    dummy[11-i] = (BYTE)((frameSize & mask) >> i*8);
    mask <<= 8;
  }
  dummy[7]=dummy[6]=dummy[5]=dummy[4]=0;
  fwrite(dummy,sizeof(BYTE), 12, m_DSTFp);
  fwrite(EncodedFrame, frameSize, 1, m_DSTFp);

  if (frameSize%2) 
  {
    fputc(0,m_DSTFp);
    frameSize++;
  }
  m_DSTsoundDataBytes += frameSize;
  m_framesWritten++;

}

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_WriteDSTHeader
 * Description            : Write file header of DST file.
 * Input                  :  
 * Output                 :
 * Pre-condition          : DSTFp is not NULL (FIO_Open is called)
 * Post-condition         :
 * Returns:               :
 *
 * Global parameter usage :
 * 
 *****************************************************************************/
void FIO_WriteDSTHeader (void) 
{
  writeDataBeforeDSTchunk(m_DSTFp);
}

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_WriteDSTChunkSizes
 * Description            : Calculate and write all chunk sizes left open.
 * Input                  :  
 * Output                 :
 * Pre-condition          : DSTFp is not NULL (FIO_Open is called)
 * Post-condition         :
 * Returns:               :
 *
 * Global parameter usage :
 * 
 *****************************************************************************/
void FIO_WriteDSTChunkSizes (void) 
{
  /* FRTE size */
  /* = sizeof(numFrames + FrameRate) (= 6) */
  writeDataSizeInChunk ("FRTE",4,6,m_DSTFp);

  /* DST size (container chunk)     */
  /* = sizeof(FRTE and DSTF chunks) */
  writeDataSizeInChunk ("DST ",4,18+m_DSTsoundDataBytes+m_framesWritten*12,m_DSTFp);

  /* FRM8 size */
  writeDataSizeInChunk ("FRM8",4,m_headerSize-12+m_DSTsoundDataBytes+(m_framesWritten*12),m_DSTFp);

  /* File must contain even number of bytes. */
  if (m_DSTsoundDataBytes%2) 
  {
    /* write pad byte */
    fflush(m_DSTFp);
    fseek(m_DSTFp,0,SEEK_END);
    fputc(0,m_DSTFp);
  }
  writeCompressionType(m_DSTFp);
}

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_Open
 * Description            : Open input and output files.
 * Input                  : FileIn (DSD filename) and FileOut (DST filename). 
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               : TRUE if open is ok, else FALSE.
 * Global parameter usage :
 * 
 *****************************************************************************/
BOOL FIO_Open (char*  FileIn,
               char*  FileOut,
               ULONG* nrOfChannels,
               ULONG* Fsample44,
               ULONG* nrOfFramesInFile)
{
  BOOL result = TRUE;

  *nrOfFramesInFile = 0;

  /* Open input file */
  if (strcmp(FileIn, "") != 0)
  {
    if ((m_DSDFp = fopen(FileIn,"rb")) == NULL)
    {
      fprintf(stderr, "\nError opening %s\n", FileIn);
      result = FALSE;
      exit(1);
    }
    else
    {
      fprintf(stdout, "Input file name    : %s\n", FileIn);
    }
  }
  /* Open output file */
  if (strcmp(FileOut, "") != 0)
  {
    if ((m_DSTFp = fopen(FileOut,"wb+")) == NULL)
    {
      fprintf(stderr, "\nError opening %s\n", FileOut);
      result = FALSE;
      exit(1);
    }
    else
    {
      fprintf(stdout, "Output file name   : %s\n", FileOut);
    }
  }

  if (result == TRUE)
  {
    *nrOfChannels = FIO_retrieveNumberOfChannels();
  }
  fprintf(stdout, "Number of channels : %d\n", *nrOfChannels);

  if (result == TRUE)
  {
    *Fsample44    = FIO_retrieveFsample44();
  }
  if ( (*Fsample44 == 64) || (*Fsample44 == 128) || (*Fsample44 == 256) )
  {
    fprintf(stdout, "Sample frequency   : %d (%dxFS44)\n", *Fsample44 * 44100, *Fsample44);
  }
  else
  {
    fprintf(stdout, "Sample frequency   : %d (invalid!)\n", *Fsample44 * 44100);
  }

  FIO_DSDCheckFileAndSkip(nrOfFramesInFile);

  return result;
}

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : FIO_Close
 * Description            : Close input and output files.
 * Input                  : FileIn (DSD filename) and FileOut (DST filename). 
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
void FIO_Close (void)
{
  if (m_DSDFp != NULL)
  {
    fclose(m_DSDFp);
  }
  if (m_DSTFp != NULL)
  {
    fclose(m_DSTFp);
  }
  if (m_StrategyFp != NULL)
  {
    fclose(m_StrategyFp);
  }
}

/*============================================================================*/
/*       STATIC FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/
/* Is this an existing ckID */
static int isIDValid(char *ckID) 
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
    isIDEqual ("DST ",ckID,4) ||
    isIDEqual ("LSCO",ckID,4)
  );
}

/* pre: current file pointer right before chunkID. */
static int setfpBefore(char *ckIDIn, FILE *fp, int IDSize, int *numRead)
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
static int isIDEqual (char *ckID1, char *ckID2,int size) 
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

/* Determine size written in header info. */
static int sizeOfDataPortion_32bit (BYTE* headerInfo) 
{
  /* 32 bit version */
  int i, res = 0;

  for (i=0;i<4;i++) res += (headerInfo[11-i] << i*8);

  return res;
}

/* Write DSDIFF file */
static void writeDataBeforeDSTchunk (FILE *fp) 
{
  fwrite(m_DSTHeader,m_headerSize,1,fp);
  free(m_DSTHeader);
}

/* Write datasize value numBytes in chunk specified with ckID */
static void writeDataSizeInChunk (char *ckID,int ckIDsize,int numBytes,FILE *fp) 
{
  /* 32 bit version */
  int numRead, mask = 255, i;
  BYTE ckDataSize[12];

  fflush(fp);

  if (setfpBefore(ckID,fp,ckIDsize,&numRead)) 
  {
    /* copy old data size */
    fread(&ckDataSize,sizeof(BYTE),12,fp);
    fflush(fp);
    /* write new data size */
    setfpBefore(ckID,fp,ckIDsize,&numRead);

    for (i=0;i<4;i++) 
    {
      ckDataSize[11-i] = (BYTE)((numBytes & mask) >> i*8);
      mask <<= 8;
    }
    /* write zeroes above 32 bit */
    for (i=4;i<8;i++)
    {
      ckDataSize[11-i] = 0;
    }
    fwrite(ckDataSize,sizeof(BYTE),12,fp);
    fflush(fp);
  }
}

/* Write compression type in CMPR chunk. */
static void writeCompressionType (FILE *fp) 
{
  int numRead;
  BYTE dummy[31];

  fflush(fp);

  if (setfpBefore("CMPR",fp,4,&numRead)) 
  {
    /* copy old data */
    fread(&dummy,sizeof(BYTE),31,fp);
    fflush(fp);
    /* write new data */
    dummy[14] = 'T';
    memcpy(dummy+17,"DST Encoded   ", 14);
    setfpBefore("CMPR",fp,4,&numRead);
    fwrite(dummy,sizeof(BYTE),31,fp);
    fflush(fp);
  }
}

/* end of file */


