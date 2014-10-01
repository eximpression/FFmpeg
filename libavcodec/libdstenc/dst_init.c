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

Source file: DST_Init.c (Initialize encoder environment)

Required libraries: <none>

Authors:
RT:  Richard Theelen, PDSL-labs Eindhoven <r.h.m.theelen@philips.com>

Changes:
08-Mar-2004 RT  Initial version

************************************************************************/


/*============================================================================*/
/*       INCLUDES                                                             */
/*============================================================================*/

#include "types.h"
#include "conststr.h"
#include "dst_init.h"
#include "fio_dsd.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*============================================================================*/
/*       GLOBAL FUNCTION IMPLEMENTATIONS                                      */
/*============================================================================*/

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : DST_UseEncoder
 * Description            : Usermanual
 * Input                  :  
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
static void DST_UseEncoder(void)
{
  fprintf(stdout, "Ref DST Encoder    \n");
  fprintf(stdout, "                   \n");
  fprintf(stdout, "Usage:             \n");
  fprintf(stdout, "  Ref DST Encoder  \n");
  fprintf(stdout, "  -i  'DSDIFF (DSD) input file'  : DSD file to read                       \n");
  fprintf(stdout, "  -o  'DSDIFF (DST) output file' : Lossless coded file to store           \n");
  fprintf(stdout, "  -h                             : print this list\n\n");
  exit(-1);
}

/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : ReadEncCmdLineParams
 * Description            : Read the command line parameters and assign/change values
 *                          to/of key variables accordingly. 
 * Input                  :  
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
void MANGLE(ReadEncCmdLineParams)(int argc, char *argv[], CoderOptions *CO)
{
  int i;

  strcpy(CO->InFileName , "");
  strcpy(CO->OutFileName, "");

  if (argc == 1)
  {
    DST_UseEncoder();
  }
  for(i = 1; i < argc; i++)
  {
    /* parameter -h : help function */
    if (strcmp("-h", argv[i]) == 0)
    {
      DST_UseEncoder();
    }
    /* parameter -i : input file name */
    else if (strcmp("-i", argv[i]) == 0)
    {
      if (i < argc - 1)
      {
        strcpy(CO->InFileName, argv[++i]);
      }
      else
      {
        fprintf(stderr, "ERROR: No value for the DSD input filename was entered!\n\n");
        DST_UseEncoder();
      }
    }
    /* parameter -o : output file name */
    else if (strcmp("-o", argv[i]) == 0)
    {
      if (i < argc - 1)
      {
        strcpy(CO->OutFileName, argv[++i]);
      }
      else
      {
        fprintf(stderr, "ERROR: No value for the output file was entered!\n\n");
        DST_UseEncoder();
      }
    }
  }
}


/*************************GLOBAL FUNCTION**************************************
 * 
 * Name                   : DST_InitEncInitialisation
 * Description            : Complete initialisation of the encoder.
 * Input                  :  
 * Output                 :
 * Pre-condition          :
 * Post-condition         :
 * Returns:               :
 * Global parameter usage :
 * 
 *****************************************************************************/
BOOL MANGLE(DST_InitEncInitialisation)(ebunch* E)
{
  BOOL retval = TRUE;
    
  unsigned int ChNr;
  int FilterNr;
  int PtableNr;

  /* Default initialization of strategy parameters */

  /* ALL DIFFERENT STRATEGY */
  E->NrOfFilters=E->NrOfChannels;
  E->NrOfPtables=E->NrOfChannels;

  for (ChNr=0; ChNr<E->NrOfChannels; ChNr++)
  {
    E->ChannelFilter[ChNr]=ChNr;
    E->ChannelPtable[ChNr]=ChNr;
  }
  
  for (FilterNr=0; FilterNr<E->NrOfFilters; FilterNr++)
  {
    E->PredOrder[FilterNr]=MAXPREDORDER;
  }

  for (PtableNr=0; PtableNr<E->NrOfPtables; PtableNr++)
  {
    E->PtableLen[PtableNr]=MAXPTABLELEN;
  }

  return (retval);
}

/* end of file */

