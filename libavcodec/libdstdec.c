/*
 * ISO 14496-3 Direct Stream Transfer (DST) Reference Decoder
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
 * ISO 14496-3 Direct Stream Transfer (DST) Refernce Decoder
 */

#include "internal.h"
#include "avcodec.h"
#include "libavutil/avassert.h"

#include "libdstdec/DSTDecoder.c"
#include "libdstdec/dst_init.c"
#include "libdstdec/dst_fram.c"
#include "libdstdec/ccp_calc.c"
#include "libdstdec/UnpackDST.c"
#include "libdstdec/dst_ac.c"
#include "libdstdec/DSTData.c"

#include "dst.h"

static av_cold int decode_init(AVCodecContext *avctx)
{
    av_log(avctx, AV_LOG_INFO, "using reference decoder\n");
    MANGLE(Init)(avctx->channels, DSD_FS44(avctx->sample_rate));
    avctx->sample_fmt = AV_SAMPLE_FMT_DSD;
    return 0;
}

static int decode_frame(AVCodecContext *avctx, void *data,
                        int *got_frame_ptr, AVPacket *avpkt)
{
    AVFrame *frame = data;
    uint32_t size;
    int ret;

    frame->nb_samples = DST_SAMPLES_PER_FRAME(avctx->sample_rate) / 8;
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    size = avpkt->size;
    MANGLE(Decode)(avpkt->data, frame->data[0], 0 /* frame counter for debugging */, &size);

    if (size != avpkt->size)
        av_log(avctx, AV_LOG_WARNING, "packet size was %i, but decoder consumed %i\n", avpkt->size, size);

    *got_frame_ptr = 1;
    return avpkt->size;
}

static av_cold int decode_close(AVCodecContext *avctx)
{
    MANGLE(Close)();
    return 0;
}

AVCodec ff_libdst_decoder = {
    .name         = "libdst",
    .long_name    = NULL_IF_CONFIG_SMALL("Digital Stream Transport (libdstdec)"),
    .type         = AVMEDIA_TYPE_AUDIO,
    .id           = AV_CODEC_ID_DST,
    .init         = decode_init,
    .decode       = decode_frame,
    .close        = decode_close,
    .sample_fmts  = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_DSD,
                                                   AV_SAMPLE_FMT_NONE },
};
