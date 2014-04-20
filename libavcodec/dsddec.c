/*
 * Direct Stream Digital (DSD) decoder
 * based on BSD licensed dsd2pcm by Sebastian Gesemann
 * Copyright (c) 2009, 2011 Sebastian Gesemann. All rights reserved.
 * Copyright (c) 2014 Peter Ross
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
 * Direct Stream Digital (DSD) decoder
 */

#include "libavcodec/internal.h"
#include "libavcodec/mathops.h"
#include "avcodec.h"

static void reverse_memcpy(uint8_t *dst, const uint8_t *src, int size)
{
    int i;
    for (i = 0; i < size; i++)
        dst[i] = ff_reverse[src[i]];
}

static av_cold int decode_init(AVCodecContext *avctx)
{
    avctx->sample_fmt = avctx->codec->sample_fmts[0];
    return 0;
}

static int decode_frame(AVCodecContext *avctx, void *data,
                        int *got_frame_ptr, AVPacket *avpkt)
{
    const uint8_t * src = avpkt->data;
    AVFrame *frame = data;
    int ret, ch;

    frame->nb_samples = avpkt->size / avctx->channels;
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    switch(avctx->codec_id) {
    case AV_CODEC_ID_DSD_LSBF:
        reverse_memcpy(frame->data[0], src, frame->nb_samples * avctx->channels);
        break;
    case AV_CODEC_ID_DSD_MSBF:
        memcpy(frame->data[0], src, frame->nb_samples * avctx->channels);
        break;
    case AV_CODEC_ID_DSD_LSBF_PLANAR:
        for (ch = 0; ch < avctx->channels; ch++ ) {
            reverse_memcpy(frame->extended_data[ch], src, frame->nb_samples);
            src += frame->nb_samples;
        }
        break;
    case AV_CODEC_ID_DSD_MSBF_PLANAR:
        for (ch = 0; ch < avctx->channels; ch++ ) {
            memcpy(frame->extended_data[ch], src, frame->nb_samples);
            src += frame->nb_samples;
        }
        break;
    default:
        return -1;
    }
    *got_frame_ptr = 1;
    return frame->nb_samples * avctx->channels;
}

#define DSD_DECODER(id_, sample_fmt_, name_, long_name_) \
AVCodec ff_##name_##_decoder = { \
    .name         = #name_, \
    .long_name    = NULL_IF_CONFIG_SMALL(long_name_), \
    .type         = AVMEDIA_TYPE_AUDIO, \
    .id           = AV_CODEC_ID_##id_, \
    .init         = decode_init, \
    .decode       = decode_frame, \
    .sample_fmts  = (const enum AVSampleFormat[]){ sample_fmt_, \
                                                   AV_SAMPLE_FMT_NONE }, \
};

DSD_DECODER(DSD_LSBF,        AV_SAMPLE_FMT_DSD,  dsd_lsbf,        "DSD (Direct Stream Digital), least significant bit first")
DSD_DECODER(DSD_MSBF,        AV_SAMPLE_FMT_DSD,  dsd_msbf,        "DSD (Direct Stream Digital), most significant bit first")
DSD_DECODER(DSD_LSBF_PLANAR, AV_SAMPLE_FMT_DSDP, dsd_lsbf_planar, "DSD (Direct Stream Digital), least significant bit first, planar")
DSD_DECODER(DSD_MSBF_PLANAR, AV_SAMPLE_FMT_DSDP, dsd_msbf_planar, "DSD (Direct Stream Digital), most significant bit first, planar")

static av_cold int encode_init(AVCodecContext *avctx)
{
    avctx->bits_per_coded_sample = av_get_bits_per_sample(avctx->codec->id);
    avctx->bit_rate              = avctx->sample_rate * avctx->channels;
    if (avctx->sample_fmt == AV_SAMPLE_FMT_DSDP) {
        avctx->frame_size  = 4096;
        avctx->block_align = avctx->frame_size * avctx->channels;
    }
    return 0;
}

static int encode_frame(AVCodecContext *avctx, AVPacket *avpkt,
                            const AVFrame *frame, int *got_packet_ptr)
{
    uint8_t *dst;
    int n, ret, ch;
    n = frame->nb_samples * avctx->channels;
    if ((ret = ff_alloc_packet2(avctx, avpkt, n, 0)) < 0)
        return ret;
    dst = avpkt->data;

    switch(avctx->codec_id) {
    case AV_CODEC_ID_DSD_LSBF:
        reverse_memcpy(dst, frame->data[0], n);
        break;
    case AV_CODEC_ID_DSD_MSBF:
        memcpy(dst, frame->data[0], n);
        break;
    case AV_CODEC_ID_DSD_LSBF_PLANAR:
        for (ch = 0; ch < avctx->channels; ch++ ) {
            reverse_memcpy(dst, frame->extended_data[ch], frame->nb_samples);
            dst += frame->nb_samples;
        }
        break;
    case AV_CODEC_ID_DSD_MSBF_PLANAR:
        for (ch = 0; ch < avctx->channels; ch++ ) {
            memcpy(dst, frame->extended_data[ch], frame->nb_samples);
            dst += frame->nb_samples;
        }
        break;
    default:
        return -1;
    }
    *got_packet_ptr = 1;
    return 0;
}

#define DSD_ENCODER(id_, sample_fmt_, name_, long_name_, capabilities_) \
AVCodec ff_##name_##_encoder = { \
    .name         = #name_, \
    .long_name    = NULL_IF_CONFIG_SMALL(long_name_), \
    .type         = AVMEDIA_TYPE_AUDIO, \
    .id           = AV_CODEC_ID_##id_, \
    .init         = encode_init, \
    .encode2      = encode_frame, \
    .capabilities = capabilities_,\
    .sample_fmts  = (const enum AVSampleFormat[]){ sample_fmt_, \
                                                   AV_SAMPLE_FMT_NONE }, \
};

DSD_ENCODER(DSD_LSBF,        AV_SAMPLE_FMT_DSD,  dsd_lsbf,        "DSD (Direct Stream Digital), least significant bit first", CODEC_CAP_VARIABLE_FRAME_SIZE)
DSD_ENCODER(DSD_MSBF,        AV_SAMPLE_FMT_DSD,  dsd_msbf,        "DSD (Direct Stream Digital), most significant bit first", CODEC_CAP_VARIABLE_FRAME_SIZE)
DSD_ENCODER(DSD_LSBF_PLANAR, AV_SAMPLE_FMT_DSDP, dsd_lsbf_planar, "DSD (Direct Stream Digital), least significant bit first, planar", 0)
DSD_ENCODER(DSD_MSBF_PLANAR, AV_SAMPLE_FMT_DSDP, dsd_msbf_planar, "DSD (Direct Stream Digital), most significant bit first, planar", 0)
