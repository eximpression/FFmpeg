include $(SUBDIR)../config.mak

NAME = swresample
FFLIBS = avutil

HEADERS = swresample.h                       \
          version.h                          \

OBJS = audioconvert.o                        \
       dither.o                              \
       options.o                             \
       rematrix.o                            \
       resample.o                            \
       resample_dsp.o                        \
       swresample.o                          \
       swresample_frame.o                    \

OBJS-$(CONFIG_LIBSOXR) += soxr_resample.o
OBJS-$(CONFIG_SHARED)  += log2_tab.o

# Windows resource file
SLIBOBJS-$(HAVE_GNU_WINDRES) += swresampleres.o

SKIPHEADERS                            += %_tablegen.h                  \
                                          %_tables.h                    \

TESTPROGS = swresample

HOSTPROGS = dsd_tablegen                                                \

CLEANFILES = *_tables.c *_tables.h *_tablegen$(HOSTEXESUF)

GEN_HEADERS = dsd_tables.h dv_tables.h

GEN_HEADERS := $(addprefix $(SUBDIR), $(GEN_HEADERS))

$(GEN_HEADERS): $(SUBDIR)%_tables.h: $(SUBDIR)%_tablegen$(HOSTEXESUF)
	$(M)./$< > $@

ifdef CONFIG_HARDCODED_TABLES
$(SUBDIR)audioconvert.o: $(SUBDIR)dsd_tables.h
endif
