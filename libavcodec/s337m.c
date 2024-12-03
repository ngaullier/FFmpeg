/*
 * S337M decoder
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

#include "libavutil/avassert.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"

#include "avcodec.h"
#include "codec_internal.h"
#include "decode.h"
#include "spdif_s337m_parser_internal.h"

typedef struct S337MDecodeContext {
    const AVClass   *class;
    AVCodecContext  *avctx;
    SPDIFS337MContext dectx;

    int             passthrough;

    int             inited;
    int             flushed;

    int             aes_start_position;
    AVCodecContext  *codec_avctx;
    AVFrame         *codec_frame;
    int             codec_initial_sample_rate;
    SwrContext      *swr;
    int64_t         next_pts;
    int             prev_aes_samples;
} S337MDecodeContext;

static av_cold int s337m_init(AVCodecContext *avctx)
{
    S337MDecodeContext *s = avctx->priv_data;
    SPDIFS337MContext *dectx = &s->dectx;

    dectx->avctx = s->avctx = avctx;

    return 0;
}

static int set_codec(S337MDecodeContext *s)
{
    SPDIFS337MContext *dectx = &s->dectx;
    const AVCodec *codec;
    int ret;

    if (s->codec_avctx) {
        if (s->codec_avctx->codec_id != dectx->codec)
            return AVERROR_INPUT_CHANGED;
        return 0;
    }

    codec = avcodec_find_decoder(dectx->codec);
    if (!codec)
        return AVERROR_BUG;

    s->codec_avctx = avcodec_alloc_context3(codec);
    if (!s->codec_avctx)
        return AVERROR(ENOMEM);

    ret = avcodec_open2(s->codec_avctx, codec, NULL);
    if (ret < 0)
        return ret;

    s->codec_frame = av_frame_alloc();
    if (!s->codec_frame)
        return AVERROR(ENOMEM);

    return 0;
}

static int init_resample(AVCodecContext *avctx, SwrContext **swrp, int in_sample_rate)
{
    SwrContext *swr = *swrp = swr_alloc();
    int ret;

    if (!swr)
        return AVERROR(ENOMEM);

    av_opt_set_chlayout(swr, "in_chlayout",  &avctx->ch_layout, 0);
    av_opt_set_chlayout(swr, "out_chlayout", &avctx->ch_layout, 0);
    av_opt_set_int(swr, "in_sample_fmt",     avctx->sample_fmt, 0);
    av_opt_set_int(swr, "out_sample_fmt",    avctx->sample_fmt, 0);
    av_opt_set_int(swr, "in_sample_rate",    in_sample_rate, 0);
    av_opt_set_int(swr, "out_sample_rate",   avctx->sample_rate, 0);

    /* There are two main cases that require sync to timestamps:
     * - dolby_e sample rate value is not accurate for drop frame video:
     * use soft comp responsively to handle these regular single-sample drifts
     * - in case of loss of s337m sync:
     * use hard comp to insert whole frames of silence
     *
     * And one use case in between that requires NO sync:
     * The guardband phase has to be silently ignored as the video
     * frame is assumed to be synced with sample 0 of the aes stream.
     */
    av_opt_set_int(swr,    "async",          1,        0);
    av_opt_set_double(swr, "min_comp",       1./48000, 0);
    av_opt_set_double(swr, "max_soft_comp",  0.0001,   0);
    av_opt_set_double(swr, "min_hard_comp",  0.02,     0);

    ret = swr_init(swr);
    if (ret < 0)
        return ret;

    return 0;
}

extern const FFCodec ff_s337m_24_decoder;

/* The first input packet is usually empty: it is guard band bytes.
 * Following packets always start with a syncword.
 * The 1-frame (2 frames if including the usual first null packet) delay
 * accomodates with the resampler, but the number of samples per frame is always preserved
 * (ex: alternation of 1601/1602 audio samples per frame for Dolby E@29.97).
 */
static int s337m_decode_frame(AVCodecContext *avctx, AVFrame *frame,
                            int *got_frame, AVPacket *avpkt)
{
    S337MDecodeContext *s1 = avctx->priv_data;
    SPDIFS337MContext *dectx = &s1->dectx;
    int aes_word_bits = avctx->codec == (AVCodec *)&ff_s337m_24_decoder ? 24 : 16;
    int aes_samples =  avpkt->size / (aes_word_bits >> 2);
    int prev_aes_samples = s1->prev_aes_samples;
    int ret, next;

    if (s1->flushed || !avpkt->size && s1->passthrough)
            return 0;

    if (s1->passthrough) {
        if (!s1->inited) {
            av_channel_layout_uninit(&avctx->ch_layout);
            avctx->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
            avctx->sample_fmt = aes_word_bits == 24 ? AV_SAMPLE_FMT_S32 : AV_SAMPLE_FMT_S16;
            s1->inited = 1;
        }
        frame->nb_samples = aes_samples;
        if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
            return ret;
        if (aes_word_bits == 16)
            memcpy(frame->extended_data[0], avpkt->data, aes_samples * (aes_word_bits >> 2));
        else {
            uint8_t *buf_in = avpkt->data;
            uint8_t *buf_out = frame->extended_data[0];
            for (; buf_in + 5 < avpkt->data + avpkt->size; buf_in+=6, buf_out+=8)
                AV_WL64(buf_out,
                     (uint64_t)AV_RL24(buf_in)   <<  8 |
                     (uint64_t)AV_RL24(buf_in+3) << 40 );
        }
        *got_frame = 1;
        return avpkt->size;
    }

    s1->prev_aes_samples = aes_samples;
    if (s1->inited) {
        frame->nb_samples = prev_aes_samples;
        if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
            return ret;
    }
    if (!avpkt->size) {
        ret = swr_convert(s1->swr,
                frame->extended_data, frame->nb_samples,
                NULL, 0);
        if (ret < 0)
            return ret;
        av_assert0(ret == frame->nb_samples);
        *got_frame = 1;
        s1->flushed = 1;
        return 0;
    }

    next = avpriv_s337m_parse_header(dectx, avpkt->data, avpkt->size, aes_word_bits);
    if (next < 0)
        return next;
    else if (!next) {
        av_assert0(!s1->inited);
        s1->aes_start_position += avpkt->size;
        return avpkt->size;
    }
    ret = set_codec(s1);
    if (ret < 0)
        return ret;

    ret = av_packet_make_writable(avpkt);
    if (ret < 0)
        return ret;
    avpkt->data += next;
    avpkt->size = dectx->frame_size;

    if (aes_word_bits == 16)
        avpriv_spdif_s337m_bswap_buf16((uint16_t *)avpkt->data, (uint16_t *)avpkt->data, avpkt->size >> 1);
    else
        avpriv_spdif_s337m_bswap_buf24(avpkt->data, avpkt->size);

    ret = avcodec_send_packet(s1->codec_avctx, avpkt);
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Error submitting a packet for decoding\n");
        return ret;
    }

    ret = avcodec_receive_frame(s1->codec_avctx, s1->codec_frame);
    if (ret < 0)
        return ret;

    if (!s1->inited) {
        ret = av_channel_layout_copy(&avctx->ch_layout, &s1->codec_avctx->ch_layout);
        if (ret < 0)
            return ret;
        avctx->sample_fmt = s1->codec_avctx->sample_fmt;
        s1->codec_initial_sample_rate = s1->codec_avctx->sample_rate;
        ret = init_resample(avctx, &s1->swr, s1->codec_initial_sample_rate);
        if (ret < 0)
            return ret;
        swr_next_pts(s1->swr, 0);
        s1->inited = 1;
        av_log(avctx, AV_LOG_VERBOSE,
                "s337m phase: %.6fs\n", s1->aes_start_position / (aes_word_bits >> 2) / (float)avctx->sample_rate);
        /* The small initial guard band must not be taken into account for syncing
         * but completely missing frames must be (in that case, the guard band length is unknown, so also included).
         */
        if (s1->aes_start_position >= dectx->frame_size) {
            s1->next_pts += s1->codec_initial_sample_rate * s1->aes_start_position / (aes_word_bits >> 2);
            swr_next_pts(s1->swr, s1->next_pts);
        }
        ret = swr_convert(s1->swr,
                NULL, 0,
                (const uint8_t**)s1->codec_frame->extended_data, s1->codec_frame->nb_samples);
        if (ret < 0)
            return ret;

        return avpkt->size;
    } else {
        if (av_channel_layout_compare(&avctx->ch_layout, &s1->codec_avctx->ch_layout)
            || avctx->sample_fmt != s1->codec_avctx->sample_fmt
            || s1->codec_initial_sample_rate != s1->codec_avctx->sample_rate)
            return AVERROR_INPUT_CHANGED;

        s1->next_pts += s1->codec_initial_sample_rate * prev_aes_samples;
        swr_next_pts(s1->swr, s1->next_pts);
        ret = swr_convert(s1->swr,
                frame->extended_data, frame->nb_samples,
                (const uint8_t**)s1->codec_frame->extended_data, s1->codec_frame->nb_samples);
        if (ret < 0)
            return ret;
        av_assert0(ret == frame->nb_samples);
    }

    *got_frame = 1;
    return avpkt->size;
}

static void s337m_flush(AVCodecContext *avctx)
{
    S337MDecodeContext *s = avctx->priv_data;
    avcodec_flush_buffers(s->codec_avctx);
}

static av_cold int s337m_close(AVCodecContext *avctx)
{
    S337MDecodeContext *s = avctx->priv_data;
    avcodec_free_context(&s->codec_avctx);
    swr_free(&s->swr);
    av_frame_free(&s->codec_frame);

    return 0;
}

#define PAR (AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_AUDIO_PARAM)
static const AVOption options[] = {
    {"passthrough", "Pass NON-PCM through unchanged", offsetof(S337MDecodeContext, passthrough), AV_OPT_TYPE_BOOL, {.i64 = 0 }, 0, 1, PAR },
    {NULL}
};

static const AVClass s337m_decoder_class = {
    .class_name = "s337m decoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const FFCodec ff_s337m_16_decoder = {
    .p.name         = "s337m_16",
    .p.long_name    = NULL_IF_CONFIG_SMALL("S337M 16-bit transport"),
    .p.type         = AVMEDIA_TYPE_AUDIO,
    .p.id           = AV_CODEC_ID_S337M_16,
    .init           = s337m_init,
    FF_CODEC_DECODE_CB(s337m_decode_frame),
    .close          = s337m_close,
    .flush          = s337m_flush,
    .priv_data_size = sizeof(S337MDecodeContext),
    .p.priv_class   = &s337m_decoder_class,
    .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_CHANNEL_CONF | AV_CODEC_CAP_DELAY,
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP,
};
const FFCodec ff_s337m_24_decoder = {
    .p.name         = "s337m_24",
    .p.long_name    = NULL_IF_CONFIG_SMALL("S337M 24-bit transport"),
    .p.type         = AVMEDIA_TYPE_AUDIO,
    .p.id           = AV_CODEC_ID_S337M_24,
    .init           = s337m_init,
    FF_CODEC_DECODE_CB(s337m_decode_frame),
    .close          = s337m_close,
    .flush          = s337m_flush,
    .priv_data_size = sizeof(S337MDecodeContext),
    .p.priv_class   = &s337m_decoder_class,
    .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_CHANNEL_CONF | AV_CODEC_CAP_DELAY,
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP,
};
