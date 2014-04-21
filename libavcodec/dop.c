/*
 * DSD-over-PCM (DOP) encoder and decoder
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
 * DSD-over-PCM (DOP) encoder and decoder
 */

#include "libavcodec/internal.h"
#include "bytestream.h"
#include "avcodec.h"

typedef struct {
    int state;
    int sync;
} DOPContext;

static const uint8_t dop_marker[2] = {0x05, 0xFA};

static av_cold int decode_init(AVCodecContext *avctx)
{
    DOPContext * s = avctx->priv_data;
    avctx->sample_fmt = AV_SAMPLE_FMT_DSD;

    switch(avctx->sample_rate) {
    case 176400:
    case 352800:
       avctx->sample_rate *= 16;
       break;
    case 2822400:
    case 5644800:
       break;
    default:
       av_log(avctx, AV_LOG_WARNING, "unsupported sample rate\n");
    }
    s->sync = 32 * avctx->channels;
    return 0;
}

static int decode_frame(AVCodecContext *avctx, void *data,
                        int *got_frame_ptr, AVPacket *avpkt)
{
    DOPContext * s = avctx->priv_data;
    const uint8_t *src = avpkt->data;
    uint8_t *dst;
    AVFrame *frame = data;
    int ret, i, ch;
    int pcm_samples = avpkt->size / (3 * avctx->channels);
    frame->nb_samples = 2 * pcm_samples;
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    dst = frame->data[0];

    for (i = 0; i < pcm_samples; i++) {
        for (ch = 0; ch < avctx->channels; ch++) {
            uint8_t d1     = src[0];
            uint8_t d0     = src[1];

            uint8_t marker = src[2];
            if (!s->sync) {
                if (marker == 0x05 || marker == 0xFA) {
                    s->state = marker == 0xFA;
                    s->sync++;
                }
                d0 = d1 = 0x69;
            } else {
                if (marker != dop_marker[s->state]) { /* lost sync */
                    s->sync = 0;
                    d0 = d1 = 0x69;
                } else if (s->sync < 32 * avctx->channels) { /* resynching */
                    s->sync++;
                    d0 = d1 = 0x69;
                }
            }
            src += 3;

            dst[0]               = d0;
            dst[avctx->channels] = d1;
            dst++;
        }
        dst += avctx->channels;
        s->state ^= 1;
    }

    *got_frame_ptr = 1;
    return pcm_samples * 3 * avctx->channels;
}

AVCodec ff_dop_s24le_decoder = {
    .name           = "dop_s24le",
    .priv_data_size = sizeof(DOPContext),
    .long_name      = NULL_IF_CONFIG_SMALL("DoP (DSD-over-PCM); signed 24-bit little-endian"),
    .type           = AVMEDIA_TYPE_AUDIO,
    .id             = AV_CODEC_ID_DOP_S24LE,
    .init           = decode_init,
    .decode         = decode_frame,
    .sample_fmts    = (const enum AVSampleFormat[]){AV_SAMPLE_FMT_DSD,
                                                    AV_SAMPLE_FMT_NONE },
};

static av_cold int encode_init(AVCodecContext *avctx)
{
    avctx->bits_per_coded_sample = av_get_bits_per_sample(avctx->codec->id);
    avctx->block_align           = avctx->channels * avctx->bits_per_coded_sample / 8;
    avctx->bit_rate              = avctx->block_align * avctx->sample_rate * 8;
    avctx->sample_rate          /= 16;
    return 0;
}

static int encode_frame(AVCodecContext *avctx, AVPacket *avpkt,
                            const AVFrame *frame, int *got_packet_ptr)
{
    DOPContext * s = avctx->priv_data;
    int ret, i, ch;
    const uint8_t *src;
    uint8_t *dst;

    if (frame->nb_samples % 2)
        av_log(avctx, AV_LOG_WARNING, "expected mod 16 samples\n");

    if ((ret = ff_alloc_packet2(avctx, avpkt, ((frame->nb_samples) / 2) * 3 * avctx->channels, 0)) < 0)
        return ret;

    src = frame->data[0];
    dst = avpkt->data;
    for (i = 0; i < frame->nb_samples; i += 2) {
        for (ch = 0; ch < avctx->channels; ch++) {
            dst[0] = src[avctx->channels];
            dst[1] = src[0];
            dst[2] = dop_marker[s->state];
            src++;
            dst += 3;
        }
        src += avctx->channels;
        s->state ^= 1;
    }

    *got_packet_ptr = 1;
    return 0;
}

AVCodec ff_dop_s24le_encoder = {
    .name           = "dop_s24le",
    .priv_data_size = sizeof(DOPContext),
    .long_name      = NULL_IF_CONFIG_SMALL("DoP (DSD-over-PCM); signed 24-bit little-endian"),
    .type           = AVMEDIA_TYPE_AUDIO,
    .id             = AV_CODEC_ID_DOP_S24LE,
    .init           = encode_init,
    .encode2        = encode_frame,
    .capabilities   = CODEC_CAP_VARIABLE_FRAME_SIZE,
    .sample_fmts    = (const enum AVSampleFormat[]){AV_SAMPLE_FMT_DSD,
                                                    AV_SAMPLE_FMT_NONE},
};
