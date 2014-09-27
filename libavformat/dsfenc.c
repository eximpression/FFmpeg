/*
 * DSD Stream File (DSF) muxer
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
 * DSD Stream File (DSF) muxer
 */

#include "avformat.h"
#include "avio_internal.h"
#include "id3v2.h"
#include "rawenc.h"

static const AVCodecTag codec_dsf_tags[] = {
    { AV_CODEC_ID_DSD_LSBF_PLANAR, 1 },
    { AV_CODEC_ID_DSD_MSBF_PLANAR, 8 },
};

static int write_header(AVFormatContext *s)
{
    AVIOContext *pb = s->pb;
    AVStream *st = s->streams[0];

    if (s->nb_streams != 1) {
        av_log(s, AV_LOG_ERROR, "only one stream is supported\n");
        return AVERROR(EINVAL);
    }

    if (!ff_codec_get_tag(codec_dsf_tags, st->codec->codec_id)) {
        av_log(s, AV_LOG_ERROR, "unsupported codec\n");
        return AVERROR(EINVAL);
    }

    if (st->codec->frame_size != 4096) {
        av_log(s, AV_LOG_ERROR, "frame_size != 4096\n");
        return AVERROR(EINVAL);
    }

    avio_wl32(pb, MKTAG('D','S','D',' '));
    avio_wl64(pb, 28);
    avio_skip(pb, 16);

    avio_wl32(pb, MKTAG('f','m','t',' '));
    avio_wl64(pb, 52);
    avio_wl32(pb, 1);
    avio_wl32(pb, 0);
    avio_wl32(pb, st->codec->channels);
    avio_wl32(pb, st->codec->channels);
    avio_wl32(pb, st->codec->sample_rate);
    avio_wl32(pb, ff_codec_get_tag(codec_dsf_tags, st->codec->codec_id));
    avio_skip(pb, 8);
    avio_wl32(pb, st->codec->frame_size);
    ffio_fill(pb, 0, 4);

    avio_wl32(pb, MKTAG('d','a','t','a'));
    avio_wl64(pb, 52);
    return 0;
}

static int write_trailer(AVFormatContext *s)
{
    AVIOContext *pb = s->pb;
    AVStream *st = s->streams[0];
    int64_t data_end, total_size;

    data_end  = avio_tell(pb);

    ff_id3v2_write_simple(s, 4, ID3v2_DEFAULT_MAGIC);
    total_size = avio_tell(pb);

    avio_seek(pb, 0xC, SEEK_SET);
    avio_wl64(pb, total_size);

    /* id3v2 offset */
    avio_seek(pb, 0x14, SEEK_SET);
    avio_wl64(pb, data_end);

    /* total 1-bit samples per channel */
    avio_seek(pb, 0x40, SEEK_SET);
    avio_wl64(pb, ((data_end - 0x5c) / st->codec->channels) * 8);

    /* data chunk size */
    avio_seek(pb, 0x54, SEEK_SET);
    avio_wl64(pb, data_end - 0x50);
    return 0;
}

AVOutputFormat ff_dsf_muxer = {
    .name           = "dsf",
    .long_name      = NULL_IF_CONFIG_SMALL("DSD Stream File (DSF)"),
    .extensions     = "dsf",
    .audio_codec    = AV_CODEC_ID_DSD_LSBF_PLANAR,
    .write_header   = write_header,
    .write_packet   = ff_raw_write_packet,
    .write_trailer  = write_trailer,
};
