#####################################################################
### tmake project file for the MPEG DST encoder                   ###
#####################################################################

## $Id: AUW_test.pro,v 1.3.2.6 2004/02/16 12:51:16 rijnberg Exp $

TARGET                  = RefDstDecoder

TEMPLATE                = app
win32-msvc:TEMPLATE     = vcapp

win32:CONFIG            = console warn_on debug
unix:CONFIG             = warn_on debug

#####################################################################
## Definition of directories, relative to the current directory    ##
#####################################################################
## Directory where complete software source tree is located
SRC                     = .

## Add here the included header file paths of other modules 
#INCLUDEPATH            += $$SRC/import

## Add here the directory where used libraries are located
win32:LIBDIR            = $$SRC/../lib/win32
linux-gcc:LIBDIR        = $$SRC/../lib/linux

## Directory where the target must be located
DESTDIR                 = $$SRC/../export

## Add here the source (.c) files of the module (1 line per source file)
SOURCES                += $$SRC/ccp_calc.c
SOURCES                += $$SRC/dst_ac.c
SOURCES                += $$SRC/DSTData.c
SOURCES                += $$SRC/DSTDecMain.c
SOURCES                += $$SRC/DSTDecoder.c
SOURCES                += $$SRC/dst_fram.c
SOURCES                += $$SRC/dst_init.c
SOURCES                += $$SRC/fio_dsd.c
SOURCES                += $$SRC/fio_dst.c
SOURCES                += $$SRC/UnpackDST.c

## Add here the header (.h) files of the module (1 line per header file)
HEADERS                += $$SRC/ccp_calc.h
HEADERS                += $$SRC/conststr.h
HEADERS                += $$SRC/dst_ac.h
HEADERS                += $$SRC/DSTData.h
HEADERS                += $$SRC/DSTDecoder.h
HEADERS                += $$SRC/dst_fram.h
HEADERS                += $$SRC/dst_init.h
HEADERS                += $$SRC/fio_dsd.h
HEADERS                += $$SRC/fio_dst.h
HEADERS                += $$SRC/types.h
HEADERS                += $$SRC/UnpackDST.h

## Add here the used libraries (1 line per library)
#  Pay attention: order of specification is important.
#  A library ("A") that depends on another ("B") must be put in front,
#  so "A" must be listed before "B".
#win32:LIBS             += $$LIBDIR/some_lib.lib

#unix:LIBS              += -L$$LIBDIR -lsome_lib

#####################################################################
### end of tmake project file                                     ###
#####################################################################
