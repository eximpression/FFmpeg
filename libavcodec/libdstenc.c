/*
 * ISO 14496-3 Direct Stream Transport (DST) Reference Encoder
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
 * ISO 14496-3 Direct Stream Transport (DST) Reference Encoder
 */

#include "avcodec.h"
#include "internal.h"
#include "dst.h"

#include "libdstenc/DSTEncoder.c"
#include "libdstenc/dst_init.c"
#include "libdstenc/dst_fram.c"
#include "libdstenc/CalcAutoVectors.c"
#include "libdstenc/CalcFCoefs.c"
#include "libdstenc/QuantFCoefs.c"
#include "libdstenc/CountForProbCalc.c"
#include "libdstenc/GeneratePTables.c"
#include "libdstenc/FIR.c"
#include "libdstenc/CountForProbCalc.h"
#include "libdstenc/GeneratePTables.h"
#include "libdstenc/BitPLookUp.c"
#include "libdstenc/ACEncodeFrame.c"
#include "libdstenc/FrameToStream.c"

static av_cold int encode_init(AVCodecContext *avctx)
{
    av_log(avctx, AV_LOG_INFO, "using reference encoder\n");
    avctx->frame_size = DST_SAMPLES_PER_FRAME(avctx->sample_rate) / 8;
    if (!MANGLE(Init)(avctx->channels, DSD_FS44(avctx->sample_rate)))
        return AVERROR(EINVAL);
    return 0;
}

static int encode_frame(AVCodecContext *avctx, AVPacket *avpkt,
                        const AVFrame *frame, int *got_packet_ptr)
{
    int n, ret;
    n = frame->nb_samples * avctx->channels;
    if ((ret = ff_alloc_packet2(avctx, avpkt, n + 1, 0)) < 0)
        return ret;

    if (MANGLE(Encode)(frame->data[0], avpkt->data, &avpkt->size) != DST_NOERROR) {
        return AVERROR(EINVAL);
    }
    avpkt->duration = avctx->frame_size;
    *got_packet_ptr = 1;
    return 0;
}

static int encode_close(AVCodecContext *avctx)
{
    MANGLE(Close)();
    return 0;
}

AVCodec ff_libdst_encoder = {
    .name           = "libdst",
    .long_name      = NULL_IF_CONFIG_SMALL("Direct Stream Transport (DST) encoder (libdstenc)"),
    .type           = AVMEDIA_TYPE_AUDIO,
    .id             = AV_CODEC_ID_DST,
    .init           = encode_init,
    .encode2        = encode_frame,
    .close          = encode_close,
    .sample_fmts    = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_DSD,
                                                     AV_SAMPLE_FMT_NONE },
};
