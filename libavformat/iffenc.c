/*
 * DSDIFF muxer
 * Copyright (c) 2014 Peter Ross <pross@xvid.org>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * DSDIFF muxer
 */

#include "avformat.h"
#include "avio_internal.h"
#include "id3v2.h"
#include "rawenc.h"
#include "iff.h"
#include "libavutil/opt.h"

typedef struct {
    int     id3v2tag;
    int64_t data_start;
    int32_t frame_count;
} IFFContext;

static void put_chunk_be32(AVIOContext *pb, int tag, int value)
{
    avio_wl32(pb, tag);
    avio_wb64(pb, 4);
    avio_wb32(pb, value);
}

static void iff_pad(AVIOContext *pb, int size)
{
    if ((size & 1))
        avio_w8(pb, 0);
}

static int write_id3(AVFormatContext *s)
{
    AVIOContext *pb = s->pb;
    int64_t start, end;
    int ret;

    avio_wl32(pb, MKTAG('I','D','3',' '));
    avio_skip(pb, 8);
    start = avio_tell(pb);
    ret = ff_id3v2_write_simple(s, 4, ID3v2_DEFAULT_MAGIC);
    if (ret < 0)
        return ret;
    end = avio_tell(pb);
    avio_seek(pb, start - 8, SEEK_SET);
    avio_wb64(pb, end - start);
    avio_seek(pb, end, SEEK_SET);
    iff_pad(pb, end);
    return 0;
}

static const DSDLayoutDesc * find_channel_layout(int channels, int layout)
{
    const DSDLayoutDesc * d;
    for (d = ff_dsdiff_channel_layout; d->layout; d++)
        if (av_get_channel_layout_nb_channels(d->layout) == channels && d->layout == layout)
            return d;
    return NULL;
}

static int write_header(AVFormatContext *s)
{
    IFFContext *iff = s->priv_data;
    AVIOContext *pb = s->pb;
    AVStream *st = s->streams[0];
    int64_t prop_start, prop_end;
    int i, tag;
    const char * name;
    const DSDLayoutDesc *d;

    if (s->nb_streams != 1) {
        av_log(s, AV_LOG_ERROR, "only one stream is supported\n");
        return AVERROR(EINVAL);
    }

    avio_wl32(pb, MKTAG('F','R','M','8'));
    avio_skip(pb, 8);
    avio_wl32(pb, MKTAG('D','S','D',' '));

    put_chunk_be32(pb, MKTAG('F','V','E','R'), 0x1050000);

    /* PROP group */

    avio_wl32(pb, MKTAG('P','R','O','P'));
    avio_skip(pb, 8);
    prop_start = avio_tell(pb);
    avio_wl32(pb, MKTAG('S','N','D',' '));

    put_chunk_be32(pb, MKTAG('F','S',' ',' '), st->codec->sample_rate);

    /* channel */
    avio_wl32(pb, MKTAG('C','H','N','L'));
    avio_wb64(pb, st->codec->channels * 4 + 2);
    avio_wb16(pb, st->codec->channels);
    d = find_channel_layout(st->codec->channels, st->codec->channel_layout);
    if (d)
        avio_write(pb, (const unsigned char*)d->dsd_layout, st->codec->channels * 4);
    else
        for (i = 0; i < st->codec->channels; i++) 
            avio_wl32(pb, MKTAG('C','0','0', i));

    /* compression */
    tag = ff_codec_get_tag(ff_dsdiff_codec_tags, st->codec->codec_id);
    avio_wl32(pb, MKTAG('C','M','P','R'));
    switch(st->codec->codec_id) {
    case AV_CODEC_ID_DSD_MSBF: name = "not compressed"; break;
    case AV_CODEC_ID_DST:      name = "DST compressed"; break;
    default:
        av_log(s, AV_LOG_ERROR, "unsupported coded\n");
        return AVERROR(EINVAL);
    }
    /* foo_input_sacd < 0.7.3 and the 14496-3 DST reference decoder expect
       this chunk size to include padding, even though it is implied */
    avio_wb64(pb, (5 + strlen(name) + 1) & ~1);
    avio_wl32(pb, tag);
    avio_w8(pb, strlen(name));
    avio_write(pb, name, strlen(name));
    iff_pad(pb, 5 + strlen(name));

    //FIXME: ff_id3v2_write_simple() renames "title" to "TALB" and "artist" to "TPE1"
    //       we need to refer to these in write_trailer()
    if (iff->id3v2tag)
        write_id3(s);

    avio_wl32(pb, MKTAG('A','B','S','S'));
    avio_wb64(pb, 8);
    ffio_fill(pb, 0, 8);

    prop_end = avio_tell(pb);

    /* DSD/DST block */

    avio_wl32(pb, tag);
    avio_skip(pb, 8);
    iff->data_start = avio_tell(pb);

    avio_seek(pb, prop_start - 8, SEEK_SET);
    avio_wb64(pb, prop_end - prop_start);
    avio_seek(pb, iff->data_start, SEEK_SET);

    if (st->codec->codec_id == AV_CODEC_ID_DST) {
        avio_wl32(pb, MKTAG('F','R','T','E'));
        avio_wb64(pb, 6);
        avio_skip(pb, 4);
        avio_wb16(pb, 75);
    }

    return 0;
}

static int write_packet(AVFormatContext *s, AVPacket *pkt)
{
    IFFContext *iff = s->priv_data;
    AVIOContext *pb = s->pb;
    AVStream    *st = s->streams[0];
    int dst = st->codec->codec_id == AV_CODEC_ID_DST;

    if (dst) {
        avio_wl32(pb, MKTAG('D','S','T','F'));
        avio_wb64(pb, pkt->size);
        iff->frame_count++;
    }
    avio_write(pb, pkt->data, pkt->size);
    if (dst && (pkt->size & 1))
       avio_w8(pb, 0);
    return 0;
}

static void write_metadata(AVIOContext *pb, int tag, const char *value)
{
    int size = strlen(value);
    avio_wl32(pb, tag);
    avio_wb64(pb, 4 + size);
    avio_wb32(pb, size);
    avio_write(pb, value, (size + 1) & ~1);
}

static int write_trailer(AVFormatContext *s)
{
    IFFContext *iff = s->priv_data;
    AVIOContext *pb = s->pb;
    AVStream    *st = s->streams[0];
    int64_t data_end, file_end, diin_start;
    const AVDictionaryEntry *artist, *title;

    data_end  = avio_tell(pb);
    iff_pad(pb, data_end);

    /* DIIN block */

    artist = av_dict_get(s->metadata, "artist", NULL, 0);
    title  = av_dict_get(s->metadata, "title", NULL, 0);
    if (title || artist) {
        avio_wl32(pb, MKTAG('D','I','I','N'));
        avio_skip(pb, 8);

        diin_start = avio_tell(pb);
        if (artist) write_metadata(pb, MKTAG('D','I','A','R'), artist->value);
        if (title)  write_metadata(pb, MKTAG('D','I','T','I'), title->value);
    }

    file_end = avio_tell(pb);

    avio_seek(pb, 0x4, SEEK_SET);
    avio_wb64(pb, file_end - 12);

    avio_seek(pb, iff->data_start - 8, SEEK_SET);
    avio_wb64(pb, data_end - iff->data_start);

    if (st->codec->codec_id == AV_CODEC_ID_DST) {
        avio_seek(pb, iff->data_start + 12, SEEK_SET);
        avio_wb32(pb, iff->frame_count);
    }

    if (title || artist) {
        avio_seek(pb, diin_start - 8, SEEK_SET);
        avio_wb64(pb, file_end - diin_start);
    }

    return 0;
}

#define ENC AV_OPT_FLAG_ENCODING_PARAM
#define OFFSET(obj) offsetof(IFFContext, obj)
static const AVOption options[] = {
    { "write_id3v2", "Enable ID3v2 tag writing", OFFSET(id3v2tag), AV_OPT_TYPE_INT, {.i64 = 0}, 0, 1, ENC},
    { NULL },
};

static const AVClass iff_muxer_class = {
    .class_name     = "IFF muxer",
    .item_name      = av_default_item_name,
    .option         = options,
    .version        = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_iff_muxer = {
    .name           = "iff",
    .long_name      = NULL_IF_CONFIG_SMALL("IFF"),
    .priv_data_size = sizeof(IFFContext),
    .extensions     = "dff,dif",
    .audio_codec    = AV_CODEC_ID_DSD_MSBF,
    .write_header   = write_header,
    .write_packet   = write_packet,
    .write_trailer  = write_trailer,
    .priv_class     = &iff_muxer_class,
};
