/*
 * ISO 14496-3 Direct Stream Transfer (DST) "FAST" Reference Decoder
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
 * ISO 14496-3 Direct Stream Transfer (DST) "FAST" Reference Decoder
 */

#include "libavcodec/internal.h"
#include "avcodec.h"

#include "libdstdec_fast/ccp_calc.h"
#include "libdstdec_fast/ccp_calc.c"
#include "libdstdec_fast/dst_ac.h"
#include "libdstdec_fast/dst_ac.c"
#include "libdstdec_fast/dst_data.h"
#include "libdstdec_fast/dst_data.c"
#include "libdstdec_fast/dst_decoder.h"
#include "libdstdec_fast/dst_decoder.c"
#include "libdstdec_fast/dst_fram.h"
#include "libdstdec_fast/dst_fram.c"
#include "libdstdec_fast/dst_init.h"
#include "libdstdec_fast/dst_init.c"
#include "libdstdec_fast/types.h"
#include "libdstdec_fast/unpack_dst.h"
#include "libdstdec_fast/unpack_dst.c"

#include "dst.h"

typedef struct {
    ebunch context;
} DSTContext;

static av_cold int decode_init(AVCodecContext *avctx)
{
    DSTContext *s = avctx->priv_data;

    av_log(avctx, AV_LOG_INFO, "using \"FAST\" reference decoder\n");

    if (Init(&s->context, avctx->channels, DSD_FS44(avctx->sample_rate))) {
        av_log(avctx, AV_LOG_ERROR, "Init failed\n");
        return -1;
    }

    avctx->sample_fmt = AV_SAMPLE_FMT_DSD;
    return 0;
}

static int decode_frame(AVCodecContext *avctx, void *data,
                        int *got_frame_ptr, AVPacket *avpkt)
{
    DSTContext * s = avctx->priv_data;
    AVFrame *frame = data;
    uint32_t size = avpkt->size;
    int ret;

    frame->nb_samples = DST_SAMPLES_PER_FRAME(avctx->sample_rate) / 8;
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    ret = Decode(&s->context, avpkt->data, frame->data[0], 0 /* frame counter for debugging */, &size);
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Decode failed: %i\n", ret);
        return -1;
    }

    if (size != avpkt->size)
        av_log(avctx, AV_LOG_WARNING, "packet size was %i, but decoder consumed %i\n", avpkt->size, size);

    *got_frame_ptr = 1;
    return avpkt->size;
}

static av_cold int decode_close(AVCodecContext *avctx)
{
    DSTContext *s = avctx->priv_data;
    Close(&s->context);
    return 0;
}

AVCodec ff_libdst_fast_decoder = {
    .name         = "libdst_fast",
    .priv_data_size = sizeof(DSTContext),
    .long_name    = NULL_IF_CONFIG_SMALL("Digital Stream Transport (libdstdec_fast)"),
    .type         = AVMEDIA_TYPE_AUDIO,
    .id           = AV_CODEC_ID_DST,
    .init         = decode_init,
    .decode       = decode_frame,
    .close        = decode_close,
    .sample_fmts  = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_DSD,
                                                   AV_SAMPLE_FMT_NONE },
};
