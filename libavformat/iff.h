
#define ID_DSD        MKTAG('D','S','D',' ')
#define ID_DST        MKTAG('D','S','T',' ')

extern const AVCodecTag ff_dsdiff_codec_tags[];


typedef struct {
    uint64_t layout;
    const uint32_t * dsd_layout;
} DSDLayoutDesc;

extern const DSDLayoutDesc ff_dsdiff_channel_layout[];

