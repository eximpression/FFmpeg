/*
 * Direct Stream Transfer (DST) decoder
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
 * Direct Stream Transfer (DST) decoder
 * ISO/IEC 14496-3 Part 3 Subpart 10: Technical description of lossless coding of oversampled audio
 */

#include "avcodec.h"
#include "dst.h"
#include "internal.h"
#include "get_bits.h"
#include "golomb.h"
#include "mathops.h"
#include "thread.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"

#define DST_MAX_CHANNELS 6
#define DST_MAX_ELEMENTS (2 * DST_MAX_CHANNELS)

static const int8_t fsets_code_pred_coeff[3][3] = {
    { -8 },
    { -16, 8 },
    { -9, -5, 6 },
};

static const int8_t probs_code_pred_coeff[3][3] = {
    { -8 },
    { -16, 8 },
    { -24, 24, -8 },
};

typedef struct {
    AVCodecContext *avctx;
    int verify;
} DSTContext;

typedef struct {
    unsigned int elements;
    unsigned int length[DST_MAX_ELEMENTS];
    int coeff[DST_MAX_ELEMENTS][128];
} Table;

static av_cold int decode_init(AVCodecContext *avctx)
{
    if (avctx->channels > DST_MAX_CHANNELS) {
        avpriv_request_sample(avctx, "Channel count %d", avctx->channels);
        return AVERROR_PATCHWELCOME;
    }

    avctx->sample_fmt = AV_SAMPLE_FMT_DSD;
    return 0;
}

/**
 * FIXME: comment
 */
static int read_map(GetBitContext *gb, Table *t, unsigned int map[DST_MAX_CHANNELS], int channels)
{
    int ch;
    t->elements = 1;
    if (!get_bits1(gb)){
        map[0] = 0;
        for (ch = 1; ch < channels; ch++) {
            int bits = av_log2(t->elements) + 1;
            map[ch] = get_bits(gb, bits);
            if (map[ch] == t->elements) {
                t->elements++;
                if (t->elements >= DST_MAX_ELEMENTS)
                    return AVERROR_INVALIDDATA;
            } else if (map[ch] > t->elements) {
                return AVERROR_INVALIDDATA;
            }
        }
    } else {
        memset(map, 0, sizeof(*map) * DST_MAX_CHANNELS);
    }
    return 0;
}

/**
 * FIXME: comment
 */
static av_always_inline int get_sr_golomb_dst(GetBitContext *gb, unsigned int k)
{
#if 0
    /* 'run_length' upper bound is not specified; we can never be sure it will fit into get_bits cache */
    int v = get_ur_golomb(gb, k, INT_MAX, 0);
#else
    int v = 0;
    while (!get_bits1(gb))
        v++;
    if (k)
        v = (v << k) | get_bits(gb, k);
#endif
    if (v && get_bits1(gb))
        v = -v;
    return v;
}

static void read_uncoded_coeff(GetBitContext *gb, int *dst, unsigned int elements, int coeff_bits, int is_signed, int offset)
{
    unsigned int i;
    for (i = 0; i < elements; i++)
        dst[i] = (is_signed ? get_sbits(gb, coeff_bits) : get_bits(gb, coeff_bits)) + offset;
}

static int read_table(GetBitContext *gb, Table *t, const int8_t code_pred_coeff[3][3], int length_bits, int coeff_bits, int is_signed, int offset)
{
    unsigned int i, j, k;
    for (i = 0; i < t->elements; i++) {
        t->length[i] = get_bits(gb, length_bits) + 1;
        if (!get_bits1(gb)) {
            read_uncoded_coeff(gb, t->coeff[i], t->length[i], coeff_bits, is_signed, offset);
        } else {
            int method = get_bits(gb, 2), lsb_size;
            if (method == 3)
                return AVERROR_INVALIDDATA;

            read_uncoded_coeff(gb, t->coeff[i], method + 1, coeff_bits, is_signed, offset);

            lsb_size  = get_bits(gb, 3);
            for (j = method + 1; j < t->length[i]; j++) {
                int c, x = 0;
                for (k = 0; k < method + 1; k++)
                    x += code_pred_coeff[method][k] * t->coeff[i][j - k - 1];
                c = get_sr_golomb_dst(gb, lsb_size);
                if (x >= 0)
                    c -= (x + 4) >> 3;
                else
                    c += (-x + 3) / 8;
                t->coeff[i][j] = c;
            }
        }
    }
    return 0;
}

typedef struct {
    unsigned int a;
    unsigned int c;
} Arith;

static void ac_init(Arith * ac, GetBitContext *gb)
{
    ac->a = 4095;
    ac->c = get_bits(gb, 12);
}

#define AC_GET(ac, re, gb, p, predict15, v) \
{ \
    unsigned int k = ((ac)->a >> 8) | (((ac)->a >> 7) & 1); \
    unsigned int q = k * p; \
    unsigned int a_q = (ac)->a - q; \
    if ((ac)->c < a_q) { \
        v = (predict15 ^ 1) & 1; \
        (ac)->a  = a_q; \
    } else { \
        v = (predict15 ^ 0) & 1; \
        (ac)->a  = q; \
        (ac)->c -= a_q; \
    } \
    if ((ac)->a < 2048) { \
        int n = 11 - av_log2((ac)->a); \
        (ac)->a <<= n; \
        (ac)->c = ((ac)->c << n) | SHOW_UBITS(re, gb, n); \
        SKIP_BITS(re, pb, n); \
        UPDATE_CACHE(re, gb); \
    } \
}

static uint8_t prob_dst_x_bit(int c)
{
    return (ff_reverse[c & 127] >> 1) + 1;
}

/**
 * FIXME: comment
 */
static void build_filter(int16_t table[DST_MAX_ELEMENTS][16][256], const Table *fsets)
{
    int i, j, k, l;
    for (i = 0; i < fsets->elements; i++) {
        int length = fsets->length[i];
        for (j = 0; j < 16; j++) {
            int total = av_clip(length - j * 8, 0, 8);
            for (k = 0; k < 256; k++) {
                int v = 0;
                for (l = 0; l < total; l++)
                    /* this is a faster v += k & (1 << l) ? coeff : -coeff; */
                    v += (((k >> l) & 1) * 2 - 1) * fsets->coeff[i][j * 8 + l];
                table[i][j][k] = v;
            }
        }
    }
}

#define F(ch,i) filter[felem][i][status.u8[ch][i]]

//FIXME: 32-bit version
//FIXME: big endian not tested
#if HAVE_BIGENDIAN
#define shift128left1(s,v) \
    s[0] = (s[0] << 1) | (s[1] >> 63); \
    s[1] = (s[1] << 1) | v
#else
#define shift128left1(s,v) \
    s[1] = (s[1] << 1) | (s[0] >> 63); \
    s[0] = (s[0] << 1) | v
#endif

#define DECODE(ch, dst, shift) \
{ \
    unsigned int felem = map_ch_to_felem[ch], prob, v; \
    int16_t predict = F(ch,  0) + F(ch,  1) + F(ch,  2) + F(ch,  3) + \
                      F(ch,  4) + F(ch,  5) + F(ch,  6) + F(ch,  7) + \
                      F(ch,  8) + F(ch,  9) + F(ch, 10) + F(ch, 11) + \
                      F(ch, 12) + F(ch, 13) + F(ch, 14) + F(ch, 15); \
    if (!half_prob[ch] || i >= fsets.length[felem]) { \
        unsigned int pelem = map_ch_to_pelem[ch]; \
        unsigned int index = FFABS(predict) >> 3; \
        prob = probs.coeff[pelem][FFMIN(index, probs.length[pelem] - 1)]; \
    } else \
        prob = 128; \
    predict >>= 15; \
    AC_GET(&ac, re, &gb, prob, predict, v); \
    dst |= v << shift; \
    shift128left1(status.u64[ch], v); \
}

static unsigned int calc_checksum(const uint8_t * data, unsigned int length)
{
    unsigned int Poly = 0x40000008;
    const unsigned long Long_msb = 0x80000000;
    const unsigned long Short_msb = 0x80;
    int MessageByte;
    int i,j;
    long unsigned Rem;

    Rem = 0L;
    for (i=0; i < length; i++) {
        MessageByte = data[i];
        for (j = 0; j < 8; j++) {
            if ( (((MessageByte & Short_msb) == Short_msb)) != (((Rem & Long_msb) == Long_msb))) {
                Rem ^= Poly;           // XOR and shift with value 1
                Rem = (Rem << 1) + 1;
            } else {
                Rem = (Rem << 1);      // no XOR, shift with value 0
            }
            MessageByte <<= 1;
        }
    }

    return (Rem);
}

static int decode_frame(AVCodecContext *avctx, void *data,
                        int *got_frame_ptr, AVPacket *avpkt)
{
    DSTContext *s = avctx->priv_data;
    AVFrame *frame = data;
    unsigned int samples_per_frame = DST_SAMPLES_PER_FRAME(avctx->sample_rate);
    ThreadFrame tframe = { .f = frame };
    int ret, j;
    unsigned int i, ch, same;
    GetBitContext gb;
    Arith ac;
//FIXME: struct
    Table fsets, probs;
    unsigned int half_prob[DST_MAX_CHANNELS];
    unsigned int map_ch_to_felem[DST_MAX_CHANNELS];
    unsigned int map_ch_to_pelem[DST_MAX_CHANNELS];
    union {
        uint8_t  u8[DST_MAX_CHANNELS][16];
        uint64_t u64[DST_MAX_CHANNELS][2];
    } av_alias status;
    DECLARE_ALIGNED(16, int16_t, filter)[DST_MAX_ELEMENTS][16][256];

    if (avpkt->size < 1)
        return AVERROR_INVALIDDATA;

    frame->nb_samples = samples_per_frame / 8;
    if ((ret = ff_thread_get_buffer(avctx, &tframe, 0)) < 0)
        return ret;

    if (!(avpkt->data[0] & 0x80)) {
        if (frame->nb_samples > avpkt->size - 1)
            av_log(avctx, AV_LOG_WARNING, "short frame");
        memcpy(frame->data[0], avpkt->data + 1, FFMIN(frame->nb_samples * avctx->channels, avpkt->size - 1));
        *got_frame_ptr = 1;
        return avpkt->size;
    }

    if ((ret = init_get_bits8(&gb, avpkt->data, avpkt->size)) < 0)
        return ret;

    skip_bits1(&gb);

    /* Segmentation (10.4, 10.5, 10.6) */

    if (!get_bits1(&gb)){
        avpriv_request_sample(avctx, "Same_Segmentation=0");
        return AVERROR_PATCHWELCOME;
    }

    if (!get_bits1(&gb)){
        avpriv_request_sample(avctx, "Same_Segm_For_All_Channels=0");
        return AVERROR_PATCHWELCOME;
    }

    if (!get_bits1(&gb)){
        avpriv_request_sample(avctx, "End_Of_Channel_Segm=0");
        return AVERROR_PATCHWELCOME;
    }

    /* Mapping (10.7, 10.8, 10.9) */

    same = get_bits1(&gb);

    if ((ret = read_map(&gb, &fsets, map_ch_to_felem, avctx->channels)) < 0)
        return ret;

    if (same) {
        probs.elements = fsets.elements;
        memcpy(map_ch_to_pelem, map_ch_to_felem, sizeof(map_ch_to_pelem));
    } else
        if ((ret = read_map(&gb, &probs, map_ch_to_pelem, avctx->channels)) < 0)
            return ret;

    /* Half Probability (10.10) */

    for (ch = 0; ch < avctx->channels; ch++)
        half_prob[ch] = get_bits1(&gb);

    /* Filter Coef Sets (10.12) */

    read_table(&gb, &fsets, fsets_code_pred_coeff, 7, 9, 1, 0);

    /* Probability Tables (10.13) */

    read_table(&gb, &probs, probs_code_pred_coeff, 6, 7, 0, 1);

    /* Arithmetic Coded Data (10.11) */

    skip_bits1(&gb);
    ac_init(&ac, &gb);

    build_filter(filter, &fsets);
    memset(status.u8, 0xAA, sizeof(status.u8));

    {
        uint8_t *dst = frame->data[0];
        unsigned int dst_x_bit;
        OPEN_READER(re, &gb);
        UPDATE_CACHE(re, &gb);
        AC_GET(&ac, re, &gb, prob_dst_x_bit(fsets.coeff[0][0]), 0, dst_x_bit);

    if (avctx->channels == 2) {
        for (i = 0; i < samples_per_frame; i += 8) {
            unsigned int a = 0, b = 0;
            for (j = 7; j >= 0; j--) {
                DECODE(0, a, j)
                DECODE(1, b, j)
            }
            dst[0] = a;
            dst[1] = b;
            dst += 2;
        }
    } else if (avctx->channels == 5) {
        for (i = 0; i < samples_per_frame; i += 8) {
            unsigned int a = 0, b = 0, c = 0, d = 0, e = 0;
            for (j = 7; j >= 0; j--) {
                DECODE(0, a, j)
                DECODE(1, b, j)
                DECODE(2, c, j)
                DECODE(3, d, j)
                DECODE(4, e, j)
            }
            dst[0] = a; dst[1] = b; dst[2] = c; dst[3] = d; dst[4] = e;
            dst += 5;
        }
    } else if (avctx->channels == 6) {
        for (i = 0; i < samples_per_frame; i += 8) {
            unsigned int a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
            for (j = 7; j >= 0; j--) {
                DECODE(0, a, j)
                DECODE(1, b, j)
                DECODE(2, c, j)
                DECODE(3, d, j)
                DECODE(4, e, j)
                DECODE(5, f, j)
            }
            dst[0] = a; dst[1] = b; dst[2] = c; dst[3] = d; dst[4] = e; dst[5] = f;
            dst += 6;
        }
    } else {
        memset(frame->data[0], 0, avctx->channels * frame->nb_samples);
        for (i = 0; i < samples_per_frame; i++) {
            for (ch = 0; ch < avctx->channels; ch++)
                DECODE(ch, frame->data[0][ (i >> 3) * avctx->channels + ch], (7 - (i & 0x7)));
        }
    }
}

    if (s->verify) {
        uint8_t * checksum = av_packet_get_side_data(avpkt, AV_PKT_DATA_DST_CHECKSUM, NULL);
        if (checksum && calc_checksum(frame->data[0], samples_per_frame * avctx->channels / 8) != AV_RL32(checksum))
            av_log(avctx, AV_LOG_WARNING, "checksum mismatch\n");
    }

    *got_frame_ptr = 1;
    return avpkt->size;
}

#define DEC AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_AUDIO_PARAM
#define OFFSET(obj) offsetof(DSTContext, obj)
static const AVOption options[] = {
    { "verify", "Verify checksum", OFFSET(verify), AV_OPT_TYPE_INT, {.i64 = 0}, 0, 1, DEC},
    { NULL },
};

static const AVClass dst_decoder_class = {
    .class_name     = "DST decoder",
    .item_name      = av_default_item_name,
    .option         = options,
    .version        = LIBAVUTIL_VERSION_INT,
    .category       = AV_CLASS_CATEGORY_DECODER,
};

AVCodec ff_dst_decoder = {
    .name           = "dst",
    .long_name      = NULL_IF_CONFIG_SMALL("Digital Stream Transfer (DST)"),
    .priv_data_size = sizeof(DSTContext),
    .type           = AVMEDIA_TYPE_AUDIO,
    .id             = AV_CODEC_ID_DST,
    .init           = decode_init,
    .decode         = decode_frame,
    .capabilities   = CODEC_CAP_FRAME_THREADS,
    .sample_fmts    = (const enum AVSampleFormat[]){ AV_SAMPLE_FMT_DSD,
                                                     AV_SAMPLE_FMT_NONE },
    .priv_class     = &dst_decoder_class,
};
