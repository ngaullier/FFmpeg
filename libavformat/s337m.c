/*
 * S337M demuxer
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

#include "libavutil/mem.h"
#include "libavcodec/codec_id.h"
#include "libavcodec/spdif_s337m_parser_internal.h"

#include "avformat.h"
#include "internal.h"
#include "rawdec.h"

#define AES_DEFAULT_RATE 48000
#define MAX_FRAME_RATE 30
#define PROBE_MIN_FRAMES 2

static int s337m_probe(const AVProbeData *p, int aes_word_bits)
{
    SPDIFS337MParseContext *pc1;
    int count_sync = 0, pos = 0;

    if (p->buf_size / (aes_word_bits >> 2) < PROBE_MIN_FRAMES * AES_DEFAULT_RATE / MAX_FRAME_RATE)
        return 0;
    pc1 = av_mallocz(sizeof(*pc1));
    if (!pc1)
        return AVERROR(ENOMEM);
    while(pos < p->buf_size) {
        int next;

        next = avpriv_spdif_s337m_find_syncword(pc1, p->buf + pos, p->buf_size - pos, aes_word_bits);
        if (next == END_NOT_FOUND)
            goto not_found;
        pos += next;
        next = avpriv_s337m_parse_header(NULL, p->buf + pos, p->buf_size - pos, aes_word_bits);
        if (next > 0 && ++count_sync >= PROBE_MIN_FRAMES) {
            av_free(pc1);
            return AVPROBE_SCORE_EXTENSION + 1;
        }
    }

not_found:
    av_free(pc1);
    return 0;
}

static int s337m_probe_16(const AVProbeData *p)
{
    return s337m_probe(p, 16);
}
static int s337m_probe_24(const AVProbeData *p)
{
    return s337m_probe(p, 24);
}

static int s337m_read_header(AVFormatContext *s)
{
    AVCodecParameters *par;
    AVStream *st;
    int ret = ff_raw_audio_read_header(s);

    if (ret < 0)
        return ret;
    st = s->streams[0];
    par = st->codecpar;
    par->sample_rate = AES_DEFAULT_RATE;
    par->bits_per_coded_sample = av_get_bits_per_sample(par->codec_id);

    avpriv_set_pts_info(st, 64, 1, par->sample_rate);
    return 0;
}

const FFInputFormat ff_s337m_16_demuxer = {
    .p.name         = "s337m_16",
    .p.long_name    = NULL_IF_CONFIG_SMALL("SMPTE 337M within 16-bit pcm"),
    .p.flags        = AVFMT_GENERIC_INDEX,
    .p.priv_class   = &ff_raw_demuxer_class,
    .read_probe     = s337m_probe_16,
    .read_header    = s337m_read_header,
    .read_packet    = ff_raw_read_partial_packet,
    .raw_codec_id   = AV_CODEC_ID_S337M_16,
    .priv_data_size = sizeof(FFRawDemuxerContext),
};
const FFInputFormat ff_s337m_24_demuxer = {
    .p.name         = "s337m_24",
    .p.long_name    = NULL_IF_CONFIG_SMALL("SMPTE 337M within 24-bit pcm"),
    .p.flags        = AVFMT_GENERIC_INDEX,
    .p.priv_class   = &ff_raw_demuxer_class,
    .read_probe     = s337m_probe_24,
    .read_header    = s337m_read_header,
    .read_packet    = ff_raw_read_partial_packet,
    .raw_codec_id   = AV_CODEC_ID_S337M_24,
    .priv_data_size = sizeof(FFRawDemuxerContext),
};
