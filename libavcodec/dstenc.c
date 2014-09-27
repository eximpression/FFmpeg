/*
 * Direct Stream Transport (DST) encoder
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
 * Direct Stream Transport (DST) encoder
 */

#include "avcodec.h"
#include "internal.h"
#include "dst.h"

static av_cold int encode_init(AVCodecContext *avctx)
{
    if (avctx->channels < 2)
        return AVERROR(EINVAL);
    avctx->frame_size = DST_SAMPLES_PER_FRAME(avctx->sample_rate) / 8;
    return 0;
}

static int encode_frame(AVCodecContext *avctx, AVPacket *avpkt,
                        const AVFrame *frame, int *got_packet_ptr)
{
    int n, ret;
    n = frame->nb_samples * avctx->channels;
    if ((ret = ff_alloc_packet2(avctx, avpkt, n + 1, 0)) < 0)
        return ret;

    avpkt->data[0] = 0;
    memcpy(avpkt->data + 1, frame->data[0], n);
    avpkt->duration = avctx->frame_size;

    *got_packet_ptr = 1;
    return 0;
}

AVCodec ff_dst_encoder = {
    .name           = "dst",
    .long_name      = NULL_IF_CONFIG_SMALL("Direct Stream Transport (DST) encoder"),
    .type           = AVMEDIA_TYPE_AUDIO,
    .id             = AV_CODEC_ID_DST,
    .init           = encode_init,
    .encode2        = encode_frame,
    .sample_fmts    = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_DSD,
                                                     AV_SAMPLE_FMT_NONE },
};
