# Microsoft Developer Studio Project File - Name="RefDstEncoder" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=RefDstEncoder - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "RefDstEncoder.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "RefDstEncoder.mak" CFG="RefDstEncoder - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "RefDstEncoder - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "RefDstEncoder - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "RefDstEncoder - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /G5 /W4 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /stack:0x400000,0x400000 /subsystem:console /machine:I386 /out:"..\export\RefDstEncoder.exe"
# SUBTRACT LINK32 /profile /map /nodefaultlib

!ELSEIF  "$(CFG)" == "RefDstEncoder - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug"
# PROP Intermediate_Dir ".\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /W4 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /stack:0x400000,0x400000 /subsystem:console /map /debug /machine:I386
# SUBTRACT LINK32 /incremental:no /nodefaultlib

!ENDIF 

# Begin Target

# Name "RefDstEncoder - Win32 Release"
# Name "RefDstEncoder - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\ACEncodeFrame.c
# End Source File
# Begin Source File

SOURCE=.\BitPLookUp.c
# End Source File
# Begin Source File

SOURCE=.\CalcAutoVectors.c
# End Source File
# Begin Source File

SOURCE=.\CalcFCoefs.c
# End Source File
# Begin Source File

SOURCE=.\CountForProbCalc.c
# End Source File
# Begin Source File

SOURCE=.\DST_fram.c
# End Source File
# Begin Source File

SOURCE=.\DST_init.c
# End Source File
# Begin Source File

SOURCE=.\DSTEncMain.c
# End Source File
# Begin Source File

SOURCE=.\DSTEncoder.c
# End Source File
# Begin Source File

SOURCE=.\fio_dsd.c
# End Source File
# Begin Source File

SOURCE=.\fir.c
# End Source File
# Begin Source File

SOURCE=.\FrameToStream.c
# End Source File
# Begin Source File

SOURCE=.\GeneratePTables.c
# End Source File
# Begin Source File

SOURCE=.\QuantFCoefs.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\ACEncodeFrame.h
# End Source File
# Begin Source File

SOURCE=.\BitPLookUp.h
# End Source File
# Begin Source File

SOURCE=.\CalcAutoVectors.h
# End Source File
# Begin Source File

SOURCE=.\CalcFCoefs.h
# End Source File
# Begin Source File

SOURCE=.\conststr.h
# End Source File
# Begin Source File

SOURCE=.\CountForProbCalc.h
# End Source File
# Begin Source File

SOURCE=.\DST_fram.h
# End Source File
# Begin Source File

SOURCE=.\DST_init.h
# End Source File
# Begin Source File

SOURCE=.\DSTEncoder.h
# End Source File
# Begin Source File

SOURCE=.\fio_dsd.h
# End Source File
# Begin Source File

SOURCE=.\FIR.h
# End Source File
# Begin Source File

SOURCE=.\FrameToStream.h
# End Source File
# Begin Source File

SOURCE=.\GeneratePTables.h
# End Source File
# Begin Source File

SOURCE=.\QuantFCoefs.h
# End Source File
# Begin Source File

SOURCE=.\types.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
